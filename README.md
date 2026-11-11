# ceparco-integrating-project-scanfast
CEPARCO: Integrating Project - ScanFast

## Project Initialization
*Note: Sofia's VS version is VS 2022 with the latest version. It might be relevant if VS 2026 gamit niyo :D*

ceparco-integrating-project-scanfast/
├── datasets/
│   ├── clean/             <-- Put 5-10 crisp QR PNG/JPGs here
│   ├── noisy/             <-- Salt-and-pepper degraded QR images
│   ├── uneven_lighting/   <-- Shadowed / gradient QR images
│   └── ground_truth.txt   <-- Expected decoded string per filename
├── include/
│   └── stb_image.h        <-- Single-header image loader
├── third_party/
│   └── quirc/             <-- Only the core .c and .h files from quirc/lib/
└── scanfast/
    ├── main.c             <-- Test harness & timer
    ├── baseline_sauvola.c <-- Your sequential C baseline
    └── scanfast.sln and other vs supplementaries

**datasets** - this is where various pictures qr will reside for testing.
**include** - has the header files that define the project own functions, structures and constants that will define the preprocessing baseline c and preprocess avx2.
**third-party** - multi-file libraries from quirc is stored.

### stb_image.h
This is a public domain image loader provided by the GitHub directory `nothings/stb`. 
Inputs:
    int *x                 -- outputs image width in pixels
    int *y                 -- outputs image height in pixels
    int *channels_in_file  -- outputs # of image components in image file
    int desired_channels   -- if non-zero, # of image components requested in result
Output:
    unsigned *char         -- points to the pixel data, or null if invalid or corrupted image data, that consists of *y scalines of *x pixels.

*Note: size should not exceed 16777216 pixels due to max memory limit to properly load a proper aspect ratio.*

### quirc-lib imports
The library use is documented and summarized by the contents of the header file `quirc.h`.
**quirc_new** - constructor for a new qr code recognizer.
**quirc_destroy** - destroy a qr code recognizer.
**quirc_resize** - resize a recognizer.
**quirc_begin** - used to process image for qr code recog. must be called first to obtain access to the buffer where the imput image will be placed. 
**quirc_end** - called after quirc_begin to begin process for qr-code recognition.
**quirc_point** - location in the input image buffer.
**quirc_code** - used to return information about detected qr codes in the input.
**quirc_data** - holds decoded qr-code data.
**quirc_count** - number of qr codes identified in the last processed image.
**quirc_extract** - extract qr code at a given index.
**quirc_decode_error_t** - returns payload data
**quirc_flip** - flips qr code 

### main.c
This is ScanFast's test harness where quirc and avx2 will be executed and compared based on performance.

*Note: clock() is not a reliable way to time the performance for quirc and avx2. Use a different library :D!*

### baseline_sauvola.c
Sequential baseline is written using uint32_t for the raw sum and uint64_t for the squared sum.

*Note on quirc integration: Because quirc_begin() expects an 8-bit grayscale buffer and runs its own internal thresholding, Sauvola baseline outputs 0 for black pixels and 255 for white pixels into gray_out. When quirc runs its threshold on a clean 0/255 buffer, it preserves the exact binary decisions without distortion.*

### VS configurations
Configurations are set to x64 for pure C files, quirc and AVX2 SIMD. **Project properties** are modified for the following settings:
1. C/C++ General Additional Include Directories: `..\include;..\third_party\quirc` since the solution is stored in another folder after root.
2. C/C++ Code Generation Enable Enhanced Instruction Set: `Select Advanced Vector Extensions 2 (/arch:AVX2)`
3. C/C++ Language C Language Standard: `ISO C17`
4. Debugging Working Directory: `$(ProjectDir)..` to involve relative paths from different folderes in root. 

    
