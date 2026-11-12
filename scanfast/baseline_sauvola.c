#include "scanfast.h"
#include <stdio.h>
#include <string.h>

void rgb_to_gray(const uint8_t* src, uint8_t* gray, int w, int h, int channels) {
    size_t n = (size_t)w * (size_t)h;

    if (channels == 1) {
        memcpy(gray, src, n);
        return;
    }

    if (channels == 2) {
        // Grayscale + Alpha: composite onto white background
        for (size_t i = 0; i < n; i++) {
            uint32_t g = src[i * 2 + 0];
            uint32_t a = src[i * 2 + 1];
            gray[i] = (uint8_t)((g * a + 255U * (255U - a) + 127U) / 255U);
        }
        return;
    }

    if (channels == 3) {
        for (size_t i = 0; i < n; i++) {
            uint32_t r = src[i * 3 + 0];
            uint32_t g = src[i * 3 + 1];
            uint32_t b = src[i * 3 + 2];
            gray[i] = (uint8_t)((77U * r + 150U * g + 29U * b) >> 8);
        }
        return;
    }

    if (channels == 4) {
        // RGBA: compute luminance and composite onto white background
        for (size_t i = 0; i < n; i++) {
            uint32_t r = src[i * 4 + 0];
            uint32_t g = src[i * 4 + 1];
            uint32_t b = src[i * 4 + 2];
            uint32_t a = src[i * 4 + 3];
            uint32_t y = (77U * r + 150U * g + 29U * b) >> 8;
            gray[i] = (uint8_t)((y * a + 255U * (255U - a) + 127U) / 255U);
        }
    }
}

static uint8_t median9(uint8_t v[9]) {
    for (int i = 1; i < 9; i++) {
        uint8_t key = v[i];
        int j = i - 1;
        while (j >= 0 && v[j] > key) {
            v[j + 1] = v[j];
            j--;
        }
        v[j + 1] = key;
    }
    return v[4];
}

void selective_median(const uint8_t* gray, uint8_t* out, int w, int h, int thresh) {
    if (w < 3 || h < 3) {
        memcpy(out, gray, (size_t)w * (size_t)h);
        return;
    }

    // Keep 1-pixel top and bottom borders unchanged
    memcpy(out, gray, (size_t)w);
    memcpy(out + (size_t)(h - 1) * w, gray + (size_t)(h - 1) * w, (size_t)w);

    int scaled_thresh = 9 * thresh;

    for (int y = 1; y < h - 1; y++) {
        const uint8_t* prev = gray + (size_t)(y - 1) * w;
        const uint8_t* curr = gray + (size_t)y * w;
        const uint8_t* next = gray + (size_t)(y + 1) * w;
        uint8_t* dst = out + (size_t)y * w;

        dst[0] = curr[0];
        dst[w - 1] = curr[w - 1];

        for (int x = 1; x < w - 1; x++) {
            uint8_t win[9] = {
                prev[x - 1], prev[x], prev[x + 1],
                curr[x - 1], curr[x], curr[x + 1],
                next[x - 1], next[x], next[x + 1]
            };

            int sum = win[0] + win[1] + win[2] +
                win[3] + win[4] + win[5] +
                win[6] + win[7] + win[8];

            // Avoid division: |center - (sum / 9)| > thresh  <=>  |9 * center - sum| > 9 * thresh
            int diff = 9 * (int)win[4] - sum;
            if (diff < 0) diff = -diff;

            dst[x] = (diff > scaled_thresh) ? median9(win) : win[4];
        }
    }
}

void sauvola_binarize_c(const uint8_t* gray, uint8_t* bin_out, int w, int h, ScanFastConfig cfg) {
    (void)cfg;
    // Milestone 3 TODO: Build uint32_t sum + uint64_t squared-sum integral images
    // and output 0 (black) or 255 (white) using Sauvola's local threshold.
    memcpy(bin_out, gray, (size_t)w * (size_t)h);
}