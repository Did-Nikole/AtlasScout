#define ARGSPARSERRENEWED_IMPLEMENTATION
#include "ArgsParser.hpp"
#include "ISearchPlugin.hpp"
#include "Logger.hpp"
#include "json.hpp"
#include "Config.hpp"
#include "searcher.h"
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>

using json = nlohmann::json;

bool loadConfig(std::string filename, Config &config,
                const std::map<std::string, dlObjects> &sharedObjects) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    std::cerr << "Error: Cannot open config file '" << filename << "'"
              << std::endl;
    return false;
  }

  json j;
  try {
    file >> j;
  } catch (const json::parse_error &e) {
    std::cerr << "Error: Failed to parse JSON config file: " << e.what()
              << std::endl;
    return false;
  }

  std::filesystem::path config_dir = std::filesystem::path(filename).parent_path();

  // Parse atlas_source
  if (j.contains("atlas_source") && j["atlas_source"].is_string()) {
    std::filesystem::path p = j["atlas_source"].get<std::string>();
    if (p.is_relative() && !config_dir.empty()) {
      config.atlaspath = (config_dir / p).string();
    } else {
      config.atlaspath = p.string();
    }
  }

  // Parse sprite_source
  if (j.contains("sprite_source") && j["sprite_source"].is_string()) {
    std::filesystem::path p = j["sprite_source"].get<std::string>();
    if (p.is_relative() && !config_dir.empty()) {
      config.spritespath = (config_dir / p).string();
    } else {
      config.spritespath = p.string();
    }
  }

  // Parse recursive
  if (j.contains("recursive") && j["recursive"].is_boolean()) {
    config.recurse = j["recursive"].get<bool>();
  }

  // Parse quiet
  if (j.contains("quiet") && j["quiet"].is_boolean()) {
    config.quiet = j["quiet"].get<bool>();
  }

  // Parse output_format
  if (j.contains("output_format") && j["output_format"].is_string()) {
    std::string format = j["output_format"].get<std::string>();
    if (format == "json") {
      config.outputformat = OutputFormat::JSON;
    } else if (format == "tsv") {
      config.outputformat = OutputFormat::TSV;
    } else if (format == "csv") {
      config.outputformat = OutputFormat::CSV;
    } else if (format == "xml") {
      config.outputformat = OutputFormat::XML;
    } else {
      std::cerr << "Error: Invalid output format in config: " << format
                << std::endl;
      return false;
    }
  }

  // Parse output_file
  if (j.contains("output_file") && j["output_file"].is_string()) {
    std::string outstring = j["output_file"].get<std::string>();
    if (!outstring.empty() && outstring != "stdout" && outstring != "-") {
      std::filesystem::path p = outstring + "." + config.outputformat.suffix();
      if (p.is_relative() && !config_dir.empty()) {
        p = config_dir / p;
      }
      auto *outfile = new std::ofstream(p);
      if (!outfile->is_open()) {
        std::cerr << "Error: Cannot open output file '" << p.string()
                  << "': " << std::strerror(errno) << std::endl;
        delete outfile;
        return false;
      }
      config.output_ptr = outfile;
    }
  }

  // Parse rotations
  if (j.contains("rotations") && j["rotations"].is_array()) {
    config.rotations.clear();
    for (const auto &item : j["rotations"]) {
      if (item.is_number_integer()) {
        config.rotations.push_back(item.get<int>());
      }
    }
  }

  // Parse search_type
  if (j.contains("search_type") && j["search_type"].is_string()) {
    std::string type = j["search_type"].get<std::string>();
    if (sharedObjects.contains(type)) {
      config.searchMethod = sharedObjects.at(type).plugin;
    } else {
      std::cerr << "Error: Invalid search type in config: " << type
                << std::endl;
      return false;
    }
  } else {
    // Select highest priority plugin
    if (sharedObjects.empty()) {
      std::cerr << "Error: No search types available." << std::endl;
      return false;
    } else {
      auto highestIt = sharedObjects.begin();
      for (auto it = sharedObjects.begin(); it != sharedObjects.end(); ++it) {
        if (it->second.plugin->priority() >
            highestIt->second.plugin->priority()) {
          highestIt = it;
        }
      }
      config.searchMethod = highestIt->second.plugin;
    }
  }

  return true;
}

void cleanup(std::map<std::string, dlObjects> &sharedObjects, Config config) {
  if (config.output_ptr != &std::cout) {
    delete config.output_ptr;
  }
  for (auto &pair : sharedObjects) {
    delete pair.second.plugin;
    dlclose(pair.second.handle);
  }
  sharedObjects.clear();
}

int main(int argc, char **argv) {

  Logger &log = Logger::getInstance();
  log.init("debug.log", true);

  log.log("Starting atlasscout");

  std::map<std::string, std::optional<ArgValue>> results;
  ArgsParser parser;

  std::map<std::string, dlObjects> sharedObjects;

  Config config;

  // Iterate over the plugins directory and check each plugin for availability
  // using the is_supported method and then load it into the sharedObjects map

  if (std::filesystem::exists("./plugins") &&
      std::filesystem::is_directory("./plugins")) {
    for (const auto &entry : std::filesystem::directory_iterator("./plugins")) {
      // filter for '.so' files
      if (entry.path().extension() != ".so") {
        continue;
      }

      // attempt to dlopen it
      void *handle = dlopen(entry.path().c_str(), RTLD_LAZY);
      if (!handle) {
        continue;
      }

      // get the create_plugin function
      CreatePluginFunc create_func =
          (CreatePluginFunc)dlsym(handle, "create_plugin");
      if (!create_func) {
        dlclose(handle);
        continue;
      }

      // check if is_supported is true
      ISearchPlugin *plugin = create_func();
      if (!plugin->is_supported()) {
        delete plugin;
        dlclose(handle);
        continue;
      }

      // if yes, add to sharedObjects map
      sharedObjects[plugin->get_name()] = {plugin, handle};
    }
  }

  /* clang-format off */
  /*
   *  | Flag | Long Form | Description |
   *  | :--- | :--- | :--- |
   *  | `-c` | `--config <file>` | Path to a JSON configuration file. Overrides individual CLI flags. |
   *  | `-s` | `--sprites <path>` | Path, directory, or pattern for the source sprites to search for. |
   *  | `-a` | `--atlases <path>` | Path, directory, or pattern for the atlases (spritesheets) to scan. |
   *  | `-r` | `--recurse` | Recursively search subdirectories for both source sprites and atlases. |
   *  | `-o` | `--output <file>` | Output file path. If omitted or set to `-`, defaults to `stdout`. |
   *  | `-f` | `--format <type>` | Built-in output format. Options: `json` (default), `tsv`, `csv`, `xml`. |
   *  | `-t` | `--type <name>` | Force a specific search `.so` plugin implementation (e.g., `avx2`). |
   *  | `-l` | `--list-types` | List available search types (dynamically loaded from plugins) and exit. |
   *  | `-q` | `--quiet` | Suppress all progress/status messages (silences `stderr`). Ideal for piping. |
   *  | `-h` | `--help` | Print usage information and exit. |
   */
  /* clang-format on */

  parser.addArgument({.type = ArgType::BOOLEAN,
                      .shortFlag = "-h",
                      .longFlag = "--help",
                      .description = "Display help message",
                      .required = false,
                      .defaultValue = false});

  parser.addArgument({.type = ArgType::BOOLEAN,
                      .shortFlag = "-l",
                      .longFlag = "--list-types",
                      .description = "List available search types (dynamically "
                                     "loaded from plugins) and exit.",
                      .required = false,
                      .defaultValue = false});

  parser.addArgument({.type = ArgType::STRING,
                      .shortFlag = "-c",
                      .longFlag = "--config",
                      .description =
                          "Path to a JSON configuration file. Overrides "
                          "individual CLI flags.",
                      .required = false,
                      .defaultValue = ""});

  parser.addArgument({.type = ArgType::STRING,
                      .shortFlag = "-s",
                      .longFlag = "--sprites",
                      .description =
                          "Path, directory, or pattern for the source "
                          "sprites to search for.",
                      .required = false,
                      .defaultValue = ""});

  parser.addArgument({.type = ArgType::STRING,
                      .shortFlag = "-a",
                      .longFlag = "--atlases",
                      .description =
                          "Path, directory, or pattern for the atlases "
                          "(spritesheets) to scan.",
                      .required = false,
                      .defaultValue = ""});

  parser.addArgument({.type = ArgType::BOOLEAN,
                      .shortFlag = "-r",
                      .longFlag = "--recurse",
                      .description =
                          "Recursively search subdirectories for both "
                          "source sprites and atlases.",
                      .required = false,
                      .defaultValue = false});

  parser.addArgument({.type = ArgType::STRING,
                      .shortFlag = "-o",
                      .longFlag = "--output",
                      .description = "Output file path. If omitted or set to "
                                     "`-`, defaults to `stdout`.",
                      .required = false,
                      .defaultValue = ""});

  parser.addArgument({.type = ArgType::STRING,
                      .shortFlag = "-f",
                      .longFlag = "--format",
                      .description = "Built-in output format. Options: `json` "
                                     "(default), `tsv`, `csv`, `xml`.",
                      .required = false,
                      .defaultValue = ""});

  parser.addArgument({.type = ArgType::STRING,
                      .shortFlag = "-t",
                      .longFlag = "--type",
                      .description = "Force a specific search `.so` plugin "
                                     "implementation (e.g., `avx2`).",
                      .required = false,
                      .defaultValue = ""});

  parser.addArgument({.type = ArgType::BOOLEAN,
                      .shortFlag = "-q",
                      .longFlag = "--quiet",
                      .description = "Suppress all progress/status messages "
                                     "(silences `stderr`). Ideal for piping.",
                      .required = false,
                      .defaultValue = false});

  if (!parser.parse(argc, argv, results)) {
    parser.showHelp();
    cleanup(sharedObjects, config);
    return 1;
  }

  if (results["help"].has_value() && std::get<bool>(results["help"].value())) {
    parser.showHelp();
    cleanup(sharedObjects, config);
    return 0;
  }

  if (results["list-types"].has_value() &&
      std::get<bool>(results["list-types"].value())) {
    std::cout << "Available search plugins:" << std::endl;
    for (const auto &pair : sharedObjects)
      std::cout << pair.first << std::endl;
    cleanup(sharedObjects, config);
    return 0;
  }

  // check if user has specified either -c or -s and -a (if no -c)
  if (results["config"].has_value() &&
      !std::get<std::string>(results["config"].value()).empty()) {
    if (!loadConfig(std::get<std::string>(results["config"].value()), config,
                    sharedObjects)) {
      std::cerr << "Error: Could not load config file: "
                << "\n";
      parser.showHelp();
      cleanup(sharedObjects, config);
      return 1;
    }
  } else if (results["atlases"].has_value() &&
             !std::get<std::string>(results["atlases"].value()).empty() &&
             results["sprites"].has_value() &&
             !std::get<std::string>(results["sprites"].value()).empty()) {
    // process all flags only if config not set, except -h and -l which are
    // handled above
    config.atlaspath = std::get<std::string>(results["atlases"].value());
    config.spritespath = std::get<std::string>(results["sprites"].value());
    if (results.contains("recurse"))
      config.recurse = std::get<bool>(results["recurse"].value());
    else
      config.recurse = false;
    if (results["format"].has_value()) {
      auto format = std::get<std::string>(results["format"].value());
      if (format == "json")
        config.outputformat = OutputFormat::JSON;
      else if (format == "tsv")
        config.outputformat = OutputFormat::TSV;
      else if (format == "csv")
        config.outputformat = OutputFormat::CSV;
      else if (format == "xml")
        config.outputformat = OutputFormat::XML;
      else {
        std::cerr << "Error: Invalid format: " << format << std::endl;
        cleanup(sharedObjects, config);
        return 1;
      }
      if (results.contains("output")) {
        auto outstring = std::get<std::string>(results.at("output").value());
        if (outstring != "stdout") {

          auto *outfile =
              new std::ofstream(outstring + "." + config.outputformat.suffix());
          if (!outfile->is_open()) {
            std::cerr << "Error: Cannot open output file '" << outstring
                      << "': " << std::strerror(errno) << std::endl;
            delete outfile;
            cleanup(sharedObjects, config);
            return 1;
          }
          config.output_ptr = outfile;
        }
      }

    } else {
      config.outputformat = OutputFormat::JSON;
    }

    if (results["type"].has_value()) {
      auto type = std::get<std::string>(results["type"].value());
      if (sharedObjects.contains(type)) {
        config.searchMethod = sharedObjects[type].plugin;
      } else {
        std::cerr << "Error: Invalid search type: " << type << std::endl;
        cleanup(sharedObjects, config);
        return 1;
      }
    } else {
      if (sharedObjects.empty()) {
        std::cerr << "Error: No search types available, check plugins folder "
                     "is in samefolder as app and contains valid plugins."
                  << std::endl;
        cleanup(sharedObjects, config);
        return 1;
      } else {
        auto highestIt = sharedObjects.begin();
        for (auto it = sharedObjects.begin(); it != sharedObjects.end(); ++it) {
          if (it->second.plugin->priority() >
              highestIt->second.plugin->priority()) {
            highestIt = it;
          }
        }
        config.searchMethod = highestIt->second.plugin;
      }
    }
    if (results["quiet"].has_value()) {
      config.quiet = std::get<bool>(results["quiet"].value());
    } else {
      config.quiet = false;
    }

  } else {
    std::cerr << "Error: either -c <file> or -s <path> and -a <path> must be "
                 "specified."
              << std::endl;
    parser.showHelp();
    cleanup(sharedObjects, config);
    return 1;
  }

  // dump the config at this point just for sanity, might turn it off later
  log.log("Config :" + config.to_string());
  if (!config.quiet) {
    std::cout << "Config :" + config.to_string() << std::endl;
  }

  auto res = searcher::do_search(config);

  if (res.isError) {
    log.log(res.message);
    if (!config.quiet) {
      std::cerr << res.message << std::endl;
    }
    cleanup(sharedObjects, config);
    return 1;
  }

  // call the search function with the config file.

  cleanup(sharedObjects, config);
  return 0;
}