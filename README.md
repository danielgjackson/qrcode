# QR Code

Generates a QR Code pattern.

For a tiny "V1" QR Code generator, with low RAM overhead, suitable for embedded contexts, see: [qrtiny](https://github.com/danielgjackson/qrtiny). For a JavaScript version, see: [qrcodejs](https://github.com/danielgjackson/qrcodejs).


## Create a QR Code

Required files: [`qrcode.h`](qrcode.h) [`qrcode.c`](qrcode.c)

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


## Build

### Windows

On Windows, you can build an `qrcode.exe` binary in the current directory.  You will need *Visual Studio*, or the [Build Tools for Visual Studio](https://aka.ms/buildtools) (*All Downloads* / *Tools for Visual Studio* / *Build Tools for Visual Studio*).  <!-- Ensure you have the compilers installed: *Individual Components* / *Compilers, build tools, and runtimes* / *MSVC v143 - VS 2022 C++ x64/x86 build tools (Latest)* and *MSVC v143 - VS 2022 C++ x64/x86 Spectre-mitigated libs (Latest)*. --> Download and build with:

```cmd
powershell -Command "& {Invoke-WebRequest https://github.com/danielgjackson/qrcode/archive/master.zip -o qrcode.build.zip ; Expand-Archive qrcode.build.zip ; del qrcode.build.zip ; qrcode.build/qrcode-master/build.cmd ; copy qrcode.build/qrcode-master/qrcode.exe . }"
```

Alternatively, you can clone this repository and run: `build.cmd`


### macOS, Linux, WSL

On non-Windows operating systems, you can use this single line command to build an `qrcode` binary in the current directory:

```bash
mkdir qrcode.build && curl -L https://github.com/danielgjackson/qrcode/archive/master.zip -o qrcode.build/master.zip && unzip qrcode.build/master.zip -d qrcode.build && make -C qrcode.build/qrcode-master && cp qrcode.build/qrcode-master/qrcode .
```

Alternatively, you can clone this repository and run: `make -C src/qrcode`



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


