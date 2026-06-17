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

#include "ISearchPlugin.hpp"

class BitwiseXorAvx2Plugin : public ISearchPlugin {
public:
  std::string get_name() override { return "avx2"; };
  bool is_supported() override;
  int priority() override { return 2; }
  bool bitwise_xor_match(const uint32_t *sheet_data, int sheet_width,
                         const uint32_t *target_data,
                         int temp_w, int temp_h, int start_x,
                         int start_y) override;
};