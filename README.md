# ceparco-integrating-project-scanfast
CEPARCO: Integrating Project - ScanFast
Group 5

Dimaunahan, Chelsea Jei

Infante, Gabriel 

Maravilla, Sofia

Melanio, Marion Zane

## Project Milestone Update
***Placeholder here***

## Video Demo Link
***Placeholder here***

## Project Initialization
*Note: Sofia's VS version is VS 2022 with the latest version. It might be relevant if VS 2026 gamit niyo :D*

```text
ceparco-integrating-project-scanfast/
├── datasets/
│   ├── clean/              <-- Put 5-10 crisp QR PNG/JPGs here
│   ├── noisy/              <-- Salt-and-pepper degraded QR images
│   ├── uneven_lighting/    <-- Shadowed / gradient QR images
│   └── ground_truth.txt    <-- Expected decoded string per filename
├── include/
│   └── stb_image.h         <-- Single-header image loader
├── third_party/
│   └── quirc/              <-- Only the core .c and .h files from quirc/lib/
└── scanfast/
    ├── main.c              <-- Test harness & timer
    ├── baseline_sauvola.c  <-- Your sequential C baseline
    └── scanfast.sln        <-- Visual Studio solution and supplementaries
```

**datasets** - this is where various pictures qr will reside for testing.
**include** - has the header files that define the project own functions, structures and constants that will define the preprocessing baseline c and preprocess avx2.
**third-party** - multi-file libraries from quirc is stored.

### stb_image.h
This is a public domain image loader provided by the GitHub directory `nothings/stb`. 
Inputs:
``` text
    int *x                 -- outputs image width in pixels
    int *y                 -- outputs image height in pixels
    int *channels_in_file  -- outputs # of image components in image file
    int desired_channels   -- if non-zero, # of image components requested in result
```
Output:
``` text
    unsigned *char         -- points to the pixel data, or null if invalid or corrupted image data, that consists of *y scalines of *x pixels.
```
*Note: size should not exceed 16777216 pixels due to max memory limit to properly load a proper aspect ratio.*

### quirc-lib imports
The library use is documented and summarized by the contents of the header file `quirc.h`.

- **quirc_new** - constructor for a new qr code recognizer.
- **quirc_destroy** - destroy a qr code recognizer.
- **quirc_resize** - resize a recognizer.
- **quirc_begin** - used to process image for qr code recog. must be called first to obtain access to the buffer where the imput image will be placed. 
- **quirc_end** - called after quirc_begin to begin process for qr-code recognition.
- **quirc_point** - location in the input image buffer.
- **quirc_code** - used to return information about detected qr codes in the input.
- **quirc_data** - holds decoded qr-code data.
- **quirc_count** - number of qr codes identified in the last processed image.
- **quirc_extract** - extract qr code at a given index.
- **quirc_decode_error_t** - returns payload data
- **quirc_flip** - flips qr code


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

    
## Original Proposal
## Abstract
<mark>ScanFast is an image preprocessing program written in C and x86-64 Assembly that aims to improve the processing speed of QR code images before they are passed to an existing QR code decoder such as Quirc. The project focuses on AVX2 SIMD optimization of resolution-adaptive Sauvola binarization, which converts grayscale images into black-and-white images using local image statistics. Grayscale conversion and selective median filtering will serve as supporting preprocessing steps. A sequential C implementation will be used as the baseline and compared with the AVX2-optimized implementation in terms of execution time, output correctness, and QR code decoding success rate. The project will test images under different conditions, including clean images, salt-and-pepper noise, and uneven lighting.</mark>

## Description
### A. Scope and Limitations 
The project aims to use an existing decoder instead of making a QR recognition engine from scratch. It will not find, orient, or parse the bit-matrix data of QR codes. 
### B. Inputs
The inputs of the project would involve different image formats, including PNG, JPEG, raw RGB/RGBA arrays, and the like.

As part of testing, different image formats would be in different conditions, such as:
- Standard, clear QR codes.
- QR codes with salt-and-pepper noise.
- QR codes under uneven lighting conditions.

### C. Proposed Process
#### 1. Grayscale Conversion 
Images are basically made up of arranged pixels, and their size is represented by a matrix. They are functions f(x,y), taking up values in {0, 1, ..., 255}, where x and y are the coordinates of the pixels. (Cunibil, 2026)

Grayscale is an image made up of various shades of gray, from black to white, simplifying image data while still retaining and enhancing the important visual details through its luminance (white as brightest, black as darkest, and shades of gray as everything in between) (What Is Grayscale? Definition, Uses, and Applications in Digital Imaging | Lenovo US, 2023). 

According to Linkgit (2024), the mathematical conversion of images to grayscale would involve an algorithm of summing each pixel’s red, green, and blue values, with weights assigned based on the luminous intensity of a color as perceived by the human eye. Its general conversion is:

$$Grayscale = (0.299 * Red) + (0.587 * Green) + (0.114 * Blue)$$

Converting an image to grayscale for the project would involve using 8-bit grayscale as using 16-bit or floating point would consume more memory, increasing latency. 8-bit grayscale has 256 different kinds of gray, and can take values between 0 and 255. 16-bit grayscale has 65,536 different shades of gray, which offers increased levels of intensity and differentiation between pixels. Floating point grayscale has positive floating-point pixel intensity, which in theory, has no limitations to the amount of gray levels possible, and would give high accuracy of images (Linkgit, 2024).

Implementing this with SIMD:
- The conversion formula would be converted into integer arithmetic Y = (77R + 150G +29B) >> 8 for efficient packed integer operations.
- The RGB values will be in 256-bit YMM registers and expanded from 8-bit to 16-bit words.
- The RGB values will be multiplied in parallel by the fixed weights across 16 pixels, summed and shifted to the right by 8 bits to perform a faster division by 256, and then the processed 16-bit values will be packed back into 8-bit byte vectors to write out 32 grayscale output pixels per iteration using vector instructions.

#### 2. Denoising
Images often times contain noise. Processing these images without filtering would result with an unreliable output, can even lead to the QR decoder not scanning the image properly. To filter out the salt-and-pepper noise within images, the project would implement a Selective Median Filter inspired by an algorithm by Zhu and Huang (2012).

Noise typically does not take up 100% of the  image. The Standard Median Filter would have to go through the whole image, making it inefficient. Using this filter would also have the risk of blurring the edges, which is crucial for QR decoders.

Zhu and Huang’s algorithm doesn’t go through the whole image. Instead, it checks if a pixel is noise by looking at the 3x3 grid around it (the pixel itself plus 8 immediate neighbors), finding the average of the 9 pixels to get the average brightness, and subtracting the center or target pixel’s value from the average. If there is a big absolute difference between the average and the pixel’s value compared to the threshold (e.g. |120-0| = 120 > threshold, which we could put as 30 as an example), it would be considered noise. Otherwise, it would be considered clean and the algorithm skips it. If a pixel is considered as noise, the algorithm would find the median of the 3x3 grid and replace the value of the noisy pixel with the median. 

However, it is important to note that the “big/small absolute difference” would depend on the threshold set, and different values will be benchmarked throughout the project to see what threshold gives the best outcome.

Implementing this with SIMD:
- Using the horizontal addition operation can speed up the addition process of multiple pixels at once. 
- Vector subtraction will be used to compute the difference between local averages and center pixels.
- Vector Min/Max operations will be used for finding the median without the need for scalar loops or branching. 

#### 3. SIMD Parallel Adaptive Binarization
<mark>Binarization converts a grayscale image into a black-and-white image by classifying each pixel as either black or white. This step is important for QR code processing because uneven lighting can make a single global threshold unreliable. ScanFast will use Sauvola's adaptive binarization algorithm, which calculates a local threshold based on the mean and standard deviation of pixel intensities within a surrounding window (Sauvola & Pietikäinen, 2000).</mark>

<mark>The size of the local window will adapt to the input image dimensions. For each pixel, the algorithm will use integral images to calculate the local sum and squared sum efficiently. These values will be used to calculate the local mean and variance, from which the local standard deviation and Sauvola threshold can be determined. Two integral images will be maintained: one containing the sum of pixel intensities and another containing the sum of squared pixel intensities.</mark>

<mark>The Sauvola threshold is calculated using:</mark>

<mark>$$T(x,y)=\mu(x,y)\left[1+k\left(\frac{\sigma(x,y)}{R}-1\right)\right]$$</mark>

<mark>where \(\mu(x,y)\) is the local mean, \(\sigma(x,y)\) is the local standard deviation, \(R\) is the dynamic range of standard deviation, and \(k\) is a parameter that controls the threshold adjustment. The values of \(k\), \(R\), and the rule for selecting the window size will be specified and tested during implementation.</mark>

<mark>The main SIMD optimization will focus on the Sauvola binarization pipeline. AVX2 instructions will be used to accelerate integral-image calculations and evaluate the threshold for multiple pixels in parallel. The integral-image implementation must correctly handle prefix sums across vector boundaries, including the two 128-bit halves of a 256-bit YMM register, as well as across rows. The implementation will also account for accumulator limits when storing sums and squared sums.</mark>

<mark>For areas with very low local contrast, the implementation will use a defined variance-floor condition to avoid unstable threshold calculations. The fallback threshold and its parameters will be specified and tested during implementation rather than assumed to be part of the standard Sauvola formula.</mark>

<mark>After thresholding, the output will be converted into the pixel representation required by the next stage. If a bit-packed output is used internally, an integration step will ensure that it is converted to the format expected by Quirc. The project will evaluate the correctness of the binarized output and its effect on QR code decoding.</mark>

<mark>Although grayscale conversion, median filtering, and integral-image operations can also be accelerated using SIMD, these will not be treated as separate principal optimization targets. The main performance evaluation will focus on the AVX2-optimized Sauvola binarization stage.</mark>

### D. Comparison with Existing Implementations
There are existing libraries that can detect and decode QR codes, such as ZXing-C++ and Quirc. ScanFast focuses on preprocessing QR code images before they are passed to an existing decoder, rather than implementing QR code detection and decoding from scratch.

ZXing-C++ supports different barcode formats, including QR codes, and includes binarization methods such as Hybrid Binarizer. Hybrid Binarizer uses local image regions to help handle uneven lighting. ScanFast will instead implement <mark>resolution-adaptive Sauvola binarization using integral images, with the main optimization effort focused on accelerating this stage using AVX2 SIMD instructions.</mark> The project will compare the resulting processing performance and QR code decoding success under different image conditions.

Quirc is a lightweight QR code decoding library written in C. It accepts grayscale image data and performs its own processing to detect and decode QR codes. ScanFast will preprocess the image before passing it to Quirc, allowing the project to test whether Sauvola binarization and the supporting denoising step improve decoding results for images affected by noise or uneven lighting.

<mark>SIMD implementations of operations such as grayscale conversion, median filtering, and integral-image construction already exist. Therefore, ScanFast will not claim that vectorizing these operations is, by itself, a new contribution. Instead, the project will focus on implementing and evaluating its specific resolution-adaptive Sauvola binarization pipeline and integrating its output with Quirc.</mark>

The project will compare a sequential C implementation with an AVX2-optimized implementation using the same set of QR code images. The evaluation will measure processing time, CPU clock cycles per pixel, output correctness, and QR code decoding success rate. This will help determine whether the optimized binarization stage improves performance while maintaining useful preprocessing results.

### E. Outputs 
The project is expected to produce a working prototype of ScanFast, an image preprocessing program written in C and optimized using AVX2 SIMD instructions. It will process QR code images before passing the preprocessed data to an existing QR code decoder. The expected outputs are the following:

1. Preprocessed Image

   The program will produce a black-and-white representation of the input QR code image after preprocessing. The internal representation may use 1 bit per pixel or 8-bit binary values, depending on the implementation. <mark>Before the image is passed to Quirc, the program will ensure that the output is converted into the input representation required by the decoder.</mark>

   The resulting images will be tested using clean QR codes, images with salt-and-pepper noise, and images with uneven lighting. These tests will help determine whether the preprocessing steps preserve important QR code patterns and improve decoding results.

2. Performance Benchmark Report

   The project will produce a report comparing the sequential C implementation with the AVX2-optimized implementation. The report will include:

   - Processing time, measured in milliseconds per megapixel.
   - CPU clock cycles per pixel (CPK).
   - Speedup, calculated by dividing the sequential C execution time by the AVX2 execution time.
   - Output correctness between the sequential and AVX2 implementations.

   <mark>The principal performance comparison will focus on the Sauvola binarization stage. Other preprocessing steps will be included as needed to evaluate the complete pipeline, but they will not be treated as separate principal optimization targets.</mark>

3. QR Code Decoding Success Rate

   The project will compare QR code decoding success before and after applying ScanFast's preprocessing. The tests will use the same dataset of QR code images, including images affected by salt-and-pepper noise and uneven lighting.

   The decoding success rate will be calculated using the following formula:

   $$\text{Success Rate}=\frac{\text{Successfully Decoded QR Codes}}{\text{Total QR Codes Tested}}\times100\%$$

   The results will show the percentage of QR codes successfully decoded from the original images and the preprocessed images. This will help determine whether the preprocessing pipeline improves decoding success under the tested conditions.

4. Integrated Prototype

   The final output will be a working prototype that connects ScanFast to an existing QR code decoder, with Quirc as the initial integration target. It will demonstrate the process of loading an image, applying preprocessing, converting the result into the required input format, and passing it to the decoder.

   The prototype and its test results will be used to evaluate the performance of the sequential C and AVX2 implementations, as well as the effect of preprocessing on QR code decoding success.

## Implementation
ScanFast will be developed using C and x86-64 Assembly language on a compatible computer system. C will handle image loading, memory management, execution-time measurement, and integration with an existing QR code decoding library, such as Quirc. NASM will be used to assemble the x86-64 Assembly code, while GCC will compile the C source code and link the required components. The program will check whether the target processor supports AVX2 before executing the optimized implementation.

The project will include three preprocessing operations: grayscale conversion, selective denoising, and adaptive binarization. Grayscale conversion will transform RGB pixels into 8-bit grayscale values using the integer approximation \(Y=(77R+150G+29B)>>8\). Selective denoising will use a selective median filtering algorithm inspired by Zhu and Huang (2012). It will examine a pixel and its 3×3 neighborhood, compare the pixel's intensity with the neighborhood average, and replace the pixel with the neighborhood median if the absolute difference exceeds a selected threshold. The threshold will be evaluated during testing.

<mark>The principal SIMD optimization target will be resolution-adaptive Sauvola binarization. The sequential C implementation will serve as the baseline, while the AVX2 implementation will accelerate the binarization pipeline using integral images and vectorized threshold calculations. Two integral images will be used to obtain the local sum and squared sum needed to calculate the local mean and standard deviation.</mark>

<mark>The implementation will define the rule for selecting the window size based on image dimensions, as well as the Sauvola parameters and the low-variance fallback condition. The integral-image implementation will account for prefix-sum propagation across vector boundaries and image rows. It will also check that the selected accumulator types can safely store the required sums and squared sums for the supported image dimensions.</mark>

The AVX2 implementation will process multiple pixels in parallel using 256-bit YMM registers. The number of pixels processed per iteration will depend on the data type and operations being performed. Any remaining pixels that do not fill a complete vector will be processed using a scalar loop.

<mark>After binarization, the program will convert the output into the representation required by Quirc. The project will test both output correctness and the effect of preprocessing on QR code decoding success.</mark>

To evaluate performance, both implementations will process the same QR code images under identical testing conditions. Execution time will be measured across multiple runs. Speedup will be calculated using the following formula:

$$\text{Speedup}=\frac{\text{Sequential C Execution Time}}{\text{AVX2 Execution Time}}$$

The evaluation will report processing time, speedup, output correctness, and QR code decoding success rate. <mark>The main performance analysis will focus on the Sauvola binarization stage, while the complete preprocessing pipeline will be evaluated to determine whether the changes are useful when integrated with Quirc.</mark>

## Midproject Milestones

### Milestone 1 — Baseline Infrastructure & Dataset Setup

- <mark>Build a C test harness to load QR code images under different conditions, including clean images, salt-and-pepper noise, and uneven lighting.</mark>
- <mark>Integrate the Quirc QR decoding library and establish the sequential C baseline.</mark>
- <mark>Record baseline processing time and QR code decoding success.</mark>

### Milestone 2 — Supporting Preprocessing Steps

- <mark>Implement sequential C grayscale conversion using integer arithmetic.</mark>
- <mark>Implement the selective median filter for salt-and-pepper noise.</mark>
- <mark>Verify that the supporting preprocessing steps produce valid output.</mark>

### Milestone 3 — AVX2-Optimized Sauvola Binarization

- <mark>Implement resolution-adaptive window selection based on image dimensions.</mark>
- <mark>Implement the sequential C version of the dual integral images, storing the sum of pixel intensities and the sum of squared intensities.</mark>
- <mark>Implement the AVX2 version of integral-image construction, ensuring correct prefix sums across vector boundaries and image rows.</mark>
- <mark>Implement AVX2 vectorized Sauvola threshold calculations, including the low-variance fallback condition.</mark>
- <mark>Verify the binarized output against the sequential C implementation.</mark>

### Milestone 4 — Integration, Benchmarking & Final Analysis

- <mark>Connect the preprocessed output to Quirc using the input format required by the decoder.</mark>
- <mark>Run tests on the selected QR code dataset.</mark>
- <mark>Compare sequential C and AVX2 execution time, speedup, output correctness, and QR code decoding success.</mark>
- <mark>Summarize the results and limitations of the implementation.</mark>

## References
What is Grayscale? Definition, Uses, and Applications in Digital Imaging | Lenovo US. (2023, May 28). https://www.lenovo.com/us/en/glossary/grayscale/?orgRef=https%253A%252F%252Fwww.google.com%252F&srsltid=AU7gw4VwlSKMHXb5wpxaZlYoPk4ofiV7Azv_BR5A3UgkMDAu037RpcC7

Cunibil, C. (2026, September 30). An image as a matrix. Tangente Magazine. https://tangente-mag.com/en/articles/an-image-as-a-matrix 

K. Peuwnuan, K. Woraratpanya and K. Pasupa, "Modified adaptive thresholding using integral image," 2016 13th International Joint Conference on Computer Science and Software Engineering (JCSSE), Khon Kaen, Thailand, 2016, pp. 1-5, doi: 10.1109/JCSSE.2016.7748917. 

Linkgit. (2024, May 18). Why use Grayscale Conversion when Processing Images? Grayscale Images. https://www.grayscaleimage.com/why-use-grayscale-conversion-when-processing-images/ 

Quirc. QR decoder library. https://github.com/dlbeer/quirc

Zhu, Y., & Huang, C. (2012). An improved median filtering algorithm for image noise reduction. Physics Procedia, 25, 609–616. https://doi.org/10.1016/j.phpro.2012.03.133

ZXing-C++. ReadBarcode.cpp and ReaderOptions.h. https://github.com/zxing-cpp/zxing-cpp
