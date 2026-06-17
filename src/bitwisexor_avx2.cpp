#include "bitwisexor_avx2.h"
#include <cstdint>
#include <immintrin.h>

// this can be optimised to not use the mask as it has twice comeback to bite
// us, and have had to code around it, i will leave this as is for now but is
// on the cards to redo in the future

bool BitwiseXorAvx2Plugin::bitwise_xor_match(const uint32_t *sheet_data,
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
    // Process 8 pixels (256 bits) at a time
    for (; tx <= temp_w - 8; tx += 8) {
      __m256i t_vec = _mm256_loadu_si256((const __m256i *)&target_row[tx]);
      __m256i s_vec = _mm256_loadu_si256((const __m256i *)&sheet_row[tx]);

      __m256i diff = _mm256_xor_si256(t_vec, s_vec);

      if (!_mm256_testz_si256(diff, diff)) {
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

bool BitwiseXorAvx2Plugin::is_supported() {
  return __builtin_cpu_supports("avx2");
}

extern "C" ISearchPlugin *create_plugin() { return new BitwiseXorAvx2Plugin(); }
