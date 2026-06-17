#pragma once
#include "ISearchPlugin.hpp"

class BitwiseXorFallbackPlugin : public ISearchPlugin {
public:
  std::string get_name() override { return "fallback"; }
  bool is_supported() override;
  int priority() override { return 0; }
  bool bitwise_xor_match(const uint32_t *sheet_data, int sheet_width,
                         const uint32_t *target_data,
                         int temp_w, int temp_h, int start_x,
                         int start_y) override;
};
