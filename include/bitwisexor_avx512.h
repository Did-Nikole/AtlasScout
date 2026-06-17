#pragma once
#include "ISearchPlugin.hpp"

class BitwiseXorAvx512Plugin : public ISearchPlugin {
public:
  std::string get_name() override { return "avx512"; }
  bool is_supported() override;
  bool bitwise_xor_match(const uint32_t *sheet_data, int sheet_width,
                         const uint32_t *target_data, const uint32_t *mask_data,
                         int temp_w, int temp_h, int start_x,
                         int start_y) override;
};
