#define ARGSPARSERRENEWED_IMPLEMENTATION
#include "ArgsParser.hpp"
#include "ISearchPlugin.hpp"
#include <dlfcn.h>
#include <iostream>

int main(int argc, char **argv) {

  std::map<std::string, std::optional<ArgValue>> results;
  ArgsParser parser;

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

  if (!parser.parse(argc, argv, results)) {
    parser.showHelp();
    return 1;
  }

  if (results["help"].has_value() && std::get<bool>(results["help"].value())) {
    parser.showHelp();
    return 0;
  }

  return 0;
}