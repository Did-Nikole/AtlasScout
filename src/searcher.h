/*
 * Copyright (C) 2026 Nikole Smith (ApptsolutioNZ - appsolutionz.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once
#include "Config.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <functional>

namespace searcher {
struct Result {
  bool isError = false;
  std::string message;
};

struct Image {
  std::string path;
  int w;
  int h;
  int rot;
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
                    std::vector<AtlasMap> atlasMap, std::string pluginName);

Image loadImage(std::string filepath, bool crop = false);

Result do_search(Config config,
                 std::function<void(Config, const char*)> msgCallback = nullptr,
                 std::function<void(Config, float, int)> progressCallback = nullptr);
} // namespace searcher