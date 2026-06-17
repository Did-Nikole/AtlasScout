#pragma once
#include "Config.hpp"
#include <vector>
#include <string>
#include <iostream>

namespace searcher {
struct Result {
  bool isError = false;
  std::string message;
};

struct Image {
  std::string path;
  int w;
  int h;
  uint32_t *data;
};

struct AtlasMap {
  std::string spriteName;
  std::string atlasName;
  int x;
  int y;
  int w;
  int h;
  int rot;
};

Result findAtlas(std::string filepattern, bool recurse,
                 std::vector<std::string> &atlas);

Result findSprites(std::string filepattern, bool recurse,
                   std::vector<std::string> &sprites);

Result formatOutput(OutputFormat outType, std::ostream &outPtr,
                    std::vector<AtlasMap> atlasMap);

Image loadImage(std::string filepath, bool crop = false);

Result do_search(Config config);
} // namespace searcher