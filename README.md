# QR Code

Generates a QR Code.  Optimized for low RAM use for embedded environments.

Required files: [`qrcode.h`](qrcode.h) [`qrcode.c`](qrcode.c)


## Create a QR Code

Initialize a `qrcode_t` object, where `maxVersion` is up to `QRCODE_VERSION_MAX` and `errorCorrectionLevel` is one of:
`QRCODE_ECL_L` (low, ~7%), `QRCODE_ECL_M` (medium, ~15%), `QRCODE_ECL_Q` (quartile, ~25%), `QRCODE_ECL_H` (high, ~30%).

```c
void QrCodeInit(qrcode_t *qrcode, int maxVersion, qrcode_error_correction_level_t errorCorrectionLevel);
```

Add a text segment to the QR Code object, where `mode` is typically `QRCODE_MODE_INDICATOR_AUTOMATIC` to detect minimal encoding, and `charCount` is `QRCODE_TEXT_LENGTH` if null-terminated string.

```c
void QrCodeSegmentAppend(qrcode_t *qrcode, qrcode_segment_t *segment, qrcode_mode_indicator_t mode, const char *text, size_t charCount, bool mayUppercase);
```

Get the decided dimension of the code (0=error) and, if dynamic memory is used, the minimum buffer sizes for the code and scratch area (only used during generation itself). 
If you want to use fixed-size buffers, you can pass `NULL` to ignore the parameters and the maximum buffer sizes can be known at compile time using: `QRCODE_BUFFER_SIZE(maxVersion)` and `QRCODE_SCRATCH_BUFFER_SIZE(maxVersion)`.

```c
int QrCodeSize(qrcode_t *qrcode, size_t *bufferSize, size_t *scratchBufferSize);
```

Generate the QR Code (`scratchBuffer` is only used during generation):

```c
bool QrCodeGenerate(qrcode_t *qrcode, uint8_t *buffer, uint8_t *scratchBuffer);
```

Retrieve the modules (bits/pixels) of the QR code at the given coordinate (0=light, 1=dark), you should ensure there are `QRCODE_QUIET_STANDARD` (4) units of light on all sides of the final presentation:

```c
int QrCodeModuleGet(qrcode_t* qrcode, int x, int y);
```


## Demonstration program

Demonstration program ([`main.c`](main.c)) to generate and output QR Codes.

To display a QR Code the console (use `--invert` if your console is light-on-dark, although readers should work with inverted codes):

```bash
qrcode --invert "Hello, World!"
```

To create a bitmap `.bmp` file:

```bash
qrcode --output:bmp --scale 8 --file hello.bmp "Hello, World!"
```

<!--
Example use to generate a batch of .SVG files, taking the content from the file and naming each a filename-safe version of the content:

```bash
cat list.txt | while read id; do ./qrcode --output:svg --svg-round 1 --svg-finder-round 1 --svg-point 0.9 --file $(echo "$id" | sed 's/^http\(s\)\{0,1\}:\/\///; s/[^A-Za-z0-9-]/_/g').svg "$id" ; done
```
-->

<!--

## Simple subsets of QR codes

Maximum character count in a V1 (21x21 modules, 26x8 data modules, -8xECC, -4 end, -4 encoding, -count) QR Code, by error-correction level and content type:

| ECC/codewords  |  Full/8b |  AN/Caps |  Numeric |
|:---------------|---------:|---------:|---------:|
| Low (7)        |       17 |       26 |       41 |
| Medium (10)    |       14 |       20 |       34 |
| Quartile (13)  |       11 |       16 |       27 |
| High (17)      |        7 |       10 |       17 |

Alphanumeric is upper-case A-Z, numeric 0-9, or one of the allowed symbols " $%*+-./:".

26 alphanumeric (URL upper-case): HTTPS://XYZXYZ.DEV/-ABCDEF
12 8-bit (MAC hex lower):         abcdefabcdef
12 alphanumeric (MAC hex caps):   ABCDEFABCDEF
15 numeric (MAC decimal):         281474976710655

Top-left finder at 3; top right at (version * 4 + 13, 3); bottom left at (3, version * 4 + 13).
Timing pattern at row/col 6.
V1 has no alignment, V2-V6 has one alignment pattern centered at (version * 4 + 10), V7+ have multiple alignment patterns.
V1-6 have no version info., V7+ do.
Format info (pre-calculated lookup based only on ECL and mask pattern).
Only one error-correction block is needed for low (V1-V5), medium (V1-V3), quartile (V1-V2), high (V1-V2).
Error-correction codewords (per block): low (7/10/15/20/26), medium (10/16/26), quartile (13/22), high (17/28).


-->
