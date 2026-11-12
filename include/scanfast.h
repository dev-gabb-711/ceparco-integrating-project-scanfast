#ifndef SCANFAST_H
#define SCANFAST_H

#include <stdint.h>

#define MAX_IMAGE_PIXELS 16777216  // 4096 x 4096 pixel cap

typedef struct {
    int noise_thresh;   // Zhu & Huang threshold (default: 30)
    float sauvola_k;    // Sauvola sensitivity k (default: 0.2)
    float sauvola_r;    // Dynamic range R (default: 128.0)
    int window_size;    // 0 = auto-select from image dimensions
} ScanFastConfig;

// Milestone 2: Integer grayscale conversion -> Y = (77R + 150G + 29B) >> 8
void rgb_to_gray(const uint8_t* src, uint8_t* gray, int w, int h, int channels);

// Milestone 2: Zhu & Huang (2012) 3x3 selective median filter
void selective_median(const uint8_t* gray, uint8_t* out, int w, int h, int thresh);

// Milestone 3 (placeholder): Sequential C Sauvola adaptive binarization
void sauvola_binarize_c(const uint8_t* gray, uint8_t* bin_out, int w, int h, ScanFastConfig cfg);

#endif