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
#include <atomic>
#include <bits/c++config.h>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

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

struct SpriteTask {
  std::string path;
  std::string name;
  int base_width;
  int base_height;
  std::vector<Image> rotations;
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

struct DynamicBitmask {
  int _width = 0;
  int _height = 0;
  std::unique_ptr<std::atomic<uint64_t>[]> _grid = nullptr;

  DynamicBitmask() = default;
  DynamicBitmask(int w, int h) : _width(w), _height(h) {
    size_t total_pixels = static_cast<size_t>(w * h);
    size_t total_words = (total_pixels + 63) / 64;
    _grid = std::make_unique<std::atomic<uint64_t>[]>(total_words);
    for (size_t i = 0; i < total_words; ++i)
      _grid[i].store(0, std::memory_order_relaxed);
  }

  inline bool is_skipped(int x, int y) const {
    if (!_grid) return false;
    size_t pixel_idx = static_cast<size_t>((y * _width) + x);
    size_t word_idx = pixel_idx / 64;
    int bit_idx = pixel_idx % 64;
    uint64_t word = _grid[word_idx].load(std::memory_order_relaxed);
    return (word & (1ULL << bit_idx)) != 0;
  }

  void mark_matched_bounds(int sx, int sy, int w, int h) {
    if (!_grid) return;
    for (int y = sy; y < sy + h; ++y) {
      for (int x = sx; x < sx + w; ++x) {
        size_t pixel_idx = static_cast<size_t>((y * _width) + x);
        size_t word_idx = pixel_idx / 64;
        int bit_idx = pixel_idx % 64;

        _grid[word_idx].fetch_or(1ULL << bit_idx, std::memory_order_relaxed);
      }
    }
  }
};

struct ImageWMask {
  Image image;
  DynamicBitmask mask;
};

Result findAtlas(std::string filepattern, bool recurse,
                 std::vector<std::string> &atlas);

Result findSprites(std::string filepattern, bool recurse,
                   std::vector<std::string> &sprites);

Result formatOutput(OutputFormat outType, std::ostream &outPtr,
                    std::vector<AtlasMap> atlasMap, std::string pluginName);

Image loadImage(std::string filepath, bool crop = false);

Result
do_search(Config config,
          std::function<void(Config, const char *)> msgCallback = nullptr,
          std::function<void(Config, float, int)> progressCallback = nullptr);
} // namespace searcher