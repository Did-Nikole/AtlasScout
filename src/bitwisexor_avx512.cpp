#include "bitwisexor_avx512.h"
#include <cstdint>
#include <immintrin.h>

bool BitwiseXorAvx512Plugin::bitwise_xor_match(const uint32_t *sheet_data,
                                               int sheet_width,
                                               const uint32_t *target_data,
                                               int temp_w, int temp_h,
                                               int start_x, int start_y) {

  // Quick smoke test on top-left and middle pixels
  if (sheet_data[start_y * sheet_width + start_x] != target_data[0]) {
    return false;
  }
  if (temp_h > 0 && temp_w > 0) {
    int mid_x = temp_w / 2;
    int mid_y = temp_h / 2;
    if (sheet_data[(start_y + mid_y) * sheet_width + (start_x + mid_x)] != target_data[mid_y * temp_w + mid_x]) {
      return false;
    }
  }

  // Define a lambda helper to process a single row.
  auto check_row = [&](int ty) -> bool {
    int sy = start_y + ty;
    const uint32_t *sheet_row = &sheet_data[sy * sheet_width + start_x];
    const uint32_t *target_row = &target_data[ty * temp_w];

    int tx = 0;
    // Process 16 pixels (512 bits) at a time
    for (; tx <= temp_w - 16; tx += 16) {
      __m512i t_vec = _mm512_loadu_si512((const void *)&target_row[tx]);
      __m512i s_vec = _mm512_loadu_si512((const void *)&sheet_row[tx]);

      __m512i diff = _mm512_xor_si512(t_vec, s_vec);

      __mmask16 nonzero_mask = _mm512_test_epi32_mask(diff, diff);
      if (nonzero_mask != 0) {
        return false; // Mismatch found
      }
    }
    // Scalar cleanup for remaining pixels
    for (; tx < temp_w; ++tx) {
      if (target_row[tx] != sheet_row[tx]) {
        return false; // Mismatch found
      }
    }
    return true; // Row matches perfectly
  };

  // 1. Run the Smoke Test on the middle row first
  int mid_ty = temp_h / 2;
  if (temp_h > 0) {
    if (!check_row(mid_ty)) {
      return false; // Fast exit if the center doesn't match
    }
  }

  // 2. Process all other rows sequentially
  for (int ty = 0; ty < temp_h; ++ty) {
    if (ty == mid_ty) {
      continue; // Skip the middle row since we already validated it
    }

    if (!check_row(ty)) {
      return false;
    }
  }

  return true;
}

bool BitwiseXorAvx512Plugin::is_supported() {
  return __builtin_cpu_supports("avx512f") && __builtin_cpu_supports("avx512bw");
}

extern "C" ISearchPlugin *create_plugin() {
  return new BitwiseXorAvx512Plugin();
}
