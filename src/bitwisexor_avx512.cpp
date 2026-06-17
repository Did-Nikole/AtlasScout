#include "bitwisexor_avx512.h"
#include <cstdint>
#include <immintrin.h>

bool BitwiseXorAvx512Plugin::bitwise_xor_match(const uint32_t *sheet_data,
                                               int sheet_width,
                                               const uint32_t *target_data,
                                               const uint32_t *mask_data,
                                               int temp_w, int temp_h,
                                               int start_x, int start_y) {

  // Define a lambda helper to process a single row.
  auto check_row = [&](int ty) -> bool {
    int sy = start_y + ty;
    const uint32_t *sheet_row = &sheet_data[sy * sheet_width + start_x];
    const uint32_t *target_row = &target_data[ty * temp_w];
    const uint32_t *mask_row = &mask_data[ty * temp_w];

    int tx = 0;
    // Process 16 pixels (512 bits) at a time
    for (; tx <= temp_w - 16; tx += 16) {
      __m512i t_vec = _mm512_loadu_si512((const void *)&target_row[tx]);
      __m512i s_vec = _mm512_loadu_si512((const void *)&sheet_row[tx]);
      __m512i m_vec = _mm512_loadu_si512((const void *)&mask_row[tx]);

      __m512i diff = _mm512_xor_si512(t_vec, s_vec);
      __m512i masked_diff = _mm512_and_si512(diff, m_vec);

      __mmask16 nonzero_mask = _mm512_test_epi32_mask(masked_diff, masked_diff);
      if (nonzero_mask != 0) {
        return false; // Mismatch found
      }
    }
    // Scalar cleanup for remaining pixels
    for (; tx < temp_w; ++tx) {
      uint32_t m = mask_row[tx];
      if (m != 0) {
        if ((target_row[tx] ^ sheet_row[tx]) & m) {
          return false; // Mismatch found
        }
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
