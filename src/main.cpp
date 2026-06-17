#define ARGSPARSERRENEWED_IMPLEMENTATION
#include "ArgsParser.hpp"
#include "ISearchPlugin.hpp"
#include "json.hpp"
#include <dlfcn.h>
#include <filesystem>
#include <iostream>
#include <map>
#include <vector>

using json = nlohmann::json;

struct dlObjects {
  ISearchPlugin *plugin;
  void *handle;
};

void cleanup(std::map<std::string, dlObjects> &sharedObjects) {
  for (auto &pair : sharedObjects) {
    delete pair.second.plugin;
    dlclose(pair.second.handle);
  }
  sharedObjects.clear();
}

int main(int argc, char **argv) {

  std::map<std::string, std::optional<ArgValue>> results;
  ArgsParser parser;

  std::map<std::string, dlObjects> sharedObjects;

  struct {
    std::string spritespath;
    std::string atlaspath;
    bool recurse;
    std::string outputpath;
    std::string outputformat;
    std::string searchtype;
    bool quiet;
  } config;

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

  if (!parser.parse(argc, argv, results)) {
    parser.showHelp();
    cleanup(sharedObjects);
    return 1;
  }

  if (results["help"].has_value() && std::get<bool>(results["help"].value())) {
    parser.showHelp();
    cleanup(sharedObjects);
    return 0;
  }

  if (results["list-types"].has_value() &&
      std::get<bool>(results["list-types"].value())) {
    std::cout << "Available search plugins:" << std::endl;
    for (const auto &pair : sharedObjects)
      std::cout << pair.first << std::endl;
    cleanup(sharedObjects);
    return 0;
  }

  // check if user has specified either -c or -s and -a (if no -c)

  // build the config from json if -c specified else, use -s and -a (set
  // rotations to [0,90,180,270], as no way to specify from cli atm)

  // is no -t specified, use the plugin with the highest priority ()

  // if -o specified set the config output to - else default to stdout

  // if -q specified set the config quiet to true else default to false

  // call the search function with the config file.

  if (!results["config"].has_value() &&
      (!results["sprites"].has_value() || !results["atlases"].has_value())) {
    std::cerr << "Error: either -c <file> or -s <path> and -a <path> must be "
                 "specified."
              << std::endl;
    parser.showHelp();
    cleanup(sharedObjects);
    return 1;
  }

  cleanup(sharedObjects);
  return 0;
}