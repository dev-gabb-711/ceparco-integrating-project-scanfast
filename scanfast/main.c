#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "quirc.h"
#include "scanfast.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#include <intrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_GT 256
#define MAX_TEXT 2048

typedef struct {
    char filename[128];
    char payload[MAX_TEXT];
} GroundTruth;

static double now_ms(void) {
    static LARGE_INTEGER freq = { 0 };
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart * 1000.0 / (double)freq.QuadPart;
}

static int is_image_file(const char* name) {
    const char* ext = strrchr(name, '.');
    if (!ext) return 0;
    return _stricmp(ext, ".png") == 0 ||
        _stricmp(ext, ".jpg") == 0 ||
        _stricmp(ext, ".jpeg") == 0 ||
        _stricmp(ext, ".bmp") == 0;
}

static int load_ground_truth(const char* path, GroundTruth* list, int max_items) {
    FILE* fp = fopen(path, "r");
    if (!fp) return 0;

    int count = 0;
    char line[MAX_TEXT + 256];
    while (fgets(line, sizeof(line), fp) && count < max_items) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0 || line[0] == '#') continue;

        char* sep = strpbrk(line, " \t");
        if (!sep) continue;
        *sep = '\0';

        char* text = sep + 1;
        while (*text == ' ' || *text == '\t') text++;

        strncpy(list[count].filename, line, sizeof(list[count].filename) - 1);
        list[count].filename[sizeof(list[count].filename) - 1] = '\0';
        strncpy(list[count].payload, text, sizeof(list[count].payload) - 1);
        list[count].payload[sizeof(list[count].payload) - 1] = '\0';
        count++;
    }

    fclose(fp);
    return count;
}

static const char* lookup_gt(const char* filename, const GroundTruth* list, int count) {
    for (int i = 0; i < count; i++) {
        if (_stricmp(list[i].filename, filename) == 0) {
            return list[i].payload;
        }
    }
    return NULL;
}

static int run_quirc(struct quirc* qr, const uint8_t* gray, int w, int h,
    char* out_text, size_t out_size, double* elapsed_ms) {
    double t0 = now_ms();

    if (quirc_resize(qr, w, h) < 0) {
        *elapsed_ms = now_ms() - t0;
        return 0;
    }

    int qw = 0, qh = 0;
    uint8_t* buf = quirc_begin(qr, &qw, &qh);
    memcpy(buf, gray, (size_t)w * (size_t)h);
    quirc_end(qr);

    int ok = 0;
    int detected = quirc_count(qr);
    quirc_decode_error_t last_err = QUIRC_SUCCESS;

    for (int i = 0; i < detected; i++) {
        struct quirc_code code;
        struct quirc_data data;
        quirc_extract(qr, i, &code);

        last_err = quirc_decode(&code, &data);
        if (last_err == QUIRC_ERROR_DATA_ECC) {
            quirc_flip(&code);
            last_err = quirc_decode(&code, &data);
        }
        if (last_err == QUIRC_SUCCESS) {
            size_t len = (size_t)data.payload_len;
            if (len >= out_size) len = out_size - 1;
            memcpy(out_text, data.payload, len);
            out_text[len] = '\0';
            ok = 1;
            break;
        }
    }

    if (!ok) {
        snprintf(out_text, out_size, "[det=%d: %s]",
            detected, detected > 0 ? quirc_strerror(last_err) : "no finder pattern");
    }

    *elapsed_ms = now_ms() - t0;
    return ok;
}

// Sanity checks for Milestone 2 math and edge-preserving denoising
static void verify_milestone2(void) {
    uint8_t rgb[12] = {
        255, 0,   0,    // Red   -> (77*255) >> 8  = 76
        0,   255, 0,    // Green -> (150*255) >> 8 = 149
        0,   0,   255,  // Blue  -> (29*255) >> 8  = 28
        255, 255, 255   // White -> 255
    };
    uint8_t gray[4] = { 0 };
    rgb_to_gray(rgb, gray, 4, 1, 3);
    int gray_ok = (gray[0] == 76 && gray[1] == 149 && gray[2] == 28 && gray[3] == 255);

    // 5x5 patch: left side dark (20), right side bright (220), one salt pixel (255) at (1,1)
    uint8_t patch[25] = {
        20,  20,  20, 220, 220,
        20, 255,  20, 220, 220,
        20,  20,  20, 220, 220,
        20,  20,  20, 220, 220,
        20,  20,  20, 220, 220
    };
    uint8_t filtered[25] = { 0 };
    selective_median(patch, filtered, 5, 5, 30);

    // Salt pixel at (1,1) should become 20; clean edge pixel at (2,2) should stay 20
    int filter_ok = (filtered[1 * 5 + 1] == 20) && (filtered[2 * 5 + 2] == 20);

    printf("[Milestone 2 Self-Test] Grayscale math: %s | Selective median: %s\n",
        gray_ok ? "PASS" : "FAIL",
        filter_ok ? "PASS" : "FAIL");
}

static void evaluate_folder(const char* label, const char* folder,
    const GroundTruth* gt, int gt_count,
    ScanFastConfig cfg, int export_sample) {
    char pattern[512];
    snprintf(pattern, sizeof(pattern), "%s/*", folder);

    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(pattern, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        printf("\n[%s] Folder not found: %s\n", label, folder);
        return;
    }

    struct quirc* qr = quirc_new();
    if (!qr) {
        FindClose(hFind);
        return;
    }

    printf("\n=== %s (%s) ===\n", label, folder);
    printf("%-20s | %-9s | %-6s | %-6s | %-9s | %s\n",
        "File", "Size", "Raw", "Prep", "Prep ms", "Decoded Payload");
    printf("-------------------------------------------------------------------------------------\n");

    int total = 0, raw_pass = 0, prep_pass = 0;
    double total_mp = 0.0, total_prep_ms = 0.0, total_quirc_ms = 0.0;
    uint64_t total_cycles = 0;

    do {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (!is_image_file(ffd.cFileName)) continue;

        char path[512];
        snprintf(path, sizeof(path), "%s/%s", folder, ffd.cFileName);

        int w = 0, h = 0, ch = 0;
        uint8_t* img = stbi_load(path, &w, &h, &ch, 0);
        if (!img) continue;

        size_t pixels = (size_t)w * (size_t)h;
        if (pixels == 0 || pixels > MAX_IMAGE_PIXELS) {
            stbi_image_free(img);
            continue;
        }

        uint8_t* gray = (uint8_t*)malloc(pixels);
        uint8_t* denoised = (uint8_t*)malloc(pixels);
        uint8_t* bin = (uint8_t*)malloc(pixels);
        if (!gray || !denoised || !bin) {
            free(gray);
            free(denoised);
            free(bin);
            stbi_image_free(img);
            continue;
        }

        // Baseline: Grayscale -> Quirc directly
        rgb_to_gray(img, gray, w, h, ch);
        char raw_text[MAX_TEXT] = { 0 };
        double raw_q_ms = 0.0;
        int raw_ok = run_quirc(qr, gray, w, h, raw_text, sizeof(raw_text), &raw_q_ms);

        // ScanFast pipeline: Grayscale -> Selective Median -> Sauvola (stub)
        uint64_t c0 = __rdtsc();
        double t0 = now_ms();
        rgb_to_gray(img, gray, w, h, ch);
        selective_median(gray, denoised, w, h, cfg.noise_thresh);
        sauvola_binarize_c(denoised, bin, w, h, cfg);
        double prep_ms = now_ms() - t0;
        uint64_t cycles = __rdtsc() - c0;

        // Export the first image's grayscale and denoised .pgm for visual verification
        if (export_sample && total == 0) {
            char out_gray[256], out_den[256];
            snprintf(out_gray, sizeof(out_gray), "output/%s_gray.pgm", label);
            snprintf(out_den, sizeof(out_den), "output/%s_denoised.pgm", label);
        }

        char prep_text[MAX_TEXT] = { 0 };
        double prep_q_ms = 0.0;
        int prep_ok = run_quirc(qr, bin, w, h, prep_text, sizeof(prep_text), &prep_q_ms);

        const char* expected = lookup_gt(ffd.cFileName, gt, gt_count);
        const char* gt_tag = "";
        if (expected) {
            gt_tag = (prep_ok && strcmp(expected, prep_text) == 0) ? "[GT OK]" : "[GT FAIL]";
        }

        char dim[32];
        snprintf(dim, sizeof(dim), "%dx%d(%dc)", w, h, ch);
        printf("%-20s | %-11s | %-6s | %-6s | %7.3fms | [bg=%u] %s %s\n",
            ffd.cFileName, dim,
            raw_ok ? "PASS" : "FAIL",
            prep_ok ? "PASS" : "FAIL",
            prep_ms,
            gray[0],
            prep_ok ? prep_text : (raw_ok ? raw_text : prep_text),
            gt_tag);

        total++;
        raw_pass += raw_ok;
        prep_pass += prep_ok;
        total_mp += (double)pixels / 1e6;
        total_prep_ms += prep_ms;
        total_quirc_ms += prep_q_ms;
        total_cycles += cycles;

        free(gray);
        free(denoised);
        free(bin);
        stbi_image_free(img);
    } while (FindNextFileA(hFind, &ffd));

    FindClose(hFind);
    quirc_destroy(qr);

    if (total > 0) {
        printf("-------------------------------------------------------------------------------------\n");
        printf("Decode Rate: Raw %d/%d (%.1f%%) | Preprocessed %d/%d (%.1f%%)\n",
            raw_pass, total, 100.0 * raw_pass / total,
            prep_pass, total, 100.0 * prep_pass / total);
        printf("Timing: Preprocess %.3f ms/MP (%.2f cycles/px) | Quirc %.3f ms\n",
            total_prep_ms / total_mp,
            (double)total_cycles / (total_mp * 1e6),
            total_quirc_ms);
    }
    else {
        printf("No images found in %s\n", folder);
    }
}

// Sweeps selective median thresholds on datasets/noisy to pick the best value
static void sweep_noise_threshold(const char* folder) {
    int thresholds[] = {30, 60, 90, 120, 150};
    int num_t = 5;

    printf("\n=== Milestone 2 Noise Threshold Sweep (%s) ===\n", folder);
    for (int t = 0; t < num_t; t++) {
        int thresh = thresholds[t];
        char pattern[512];
        snprintf(pattern, sizeof(pattern), "%s/*", folder);

        WIN32_FIND_DATAA ffd;
        HANDLE hFind = FindFirstFileA(pattern, &ffd);
        if (hFind == INVALID_HANDLE_VALUE) {
            printf("Folder not found: %s\n", folder);
            return;
        }

        struct quirc* qr = quirc_new();
        int total = 0, decoded = 0;

        do {
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            if (!is_image_file(ffd.cFileName)) continue;

            char path[512];
            snprintf(path, sizeof(path), "%s/%s", folder, ffd.cFileName);

            int w = 0, h = 0, ch = 0;
            uint8_t* img = stbi_load(path, &w, &h, &ch, 0);
            if (!img) continue;

            size_t pixels = (size_t)w * (size_t)h;
            if (pixels == 0 || pixels > MAX_IMAGE_PIXELS) {
                stbi_image_free(img);
                continue;
            }

            uint8_t* gray = (uint8_t*)malloc(pixels);
            uint8_t* denoised = (uint8_t*)malloc(pixels);
            if (gray && denoised) {
                rgb_to_gray(img, gray, w, h, ch);
                selective_median(gray, denoised, w, h, thresh);

                char text[MAX_TEXT] = { 0 };
                double ms = 0.0;
                total++;
                decoded += run_quirc(qr, denoised, w, h, text, sizeof(text), &ms);
            }

            free(gray);
            free(denoised);
            stbi_image_free(img);
        } while (FindNextFileA(hFind, &ffd));

        FindClose(hFind);
        quirc_destroy(qr);

        if (total == 0) {
            printf("No images found in %s\n", folder);
            return;
        }
        printf("Threshold = %2d -> Decoded %d/%d (%.1f%%)\n",
            thresh, decoded, total, 100.0 * decoded / total);
    }
}

int main(void) {
    _mkdir("output");

    GroundTruth gt[MAX_GT];
    int gt_count = load_ground_truth("dataset/ground_truth.txt", gt, MAX_GT);
    printf("ScanFast Test Harness | Ground-truth entries loaded: %d\n", gt_count);

    verify_milestone2();

    ScanFastConfig cfg = {120, 0.2f, 128.0f, 0};
    evaluate_folder("clean", "dataset/clean", gt, gt_count, cfg, 1);
    evaluate_folder("noisy", "dataset/noisy", gt, gt_count, cfg, 1);
    evaluate_folder("uneven_lighting", "dataset/uneven_lighting", gt, gt_count, cfg, 1);

    sweep_noise_threshold("dataset/noisy");

    return 0;
}