#include "searcher.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <cstring>
#include <algorithm>

namespace searcher {

Result findAtlas(std::string filepattern, bool recurse,
                 std::vector<std::string> &atlas) {
  // TODO: implement
  (void)filepattern;
  (void)recurse;
  (void)atlas;
  return Result{false, ""};
}

Result findSprites(std::string filepattern, bool recurse,
                   std::vector<std::string> &sprites) {
  // TODO: implement
  (void)filepattern;
  (void)recurse;
  (void)sprites;
  return Result{false, ""};
}

Result formatOutput(OutputFormat outType, std::ostream &outPtr,
                    std::vector<AtlasMap> atlasMap) {
  // TODO: implement
  (void)outType;
  (void)outPtr;
  (void)atlasMap;
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
  // uses stb_image.h to load the png files
  // if crop set aggressively crop to the first fully transparent pixel
  // and then add back a 1px transparent border
  if (!std::filesystem::exists(filepath)) {
    return Image{};
  }

  int x, y, channels;
  uint32_t *data = (uint32_t *)(void *)stbi_load(filepath.c_str(), &x, &y, &channels, 4);
  if (!data) {
    return Image{};
  }

  Image img;
  img.path = filepath;

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
  std::vector<AtlasMap> atlasmap;
  std::vector<std::string> atlas;
  std::vector<std::string> sprites;

  findAtlas(config.atlaspath, config.recurse, atlas);
  findSprites(config.spritespath, config.recurse, sprites);

  std::vector<Image> imgSprites;
  std::vector<Image> imgAtlases;

  for (auto &sprite : sprites) {
    imgSprites.push_back(loadImage(sprite, true)); // sprites need cropping
  }

  for (auto &atl : atlas) {
    imgAtlases.push_back(loadImage(atl)); // dont crop atlases
  }

  return formatOutput(config.outputformat, *config.output_ptr, atlasmap);
}
} // namespace searcher
