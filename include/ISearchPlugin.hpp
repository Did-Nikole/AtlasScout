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

// ISearchPlugin.hpp
#pragma once
#include <string>

class ISearchPlugin {
public:
  virtual ~ISearchPlugin() = default;

  // The required methods every plugin MUST implement
  virtual std::string get_name() = 0;
  virtual bool is_supported() = 0;
  virtual int priority() = 0;

  // Using a dummy parameters for the example

  // @TODO: remove the mask parameter, not needed anymore
  /**
   * @brief Checks if the target data matches the sheet data at the given
   * position
   *
   * @param sheet_data raw sheet data (not padded)
   * @param sheet_width width of the sheet in pixels
   * @param target_data raw template data (not padded)
   * @param mask_data raw mask data (not padded)
   * @param temp_w width of the template in pixels
   * @param temp_h height of the template in pixels
   * @param start_x top-left x coordinate of the region to check in the sheet
   * @param start_y top-left y coordinate of the region to check in the sheet
   * @return true if the template matches the sheet data at the given position
   * @return false if the template does not match the sheet data at the given
   * position
   */
  virtual bool bitwise_xor_match(const uint32_t *, int, const uint32_t *,
                                 int, int, int, int) = 0;
};

// Define a standardized type for the C-style factory function
typedef ISearchPlugin *(*CreatePluginFunc)();