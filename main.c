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

/*
#define DEFAULT_HEIGHT 5

typedef enum {
    OUTPUT_TEXT_WIDE,
    OUTPUT_TEXT_NARROW,
    OUTPUT_IMAGE_BITMAP,
} output_mode_t;

// Endian-independent short/long read/write
static void fputshort(uint16_t v, FILE *fp) { fputc((uint8_t)((v >> 0) & 0xff), fp); fputc((uint8_t)((v >> 8) & 0xff), fp); }
static void fputlong(uint32_t v, FILE *fp) { fputc((uint8_t)((v >> 0) & 0xff), fp); fputc((uint8_t)((v >> 8) & 0xff), fp); fputc((uint8_t)((v >> 16) & 0xff), fp); fputc((uint8_t)((v >> 24) & 0xff), fp); }

static void OutputQrCodeTextWide(FILE *fp, uint8_t *bitmap, size_t length, int scale, int height, bool invert)
{
    for (int repeat = 0; repeat < height; repeat++)
    {
        for (int i = 0; i < scale * length; i++)
        {
            bool bit = BARCODE_BIT(bitmap, i / scale) ^ invert;
            fprintf(fp, "%s", bit ? "█" : " ");	// \u{2588} block
        }
        fprintf(fp, "\n");
    }
}

static void OutputQrCodeTextNarrow(FILE *fp, uint8_t *bitmap, size_t length, int scale, int height, bool invert)
{
    for (int repeat = 0; repeat < height; repeat++)
    {
        for (int i = 0; i < scale * length; i += 2)
        {
            bool bit0 = BARCODE_BIT(bitmap, i / scale);
            bool bit1 = ((i + 1) / scale) < length ? BARCODE_BIT(bitmap, (i + 1) / scale) : !invert;
            int value = ((bit0 ? 2 : 0) + (bit1 ? 1 : 0)) ^ (invert ? 0x3 : 0x0);
            switch (value)
            {
                case 0: fprintf(fp, " "); break; // '\u{0020}' space
                case 1: fprintf(fp, "▐"); break; // '\u{2590}' right
                case 2: fprintf(fp, "▌"); break; // '\u{258C}' left
                case 3: fprintf(fp, "█"); break; // '\u{2588}' block
            }
        }
        fprintf(fp, "\n");
    }
}


static void OutputQrCodeImageBitmap(FILE *fp, uint8_t *bitmap, size_t length, int scale, int height, bool invert)
{
    const int BMP_HEADERSIZE = 54;
    const int BMP_PAL_SIZE = 2 * 4;

    int width = length * scale;
    int span = ((width + 31) / 32) * 4;
    int bufferSize = span * height;

    fputc('B', fp); fputc('M', fp); // bfType
    fputlong(bufferSize + BMP_HEADERSIZE + BMP_PAL_SIZE, fp); // bfSize
    fputshort(0, fp);              // bfReserved1
    fputshort(0, fp);              // bfReserved2
    fputlong(BMP_HEADERSIZE + BMP_PAL_SIZE, fp); // bfOffBits
    fputlong(40, fp);              // biSize
    fputlong(width, fp);           // biWidth
    fputlong(height, fp);          // biHeight (negative for top-down)
    fputshort(1, fp);              // biPlanes
    fputshort(1, fp);              // biBitCount
    fputlong(0, fp);               // biCompression
    fputlong(bufferSize, fp);      // biSizeImage
    fputlong(0, fp);               // biXPelsPerMeter 3780
    fputlong(0, fp);               // biYPelsPerMeter 3780
    fputlong(0, fp);               // biClrUsed
    fputlong(0, fp);               // biClrImportant
    // Palette Entry 0 - white
    fputc(0xff, fp); fputc(0xff, fp); fputc(0xff, fp); fputc(0x00, fp);
    // Palette Entry 1 - black
    fputc(0x00, fp); fputc(0x00, fp); fputc(0x00, fp); fputc(0x00, fp);
    
    // Bitmap data
    for (int y = 0; y < height; y++)
    {
        for (int h = 0; h < span; h++)
        {
            uint8_t v = 0x00;
            for (int b = 0; b < 8; b++)
            {
                int i = (h * 8 + b) / scale;
                bool bit = (i < length) ? BARCODE_BIT(bitmap, i) ^ invert : 0;
                v |= bit << (7 - b);
            }
            fprintf(fp, "%c", v);
        }
    }
}
*/

int main(int argc, char *argv[])
{
    FILE *ofp = stdout;
    const char *value = NULL;
    bool help = false;
    bool invert = false;
    int quiet = QRCODE_QUIET_STANDARD;
    //output_mode_t outputMode = OUTPUT_TEXT_NARROW;
    //int scale = 1;
    //int height = DEFAULT_HEIGHT;
    
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--help")) { help = true; }
        //else if (!strcmp(argv[i], "--height")) { height = atoi(argv[++i]); }
        //else if (!strcmp(argv[i], "--scale")) { scale = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--quiet")) { quiet = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--invert")) { invert = !invert; }
        else if (!strcmp(argv[i], "--file"))
        {
            ofp = fopen(argv[++i], "wb");
            if (ofp == NULL) { fprintf(stderr, "ERROR: Unable to open output filename: %s\n", argv[i]); return -1; }
        }
        //else if (!strcmp(argv[i], "--output:wide")) { outputMode = OUTPUT_TEXT_WIDE; }
        //else if (!strcmp(argv[i], "--output:narrow")) { outputMode = OUTPUT_TEXT_NARROW; }
        //else if (!strcmp(argv[i], "--output:bmp")) { outputMode = OUTPUT_IMAGE_BITMAP; }
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
        // [--scale 1] [--output:<wide|narrow|bmp>]
        fprintf(stderr, "USAGE: qrcode [--invert] [--quiet 4] [--file filename] <value>\n"); 
        return -1;
    }

    // Generates the QR Code as a bitmap (0=light, 1=dark) using the specified buffer.
    qrcode_t qrcode;
    QrCodeInit(&qrcode);
    unsigned int size = QRCODE_VERSION_TO_SIZE(7);
    size_t bufferSize = (size_t)size * (size_t)size;  // TODO: Final buffer is in bits (specify span) and use quiet margin
    uint8_t *buffer = malloc(bufferSize);
    QrCodeSetBuffer(&qrcode, size, buffer, bufferSize);
qrcode.quiet = quiet;
    bool result = QrCodeGenerate(&qrcode, value);

    printf("QRCODE: %s\n", result ? "success" : "fail");

/*
#ifdef _WIN32
    if (outputMode == OUTPUT_TEXT_WIDE || outputMode == OUTPUT_TEXT_NARROW) SetConsoleOutputCP(CP_UTF8);
#endif

    switch (outputMode)
    {
        case OUTPUT_TEXT_WIDE: OutputQrCodeTextWide(ofp, bitmap, length, scale, height, invert); break;
        case OUTPUT_TEXT_NARROW: OutputQrCodeTextNarrow(ofp, bitmap, length, scale, height, invert); break;
        case OUTPUT_IMAGE_BITMAP: OutputQrCodeImageBitmap(ofp, bitmap, length, scale, height, invert); break;
        default: fprintf(ofp, "<error>"); break;
    }
*/

    if (ofp != stdout) fclose(ofp);
    return 0;
}
