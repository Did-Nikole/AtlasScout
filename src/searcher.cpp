#include "searcher.h"
#include <map>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <fnmatch.h>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <atomic>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <fstream>
#include <set>
#include "json.hpp"
#include "Logger.hpp"

namespace searcher {

struct Bounds {
  int x1, y1, x2, y2;
};

namespace {
class ThreadPool {
public:
  ThreadPool(size_t threads, const std::vector<int>& physical_cpus) : stop(false) {
    for (size_t i = 0; i < threads; ++i) {
      workers.emplace_back([this, i, physical_cpus]() {
        if (i < physical_cpus.size()) {
          int target_cpu = physical_cpus[i];
          cpu_set_t cpuset;
          CPU_ZERO(&cpuset);
          CPU_SET(static_cast<size_t>(target_cpu), &cpuset);
          pthread_t thread = pthread_self();
          pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
        }

        for (;;) {
          std::function<void()> task;
          {
            std::unique_lock<std::mutex> lock(this->queue_mutex);
            this->condition.wait(lock, [this]() { return this->stop || !this->tasks.empty(); });
            if (this->stop && this->tasks.empty())
              return;
            task = std::move(this->tasks.front());
            this->tasks.pop();
          }
          task();
        }
      });
    }
  }

  template<class F, class... Args>
  auto enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::invoke_result<F, Args...>::type> {
    using return_type = typename std::invoke_result<F, Args...>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
      std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
        
    std::future<return_type> res = task->get_future();
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      if (stop)
        throw std::runtime_error("enqueue on stopped ThreadPool");
      tasks.emplace([task]() { (*task)(); });
    }
    condition.notify_one();
    return res;
  }

  ~ThreadPool() {
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      stop = true;
    }
    condition.notify_all();
    for (std::thread &worker: workers) {
      if (worker.joinable()) {
        worker.join();
      }
    }
  }

private:
  std::vector<std::thread> workers;
  std::queue<std::function<void()>> tasks;
  std::mutex queue_mutex;
  std::condition_variable condition;
  bool stop;
};

static std::vector<int> get_physical_core_cpus() {
  std::vector<int> core_cpus;
  long num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
  if (num_cpus <= 0) {
    num_cpus = 1;
  }

  std::map<int, std::vector<int>> core_to_cpus;

  for (int i = 0; i < num_cpus; ++i) {
    std::string path =
        "/sys/devices/system/cpu/cpu" + std::to_string(i) + "/topology/core_id";
    std::ifstream file(path);
    if (file.is_open()) {
      int core_id;
      if (file >> core_id) {
        core_to_cpus[core_id].push_back(i);
      }
    } else {
      core_to_cpus[i].push_back(i);
    }
  }

  for (const auto &pair : core_to_cpus) {
    if (!pair.second.empty()) {
      core_cpus.push_back(pair.second[0]);
    }
  }

  return core_cpus;
}

Result findFiles(std::string filepattern, bool recurse, std::vector<std::string> &files) {
  size_t last_slash = filepattern.find_last_of("/\\");
  std::string dir_path = ".";
  std::string pattern = filepattern;
  if (last_slash != std::string::npos) {
    dir_path = filepattern.substr(0, last_slash);
    pattern = filepattern.substr(last_slash + 1);
  }

  if (!std::filesystem::exists(dir_path) || !std::filesystem::is_directory(dir_path)) {
    return Result{true, "Directory does not exist: " + dir_path};
  }

  auto scan_dir = [&](const std::filesystem::path& p) {
    if (recurse) {
      for (const auto& entry : std::filesystem::recursive_directory_iterator(p)) {
        if (entry.is_regular_file()) {
          std::string filename = entry.path().filename().string();
          if (fnmatch(pattern.c_str(), filename.c_str(), 0) == 0) {
            files.push_back(entry.path().string());
          }
        }
      }
    } else {
      for (const auto& entry : std::filesystem::directory_iterator(p)) {
        if (entry.is_regular_file()) {
          std::string filename = entry.path().filename().string();
          if (fnmatch(pattern.c_str(), filename.c_str(), 0) == 0) {
            files.push_back(entry.path().string());
          }
        }
      }
    }
  };

  try {
    scan_dir(dir_path);
  } catch (const std::exception& e) {
    return Result{true, std::string("Error scanning directory: ") + e.what()};
  }

  return Result{false, ""};
}

Image rotateImage(const Image &img, int degrees) {
  if (degrees == 0) {
    Image copy_img = img;
    copy_img.data = new uint32_t[static_cast<size_t>(img.w * img.h)];
    std::memcpy(copy_img.data, img.data, static_cast<size_t>(img.w * img.h) * sizeof(uint32_t));
    return copy_img;
  }

  Image rot_img;
  rot_img.path = img.path;
  rot_img.rot = degrees;

  if (degrees == 90) {
    rot_img.w = img.h;
    rot_img.h = img.w;
    rot_img.data = new uint32_t[static_cast<size_t>(rot_img.w * rot_img.h)];
    for (int y = 0; y < img.h; ++y) {
      for (int x = 0; x < img.w; ++x) {
        rot_img.data[x * rot_img.w + (rot_img.w - 1 - y)] = img.data[y * img.w + x];
      }
    }
  } else if (degrees == 180) {
    rot_img.w = img.w;
    rot_img.h = img.h;
    rot_img.data = new uint32_t[static_cast<size_t>(rot_img.w * rot_img.h)];
    for (int y = 0; y < img.h; ++y) {
      for (int x = 0; x < img.w; ++x) {
        rot_img.data[(img.h - 1 - y) * rot_img.w + (img.w - 1 - x)] = img.data[y * img.w + x];
      }
    }
  } else if (degrees == 270) {
    rot_img.w = img.h;
    rot_img.h = img.w;
    rot_img.data = new uint32_t[static_cast<size_t>(rot_img.w * rot_img.h)];
    for (int y = 0; y < img.h; ++y) {
      for (int x = 0; x < img.w; ++x) {
        rot_img.data[(img.w - 1 - x) * rot_img.w + y] = img.data[y * img.w + x];
      }
    }
  } else {
    Image copy_img = img;
    copy_img.data = new uint32_t[static_cast<size_t>(img.w * img.h)];
    std::memcpy(copy_img.data, img.data, static_cast<size_t>(img.w * img.h) * sizeof(uint32_t));
    return copy_img;
  }

  return rot_img;
}
} // namespace

Result findAtlas(std::string filepattern, bool recurse,
                 std::vector<std::string> &atlas) {
  return findFiles(filepattern, recurse, atlas);
}

Result findSprites(std::string filepattern, bool recurse,
                   std::vector<std::string> &sprites) {
  return findFiles(filepattern, recurse, sprites);
}

Result formatOutput(OutputFormat outType, std::ostream &outPtr,
                    std::vector<AtlasMap> atlasMap, std::string pluginName) {
  if (outType == OutputFormat::JSON) {
    nlohmann::json root = nlohmann::json::object();
    nlohmann::json matches = nlohmann::json::array();
    for (const auto &m : atlasMap) {
      nlohmann::json match = nlohmann::json::object();
      match["sprite_name"] = std::filesystem::path(m.spriteName).stem().string();
      match["sprite_path"] = m.spriteName;
      match["atlas_name"] = std::filesystem::path(m.atlasName).stem().string();
      
      nlohmann::json bounds = nlohmann::json::object();
      bounds["x"] = m.x + 1;
      bounds["y"] = m.y + 1;
      bounds["width"] = m.w - 2;
      bounds["height"] = m.h - 2;
      match["bounds"] = bounds;
      
      match["rotated"] = (m.rot != 0);
      matches.push_back(match);
    }
    root["matches"] = matches;
    
    nlohmann::json metadata = nlohmann::json::object();
    metadata["total_found"] = atlasMap.size();
    metadata["search_plugin_used"] = pluginName;
    root["metadata"] = metadata;
    
    outPtr << root.dump(2) << std::endl;
  }
  else if (outType == OutputFormat::TSV) {
    outPtr << "# Output format:\n";
    outPtr << "# SPRITE_NAME\tSPRITE_PATH\tATLAS_NAME\tX\tY\tW\tH\tROTATED\n";
    for (const auto &m : atlasMap) {
      std::string sprite_name = std::filesystem::path(m.spriteName).stem().string();
      std::string atlas_name = std::filesystem::path(m.atlasName).stem().string();
      outPtr << sprite_name << "\t"
             << m.spriteName << "\t"
             << atlas_name << "\t"
             << (m.x + 1) << "\t"
             << (m.y + 1) << "\t"
             << (m.w - 2) << "\t"
             << (m.h - 2) << "\t"
             << (m.rot != 0 ? 1 : 0) << "\n";
    }
  }
  else if (outType == OutputFormat::CSV) {
    outPtr << "SPRITE_NAME,SPRITE_PATH,ATLAS_NAME,X,Y,W,H,ROTATED\n";
    for (const auto &m : atlasMap) {
      std::string sprite_name = std::filesystem::path(m.spriteName).stem().string();
      std::string atlas_name = std::filesystem::path(m.atlasName).stem().string();
      outPtr << "\"" << sprite_name << "\",\""
             << m.spriteName << "\",\""
             << atlas_name << "\","
             << (m.x + 1) << ","
             << (m.y + 1) << ","
             << (m.w - 2) << ","
             << (m.h - 2) << ","
             << (m.rot != 0 ? 1 : 0) << "\n";
    }
  }
  else if (outType == OutputFormat::XML) {
    outPtr << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    outPtr << "<matches>\n";
    for (const auto &m : atlasMap) {
      std::string sprite_name = std::filesystem::path(m.spriteName).stem().string();
      std::string atlas_name = std::filesystem::path(m.atlasName).stem().string();
      outPtr << "  <match>\n";
      outPtr << "    <sprite_name>" << sprite_name << "</sprite_name>\n";
      outPtr << "    <sprite_path>" << m.spriteName << "</sprite_path>\n";
      outPtr << "    <atlas_name>" << atlas_name << "</atlas_name>\n";
      outPtr << "    <x>" << (m.x + 1) << "</x>\n";
      outPtr << "    <y>" << (m.y + 1) << "</y>\n";
      outPtr << "    <width>" << (m.w - 2) << "</width>\n";
      outPtr << "    <height>" << (m.h - 2) << "</height>\n";
      outPtr << "    <rotated>" << (m.rot != 0 ? 1 : 0) << "</rotated>\n";
      outPtr << "  </match>\n";
    }
    outPtr << "</matches>\n";
  }

  return Result{false, ""};
}

static uint32_t *cropImage(const uint32_t *data, int width, int height, int &out_w, int &out_h) {
  int min_tx = width, max_tx = -1;
  int min_ty = height, max_ty = -1;

  for (int ty = 0; ty < height; ++ty) {
    for (int tx = 0; tx < width; ++tx) {
      uint32_t pixel = data[ty * width + tx];
      unsigned char alpha = (unsigned char)((pixel >> 24) & 0xFF);
      if (alpha > 0) {
        if (tx < min_tx) min_tx = tx;
        if (tx > max_tx) max_tx = tx;
        if (ty < min_ty) min_ty = ty;
        if (ty > max_ty) max_ty = ty;
      }
    }
  }

  if (max_tx == -1) {
    min_tx = 0; max_tx = 0;
    min_ty = 0; max_ty = 0;
  }

  int cropped_w = max_tx - min_tx + 1;
  int cropped_h = max_ty - min_ty + 1;

  out_w = cropped_w + 2;
  out_h = cropped_h + 2;

  uint32_t *cropped_data = new uint32_t[static_cast<size_t>(out_w * out_h)];
  std::fill_n(cropped_data, out_w * out_h, 0x00000000);

  for (int cy = 0; cy < cropped_h; ++cy) {
    int sy = min_ty + cy;
    for (int cx = 0; cx < cropped_w; ++cx) {
      int sx = min_tx + cx;
      cropped_data[(cy + 1) * out_w + (cx + 1)] = data[sy * width + sx];
    }
  }

  return cropped_data;
}

Image loadImage(std::string filepath, bool crop) {
  if (!std::filesystem::exists(filepath)) {
    return Image{};
  }

  int x, y, channels;
  uint32_t *data = (uint32_t *)(void *)stbi_load(filepath.c_str(), &x, &y, &channels, 4);
  if (!data) {
    return Image{};
  }

  // Zero out transparent pixels (alpha == 0) to ensure uniform matching in transparent areas
  for (int i = 0; i < x * y; ++i) {
    uint32_t pixel = data[i];
    unsigned char alpha = (unsigned char)((pixel >> 24) & 0xFF);
    if (alpha == 0) {
      data[i] = 0x00000000;
    }
  }

  Image img;
  img.path = filepath;
  img.rot = 0;
  img.w = 0;
  img.h = 0;
  img.data = nullptr;

  if (crop) {
    int cropped_w, cropped_h;
    img.data = cropImage(data, x, y, cropped_w, cropped_h);
    img.w = cropped_w;
    img.h = cropped_h;
    stbi_image_free(data);
  } else {
    uint32_t *copied_data = new uint32_t[static_cast<size_t>(x * y)];
    std::memcpy(copied_data, data, static_cast<size_t>(x * y) * sizeof(uint32_t));
    stbi_image_free(data);

    img.data = copied_data;
    img.w = x;
    img.h = y;
  }

  return img;
}

Result do_search(Config config) {
  Logger &logger = Logger::getInstance();
  
  std::vector<AtlasMap> atlasmap;
  std::vector<std::string> atlas;
  std::vector<std::string> sprites;
  std::map<std::string, std::vector<Bounds>> bounds_map;

  if (!config.quiet) {
    std::cout << "Scanning for atlas and sprite files..." << std::endl;
  }
  logger.log("Scanning directories...");

  Result atlas_res = findAtlas(config.atlaspath, config.recurse, atlas);
  if (atlas_res.isError) {
    logger.log("Error finding atlas: " + atlas_res.message);
    return atlas_res;
  }

  Result sprite_res = findSprites(config.spritespath, config.recurse, sprites);
  if (sprite_res.isError) {
    logger.log("Error finding sprites: " + sprite_res.message);
    return sprite_res;
  }

  if (!config.quiet) {
    std::cout << "Found " << atlas.size() << " atlas candidate(s) and " 
              << sprites.size() << " sprite candidate(s)." << std::endl;
  }
  logger.log("Found " + std::to_string(atlas.size()) + " atlases and " + std::to_string(sprites.size()) + " sprites.");

  std::vector<Image> imgSprites;
  std::vector<Image> imgAtlases;

  if (!config.quiet) {
    std::cout << "Loading images..." << std::endl;
  }
  logger.log("Loading images...");

  for (const auto &sprite : sprites) {
    Image base_sprite = loadImage(sprite, true);
    if (base_sprite.data == nullptr) {
      logger.log("Failed to load sprite: " + sprite);
      continue;
    }

    for (int deg : config.rotations) {
      imgSprites.push_back(rotateImage(base_sprite, deg));
    }
    delete[] base_sprite.data;
  }

  for (const auto &atl : atlas) {
    Image atlas_img = loadImage(atl, false);
    if (atlas_img.data != nullptr) {
      imgAtlases.push_back(atlas_img);
    } else {
      logger.log("Failed to load atlas: " + atl);
    }
  }

  if (!config.quiet) {
    std::cout << "Loaded " << imgSprites.size() << " sprite configurations and " 
              << imgAtlases.size() << " atlas image(s)." << std::endl;
  }
  logger.log("Loaded " + std::to_string(imgSprites.size()) + " sprites (with rotations) and " + std::to_string(imgAtlases.size()) + " atlases.");

  if (!config.searchMethod) {
    for (auto &img : imgSprites) delete[] img.data;
    for (auto &img : imgAtlases) delete[] img.data;
    logger.log("Error: No search plugin selected.");
    return Result{true, "Error: No search plugin selected."};
  }

  if (!config.quiet) {
    std::cout << "Starting image search using plugin: " << config.searchMethod->get_name() << std::endl;
  }
  logger.log("Starting match search...");

  {
    std::vector<int> physical_cpus = get_physical_core_cpus();
    ThreadPool pool(physical_cpus.size(), physical_cpus);
    std::vector<std::future<void>> futures;
    std::mutex results_mutex;
    std::mutex bounds_mutex;
    std::mutex log_mutex;

    std::set<std::string> matched_sprite_paths;
    size_t total_tasks = imgSprites.size();
    std::atomic<size_t> completed_tasks{0};

    for (const auto &imgSprite : imgSprites) {
      futures.push_back(pool.enqueue([&imgSprite, &imgAtlases, &atlasmap, &bounds_map, &results_mutex, &bounds_mutex, &log_mutex, &matched_sprite_paths, &completed_tasks, total_tasks, &config, &logger]() {
        // Early exit if this sprite has already been matched by another rotation/thread
        {
          std::lock_guard<std::mutex> lock(results_mutex);
          if (matched_sprite_paths.contains(imgSprite.path)) {
            size_t done = ++completed_tasks;
            std::lock_guard<std::mutex> log_lock(log_mutex);
            if (done % 50 == 0 || done == total_tasks) {
              std::string progress_msg = "Progress: " + std::to_string(done) + "/" + std::to_string(total_tasks) + " sprite configurations processed.";
              logger.log(progress_msg);
              if (!config.quiet) {
                std::cout << progress_msg << "\r" << std::flush;
              }
            }
            return;
          }
        }

        bool matched = false;
        for (const auto &imgAtlas : imgAtlases) {
          if (matched) break;

          int max_y = imgAtlas.h - imgSprite.h + 1;
          int max_x = imgAtlas.w - imgSprite.w + 1;

          for (int y = 0; y < max_y; y++) {
            if (matched) break;

            // Periodically check if another thread/rotation already matched this sprite
            {
              std::lock_guard<std::mutex> lock(results_mutex);
              if (matched_sprite_paths.contains(imgSprite.path)) {
                matched = true;
                break;
              }
            }

            std::vector<Bounds> local_bounds;
            {
              std::lock_guard<std::mutex> lock(bounds_mutex);
              if (bounds_map.contains(imgAtlas.path)) {
                local_bounds = bounds_map[imgAtlas.path];
              }
            }

            for (int x = 0; x < max_x; x++) {

              bool inside = false;
              for (const auto &b : local_bounds) {
                if (x >= b.x1 && x <= b.x2 && y >= b.y1 && y <= b.y2) {
                  inside = true;
                  break;
                }
              }
              if (inside) continue;

              if (config.searchMethod->bitwise_xor_match(
                      imgAtlas.data, imgAtlas.w, imgSprite.data, imgSprite.w,
                      imgSprite.h, x, y)) {
                
                {
                  std::lock_guard<std::mutex> lock(results_mutex);
                  // Double check match condition under lock
                  if (matched_sprite_paths.contains(imgSprite.path)) {
                    matched = true;
                    break;
                  }
                  matched_sprite_paths.insert(imgSprite.path);
                  atlasmap.push_back({imgSprite.path, imgAtlas.path, x, y,
                                      imgSprite.w, imgSprite.h, imgSprite.rot});
                }

                {
                  std::lock_guard<std::mutex> lock(bounds_mutex);
                  bounds_map[imgAtlas.path].push_back({x, y, x + imgSprite.w - 1, y + imgSprite.h - 1});
                }

                {
                  std::lock_guard<std::mutex> lock(log_mutex);
                  std::string match_msg = "Match found: " + imgSprite.path + " in " + imgAtlas.path + " at (" + std::to_string(x) + "," + std::to_string(y) + ")";
                  logger.log(match_msg);
                  if (!config.quiet) {
                    std::cout << "\n" << match_msg << std::endl;
                  }
                }

                matched = true;
                break;
              }
            }
          }
        }

        size_t done = ++completed_tasks;
        {
          std::lock_guard<std::mutex> lock(log_mutex);
          if (done % 50 == 0 || done == total_tasks) {
            std::string progress_msg = "Progress: " + std::to_string(done) + "/" + std::to_string(total_tasks) + " sprite configurations processed.";
            logger.log(progress_msg);
            if (!config.quiet) {
              std::cout << progress_msg << "\r" << std::flush;
            }
          }
        }
      }));
    }

    for (auto &f : futures) {
      f.get();
    }
  }

  if (!config.quiet) {
    std::cout << "\nSearch completed. Found " << atlasmap.size() << " match(es) total." << std::endl;
  }
  logger.log("Search finished. Total matches: " + std::to_string(atlasmap.size()));

  for (auto &img : imgSprites) {
    delete[] img.data;
  }
  for (auto &img : imgAtlases) {
    delete[] img.data;
  }

  std::string pluginName = config.searchMethod->get_name();
  return formatOutput(config.outputformat, *config.output_ptr, atlasmap, pluginName);
}
} // namespace searcher
