// Generates a QR Code
// Dan Jackson, 2020

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS // This is an example program only
#include <windows.h>
#endif
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "qrcode.h"

typedef enum {
    OUTPUT_TEXT_LARGE,
    OUTPUT_TEXT_NARROW,
    OUTPUT_TEXT_COMPACT,
    OUTPUT_IMAGE_BITMAP,
} output_mode_t;

void OutputQrCodeTextLarge(qrcode_t* qrcode, FILE* fp, int quiet, bool invert)
{
    for (int y = -quiet; y < qrcode->dimension + quiet; y++)
    {
        for (int x = -quiet; x < qrcode->dimension + quiet; x++)
        {
            int bit = QrCodeModuleGet(qrcode, x, y) ^ (invert ? 0x1 : 0x0);
            if (bit != 0) fprintf(fp, "██"); // '\u{2588}' block
            else fprintf(fp, "  ");          // '\u{0020}' space
        }
        fprintf(fp, "\n");
    }
}

void OutputQrCodeTextNarrow(qrcode_t* qrcode, FILE* fp, int quiet, bool invert)
{
    for (int y = -quiet; y < qrcode->dimension + quiet; y++)
    {
        for (int x = -quiet; x < qrcode->dimension + quiet; x++)
        {
            int bit = QrCodeModuleGet(qrcode, x, y) ^ (invert ? 0x1 : 0x0);
            if (bit != 0) fprintf(fp, "█");  // '\u{2588}' block
            else fprintf(fp, " ");           // '\u{0020}' space
        }
        fprintf(fp, "\n");
    }
}

void OutputQrCodeTextCompact(qrcode_t* qrcode, FILE* fp, int quiet, bool invert)
{
    for (int y = -quiet; y < qrcode->dimension + quiet; y += 2)
    {
        for (int x = -quiet; x < qrcode->dimension + quiet; x++)
        {
            int bitU = QrCodeModuleGet(qrcode, x, y);
            int bitL = (y + 1 < qrcode->dimension + quiet) ? QrCodeModuleGet(qrcode, x, y + 1) : (invert ? 0 : 1);
            int value = ((bitL ? 2 : 0) + (bitU ? 1 : 0)) ^ (invert ? 0x3 : 0x0);
            switch (value)
            {
            case 0: fprintf(fp, " "); break; // '\u{0020}' space
            case 1: fprintf(fp, "▀"); break; // '\u{2580}' upper half block
            case 2: fprintf(fp, "▄"); break; // '\u{2584}' lower half block
            case 3: fprintf(fp, "█"); break; // '\u{2588}' block
            }
        }
        fprintf(fp, "\n");
    }
}

// Endian-independent short/long read/write
static void fputshort(uint16_t v, FILE *fp) { fputc((uint8_t)((v >> 0) & 0xff), fp); fputc((uint8_t)((v >> 8) & 0xff), fp); }
static void fputlong(uint32_t v, FILE *fp) { fputc((uint8_t)((v >> 0) & 0xff), fp); fputc((uint8_t)((v >> 8) & 0xff), fp); fputc((uint8_t)((v >> 16) & 0xff), fp); fputc((uint8_t)((v >> 24) & 0xff), fp); }
static void OutputQrCodeImageBitmap(qrcode_t* qrcode, FILE *fp, int dimension, int quiet, int scale, bool invert)
{
    const int BMP_HEADERSIZE = 54;
    const int BMP_PAL_SIZE = 2 * 4;

    int width = (2 * quiet + dimension) * scale;
    int height = (2 * quiet + dimension) * scale;
    int span = ((width + 31) / 32) * 4;
    int bufferSize = span * height;

    fputc('B', fp); fputc('M', fp); // bfType
    fputlong(bufferSize + BMP_HEADERSIZE + BMP_PAL_SIZE, fp); // bfSize
    fputshort(0, fp);              // bfReserved1
    fputshort(0, fp);              // bfReserved2
    fputlong(BMP_HEADERSIZE + BMP_PAL_SIZE, fp); // bfOffBits
    fputlong(40, fp);              // biSize
    fputlong(width, fp);           // biWidth
    fputlong(-height, fp);         // biHeight (negative for top-down)
    fputshort(1, fp);              // biPlanes
    fputshort(1, fp);              // biBitCount
    fputlong(0, fp);               // biCompression
    fputlong(bufferSize, fp);      // biSizeImage
    fputlong(0, fp);               // biXPelsPerMeter 3780
    fputlong(0, fp);               // biYPelsPerMeter 3780
    fputlong(0, fp);               // biClrUsed
    fputlong(0, fp);               // biClrImportant

    // Invert will invert the bit values in the file, but the palette will be swapped so will be invisible in most uses
    // Palette Entry 0 - white (unless inverted)
    fputc(invert ? 0x00 : 0xff, fp); fputc(invert ? 0x00 : 0xff, fp); fputc(invert ? 0x00 : 0xff, fp); fputc(0x00, fp);
    // Palette Entry 1 - black (unless inverted)
    fputc(invert ? 0xff : 0x00, fp); fputc(invert ? 0xff : 0x00, fp); fputc(invert ? 0xff : 0x00, fp); fputc(0x00, fp);
    
    // Bitmap data
    for (int y = 0; y < height; y++)
    {
        for (int h = 0; h < span; h++)
        {
            uint8_t v = 0x00;
            for (int b = 0; b < 8; b++)
            {
                int i = (h * 8 + b) / scale;
                int j = y / scale;
                bool bit = (i < (2 * quiet + dimension)) ? QrCodeModuleGet(qrcode, i - quiet, j - quiet) ^ invert : 0;
                v |= bit << (7 - b);
            }
            fprintf(fp, "%c", v);
        }
    }
}


int main(int argc, char *argv[])
{
    FILE *ofp = stdout;
    const char *value = NULL;
    bool help = false;
    bool invert = false;
    int quiet = QRCODE_QUIET_STANDARD;
    bool mayUppercase = false;
    output_mode_t outputMode = OUTPUT_TEXT_COMPACT;
    qrcode_error_correction_level_t errorCorrectionLevel = QRCODE_ECL_M;
    int scale = 1;
    
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--help")) { help = true; }
        else if (!strcmp(argv[i], "--scale")) { scale = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--ecl:l")) { errorCorrectionLevel = QRCODE_ECL_L; }
        else if (!strcmp(argv[i], "--ecl:m")) { errorCorrectionLevel = QRCODE_ECL_M; }
        else if (!strcmp(argv[i], "--ecl:q")) { errorCorrectionLevel = QRCODE_ECL_Q; }
        else if (!strcmp(argv[i], "--ecl:h")) { errorCorrectionLevel = QRCODE_ECL_H; }
        else if (!strcmp(argv[i], "--quiet")) { quiet = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--invert")) { invert = !invert; }
        else if (!strcmp(argv[i], "--uppercase")) { mayUppercase = true; }
        else if (!strcmp(argv[i], "--file"))
        {
            ofp = fopen(argv[++i], "wb");
            if (ofp == NULL) { fprintf(stderr, "ERROR: Unable to open output filename: %s\n", argv[i]); return -1; }
        }
        else if (!strcmp(argv[i], "--output:large")) { outputMode = OUTPUT_TEXT_LARGE; }
        else if (!strcmp(argv[i], "--output:narrow")) { outputMode = OUTPUT_TEXT_NARROW; }
        else if (!strcmp(argv[i], "--output:compact")) { outputMode = OUTPUT_TEXT_COMPACT; }
        else if (!strcmp(argv[i], "--output:bmp")) { outputMode = OUTPUT_IMAGE_BITMAP; }
        else if (argv[i][0] == '-')
        {
            fprintf(stderr, "ERROR: Unrecognized parameter: %s\n", argv[i]); 
            help = true;
            break;
        }
        else if (value == NULL)
        {
            value = argv[i];
        }
        else
        {
            fprintf(stderr, "ERROR: Unrecognized positional parameter: %s\n", argv[i]); 
            help = true;
            break;
        }
    }

    if (value == NULL)
    {
        fprintf(stderr, "ERROR: Value not specified.\n"); 
        help = true;
    }

    if (help)
    {
        fprintf(stderr, "USAGE: qrcode [--ecl:<l|m|q|h>] [--uppercase] [--invert] [[--output:<large|narrow|compact>] | --output:bmp --scale 1] [--quiet 4] [--file filename] <value>\n"); 
        return -1;
    }

    // Clean QR Code object
    qrcode_t qrcode;
    QrCodeInit(&qrcode, QRCODE_VERSION_MAX, errorCorrectionLevel);

    // Add one text segment
    qrcode_segment_t segment;
    QrCodeSegmentAppend(&qrcode, &segment, QRCODE_MODE_INDICATOR_AUTOMATIC, value, QRCODE_TEXT_LENGTH, mayUppercase);

    // Gets required buffer sizes
    size_t bufferSize = 0;
    size_t scratchBufferSize = 0;
    int dimension = QrCodeSize(&qrcode, &bufferSize, &scratchBufferSize);

    // Generates the QR Code as a bitmap (0=light, 1=dark) using the specified buffer.
    uint8_t *buffer = malloc(bufferSize);
    uint8_t *scratchBuffer = malloc(scratchBufferSize);
    bool result = QrCodeGenerate(&qrcode, buffer, scratchBuffer);
    if (result)
    {
#ifdef _WIN32
        if (outputMode == OUTPUT_TEXT_LARGE || outputMode == OUTPUT_TEXT_NARROW || outputMode == OUTPUT_TEXT_COMPACT) SetConsoleOutputCP(CP_UTF8);
#endif
        switch (outputMode)
        {
            case OUTPUT_TEXT_LARGE: OutputQrCodeTextLarge(&qrcode, stdout, quiet, true); break;
            case OUTPUT_TEXT_NARROW: OutputQrCodeTextNarrow(&qrcode, stdout, quiet, true); break;
            case OUTPUT_TEXT_COMPACT: OutputQrCodeTextCompact(&qrcode, stdout, quiet, true); break;
            case OUTPUT_IMAGE_BITMAP: OutputQrCodeImageBitmap(&qrcode, ofp, dimension, quiet, scale, invert); break;
            default: fprintf(ofp, "<error>"); break;
        }
    }
    else
    {
        fprintf(stderr, "ERROR: Could not generate QR Code (too much data).\n");
    }

    if (ofp != stdout) fclose(ofp);
    return 0;
}
