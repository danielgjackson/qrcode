// Generates a QR Code
// Dan Jackson, 2020

#ifdef _WIN32
#pragma setlocale(".65001") // pseudo-locale doesn't really work? Save with UTF-8 BOM
#pragma execution_character_set("utf-8")
#define _CRT_SECURE_NO_WARNINGS // This is an example program only
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

//#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "qrcode.h"

typedef enum {
    OUTPUT_TEXT,
    OUTPUT_BITMAP,
    OUTPUT_SVG,
    OUTPUT_SIXEL,
} output_mode_t;


typedef struct
{
    int cellW;
    int cellH;
    const char *text[];
} text_render_t;

text_render_t textRenderAscii =
{
    1, 1,
    { "  ", "##" }
};

text_render_t textRenderLarge =
{
    1, 1,
    { "  ", "\xE2\x96\x88\xE2\x96\x88" }  // ██
};

text_render_t textRenderNarrow =
{
    1, 1,
    { " ", "\xE2\x96\x88" }            // '\u{0020}' space, '\u{2588}' █ block
};

text_render_t textRenderMedium =
{
    1, 2,
    { " ", "\xE2\x96\x80", "\xE2\x96\x84", "\xE2\x96\x88" }  // '\u{0020}' space, '\u{2580}' ▀ upper half block, '\u{2584}' ▄ lower half block, '\u{2588}' █ block
};

text_render_t textRenderCompact =
{
    2, 2,
    {
                   " ", "\xE2\x96\x98", "\xE2\x96\x9D", "\xE2\x96\x80", // " ", "▘", "▝", "▀",
        "\xE2\x96\x96", "\xE2\x96\x8C", "\xE2\x96\x9E", "\xE2\x96\x9B", // "▖", "▌", "▞", "▛",
        "\xE2\x96\x97", "\xE2\x96\x9A", "\xE2\x96\x90", "\xE2\x96\x9C", // "▗", "▚", "▐", "▜",
        "\xE2\x96\x84", "\xE2\x96\x99", "\xE2\x96\x9F", "\xE2\x96\x88", // "▄", "▙", "▟", "█"
    }
};

// This output type requires "BLOCK SEXTANT" codes from "Symbols For Legacy Computing", part of Unicode 13.
text_render_t textRenderTiny =
{
    2, 3,
    {
                 /*" "*/                " ", /*"\U0001FB00"*/ "\xf0\x9f\xac\x80", /*"\U0001FB01"*/ "\xf0\x9f\xac\x81", /*"\U0001FB02"*/ "\xf0\x9f\xac\x82", // 00/00/00, 10/00/00, 01/00/00, 11/00/00, 
        /*"\U0001FB03"*/ "\xf0\x9f\xac\x83", /*"\U0001FB04"*/ "\xf0\x9f\xac\x84", /*"\U0001FB05"*/ "\xf0\x9f\xac\x85", /*"\U0001FB06"*/ "\xf0\x9f\xac\x86", // 00/10/00, 10/10/00, 01/10/00, 11/10/00, 
        /*"\U0001FB07"*/ "\xf0\x9f\xac\x87", /*"\U0001FB08"*/ "\xf0\x9f\xac\x88", /*"\U0001FB09"*/ "\xf0\x9f\xac\x89", /*"\U0001FB0A"*/ "\xf0\x9f\xac\x8a", // 00/01/00, 10/01/00, 01/01/00, 11/01/00, 
        /*"\U0001FB0B"*/ "\xf0\x9f\xac\x8b", /*"\U0001FB0C"*/ "\xf0\x9f\xac\x8c", /*"\U0001FB0D"*/ "\xf0\x9f\xac\x8d", /*"\U0001FB0E"*/ "\xf0\x9f\xac\x8e", // 00/11/00, 10/11/00, 01/11/00, 11/11/00, 
        /*"\U0001FB0F"*/ "\xf0\x9f\xac\x8f", /*"\U0001FB10"*/ "\xf0\x9f\xac\x90", /*"\U0001FB11"*/ "\xf0\x9f\xac\x91", /*"\U0001FB12"*/ "\xf0\x9f\xac\x92", // 00/00/10, 10/00/10, 01/00/10, 11/00/10, 
        /*"\U0001FB13"*/ "\xf0\x9f\xac\x93",          /*"▌"*/     "\xE2\x96\x8C", /*"\U0001FB14"*/ "\xf0\x9f\xac\x94", /*"\U0001FB15"*/ "\xf0\x9f\xac\x95", // 00/10/10, 10/10/10, 01/10/10, 11/10/10, 
        /*"\U0001FB16"*/ "\xf0\x9f\xac\x96", /*"\U0001FB17"*/ "\xf0\x9f\xac\x97", /*"\U0001FB18"*/ "\xf0\x9f\xac\x98", /*"\U0001FB19"*/ "\xf0\x9f\xac\x99", // 00/01/10, 10/01/10, 01/01/10, 11/01/10, 
        /*"\U0001FB1A"*/ "\xf0\x9f\xac\x9a", /*"\U0001FB1B"*/ "\xf0\x9f\xac\x9b", /*"\U0001FB1C"*/ "\xf0\x9f\xac\x9c", /*"\U0001FB1D"*/ "\xf0\x9f\xac\x9d", // 00/11/10, 10/11/10, 01/11/10, 11/11/10, 
        /*"\U0001FB1E"*/ "\xf0\x9f\xac\x9e", /*"\U0001FB1F"*/ "\xf0\x9f\xac\x9f", /*"\U0001FB20"*/ "\xf0\x9f\xac\xa0", /*"\U0001FB21"*/ "\xf0\x9f\xac\xa1", // 00/00/01, 10/00/01, 01/00/01, 11/00/01, 
        /*"\U0001FB22"*/ "\xf0\x9f\xac\xa2", /*"\U0001FB23"*/ "\xf0\x9f\xac\xa3", /*"\U0001FB24"*/ "\xf0\x9f\xac\xa4", /*"\U0001FB25"*/ "\xf0\x9f\xac\xa5", // 00/10/01, 10/10/01, 01/10/01, 11/10/01, 
        /*"\U0001FB26"*/ "\xf0\x9f\xac\xa6", /*"\U0001FB27"*/ "\xf0\x9f\xac\xa7",          /*"▐"*/     "\xE2\x96\x90", /*"\U0001FB28"*/ "\xf0\x9f\xac\xa8", // 00/01/01, 10/01/01, 01/01/01, 11/01/01, 
        /*"\U0001FB29"*/ "\xf0\x9f\xac\xa9", /*"\U0001FB2A"*/ "\xf0\x9f\xac\xaa", /*"\U0001FB2B"*/ "\xf0\x9f\xac\xab", /*"\U0001FB2C"*/ "\xf0\x9f\xac\xac", // 00/11/01, 10/11/01, 01/11/01, 11/11/01, 
        /*"\U0001FB2D"*/ "\xf0\x9f\xac\xad", /*"\U0001FB2E"*/ "\xf0\x9f\xac\xae", /*"\U0001FB2F"*/ "\xf0\x9f\xac\xaf", /*"\U0001FB30"*/ "\xf0\x9f\xac\xb0", // 00/00/11, 10/00/11, 01/00/11, 11/00/11, 
        /*"\U0001FB31"*/ "\xf0\x9f\xac\xb1", /*"\U0001FB32"*/ "\xf0\x9f\xac\xb2", /*"\U0001FB33"*/ "\xf0\x9f\xac\xb3", /*"\U0001FB34"*/ "\xf0\x9f\xac\xb4", // 00/10/11, 10/10/11, 01/10/11, 11/10/11, 
        /*"\U0001FB35"*/ "\xf0\x9f\xac\xb5", /*"\U0001FB36"*/ "\xf0\x9f\xac\xb6", /*"\U0001FB37"*/ "\xf0\x9f\xac\xb7", /*"\U0001FB38"*/ "\xf0\x9f\xac\xb8", // 00/01/11, 10/01/11, 01/01/11, 11/01/11, 
        /*"\U0001FB39"*/ "\xf0\x9f\xac\xb9", /*"\U0001FB3A"*/ "\xf0\x9f\xac\xba", /*"\U0001FB3B"*/ "\xf0\x9f\xac\xbb",          /*"█"*/     "\xE2\x96\x88", // 00/11/11, 10/11/11, 01/11/11, 11/11/11, 
    }
};


text_render_t textRenderDots =
{
    2, 4,
    {
        "⠀", "⠁", "⠈", "⠉", "⠂", "⠃", "⠊", "⠋", "⠐", "⠑", "⠘", "⠙", "⠒", "⠓", "⠚", "⠛",
        "⠄", "⠅", "⠌", "⠍", "⠆", "⠇", "⠎", "⠏", "⠔", "⠕", "⠜", "⠝", "⠖", "⠗", "⠞", "⠟",
        "⠠", "⠡", "⠨", "⠩", "⠢", "⠣", "⠪", "⠫", "⠰", "⠱", "⠸", "⠹", "⠲", "⠳", "⠺", "⠻",
        "⠤", "⠥", "⠬", "⠭", "⠦", "⠧", "⠮", "⠯", "⠴", "⠵", "⠼", "⠽", "⠶", "⠷", "⠾", "⠿",
        "⡀", "⡁", "⡈", "⡉", "⡂", "⡃", "⡊", "⡋", "⡐", "⡑", "⡘", "⡙", "⡒", "⡓", "⡚", "⡛",
        "⡄", "⡅", "⡌", "⡍", "⡆", "⡇", "⡎", "⡏", "⡔", "⡕", "⡜", "⡝", "⡖", "⡗", "⡞", "⡟",
        "⡠", "⡡", "⡨", "⡩", "⡢", "⡣", "⡪", "⡫", "⡰", "⡱", "⡸", "⡹", "⡲", "⡳", "⡺", "⡻",
        "⡤", "⡥", "⡬", "⡭", "⡦", "⡧", "⡮", "⡯", "⡴", "⡵", "⡼", "⡽", "⡶", "⡷", "⡾", "⡿",
        "⢀", "⢁", "⢈", "⢉", "⢂", "⢃", "⢊", "⢋", "⢐", "⢑", "⢘", "⢙", "⢒", "⢓", "⢚", "⢛",
        "⢄", "⢅", "⢌", "⢍", "⢆", "⢇", "⢎", "⢏", "⢔", "⢕", "⢜", "⢝", "⢖", "⢗", "⢞", "⢟",
        "⢠", "⢡", "⢨", "⢩", "⢢", "⢣", "⢪", "⢫", "⢰", "⢱", "⢸", "⢹", "⢲", "⢳", "⢺", "⢻",
        "⢤", "⢥", "⢬", "⢭", "⢦", "⢧", "⢮", "⢯", "⢴", "⢵", "⢼", "⢽", "⢶", "⢷", "⢾", "⢿",
        "⣀", "⣁", "⣈", "⣉", "⣂", "⣃", "⣊", "⣋", "⣐", "⣑", "⣘", "⣙", "⣒", "⣓", "⣚", "⣛",
        "⣄", "⣅", "⣌", "⣍", "⣆", "⣇", "⣎", "⣏", "⣔", "⣕", "⣜", "⣝", "⣖", "⣗", "⣞", "⣟",
        "⣠", "⣡", "⣨", "⣩", "⣢", "⣣", "⣪", "⣫", "⣰", "⣱", "⣸", "⣹", "⣲", "⣳", "⣺", "⣻",
        "⣤", "⣥", "⣬", "⣭", "⣦", "⣧", "⣮", "⣯", "⣴", "⣵", "⣼", "⣽", "⣶", "⣷", "⣾", "⣿",
    }
};


text_render_t textRenderDots6 =
{
    2, 3,
    {
                   " ", "\xe2\xa0\x81", "\xe2\xa0\x88", "\xe2\xa0\x89", "\xe2\xa0\x82", "\xe2\xa0\x83", "\xe2\xa0\x8a", "\xe2\xa0\x8b", "\xe2\xa0\x90", "\xe2\xa0\x91", "\xe2\xa0\x98", "\xe2\xa0\x99", "\xe2\xa0\x92", "\xe2\xa0\x93", "\xe2\xa0\x9a", "\xe2\xa0\x9b", // "⠀", "⠁", "⠈", "⠉", "⠂", "⠃", "⠊", "⠋", "⠐", "⠑", "⠘", "⠙", "⠒", "⠓", "⠚", "⠛",
        "\xe2\xa0\x84", "\xe2\xa0\x85", "\xe2\xa0\x8c", "\xe2\xa0\x8d", "\xe2\xa0\x86", "\xe2\xa0\x87", "\xe2\xa0\x8e", "\xe2\xa0\x8f", "\xe2\xa0\x94", "\xe2\xa0\x95", "\xe2\xa0\x9c", "\xe2\xa0\x9d", "\xe2\xa0\x96", "\xe2\xa0\x97", "\xe2\xa0\x9e", "\xe2\xa0\x9f", // "⠄", "⠅", "⠌", "⠍", "⠆", "⠇", "⠎", "⠏", "⠔", "⠕", "⠜", "⠝", "⠖", "⠗", "⠞", "⠟",
        "\xe2\xa0\xa0", "\xe2\xa0\xa1", "\xe2\xa0\xa8", "\xe2\xa0\xa9", "\xe2\xa0\xa2", "\xe2\xa0\xa3", "\xe2\xa0\xaa", "\xe2\xa0\xab", "\xe2\xa0\xb0", "\xe2\xa0\xb1", "\xe2\xa0\xb8", "\xe2\xa0\xb9", "\xe2\xa0\xb2", "\xe2\xa0\xb3", "\xe2\xa0\xba", "\xe2\xa0\xbb", // "⠠", "⠡", "⠨", "⠩", "⠢", "⠣", "⠪", "⠫", "⠰", "⠱", "⠸", "⠹", "⠲", "⠳", "⠺", "⠻",
        "\xe2\xa0\xa4", "\xe2\xa0\xa5", "\xe2\xa0\xac", "\xe2\xa0\xad", "\xe2\xa0\xa6", "\xe2\xa0\xa7", "\xe2\xa0\xae", "\xe2\xa0\xaf", "\xe2\xa0\xb4", "\xe2\xa0\xb5", "\xe2\xa0\xbc", "\xe2\xa0\xbd", "\xe2\xa0\xb6", "\xe2\xa0\xb7", "\xe2\xa0\xbe", "\xe2\xa0\xbf", // "⠤", "⠥", "⠬", "⠭", "⠦", "⠧", "⠮", "⠯", "⠴", "⠵", "⠼", "⠽", "⠶", "⠷", "⠾", "⠿",
    }
};


static void OutputQrCodeText(qrcode_t *qrcode, FILE *fp, int dimension, const text_render_t *t, int quiet, bool invert)
{
    for (int y = -quiet; y < dimension + quiet; y += t->cellH)
    {
        for (int x = -quiet; x < dimension + quiet; x += t->cellW)
        {
            int value = 0;
            value |= (QrCodeModuleGet(qrcode, x, y) & 1) ? 0x01 : 0x00;
            for (int yy = 0; yy < t->cellH; yy++)
            {
                for (int xx = 0; xx < t->cellW; xx++)
                {
                    int isSet = invert ? 1 : 0;
                    if (x + xx < dimension + quiet && y + yy < dimension + quiet)
                    {
                        isSet = QrCodeModuleGet(qrcode, x + xx, y + yy) & 1;
                    }
                    if (isSet) value |= (1 << (yy * t->cellW + xx));
                }
            }
            if (invert)
            {
                value ^= ((1 << (t->cellW * t->cellH)) - 1);
            }
            fprintf(fp, "%s", t->text[value]);
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
                bool bit = (i < (2 * quiet + dimension)) ? (QrCodeModuleGet(qrcode, i - quiet, j - quiet) & 1) ^ invert : 0;
                v |= bit << (7 - b);
            }
            fprintf(fp, "%c", v);
        }
    }
}

static void OutputQrCodeImageSvg(qrcode_t* qrcode, FILE *fp, int dimension, int quiet, bool invert, char *color, double moduleSize, double moduleRound, bool finderPart, double finderRound, bool alignmentPart, double alignmentRound)
{
    const bool xlink = true;     // Use "xlink:" prefix on "href" for wider compatibility
    const bool white = false;    // Output an element for every module, not just black/dark ones but white/light ones too.

    fprintf(fp, "<?xml version=\"1.0\"?>\n");
    // viewport-fill=\"white\" 
    fprintf(fp, "<svg xmlns=\"http://www.w3.org/2000/svg\"%s fill=\"%s\" viewBox=\"-%d.5 -%d.5 %d %d\" shape-rendering=\"crispEdges\">\n", xlink ? " xmlns:xlink=\"http://www.w3.org/1999/xlink\"" : "", color, quiet, quiet, 2 * quiet + dimension, 2 * quiet + dimension);
    //fprintf(fp, "<desc>%s</desc>\n", data);
    fprintf(fp, "<defs>\n");

    // module data bit (dark)
    fprintf(fp, "<rect id=\"b\" x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\" rx=\"%f\" />\n", -moduleSize / 2, -moduleSize / 2, moduleSize, moduleSize, 0.5f * moduleRound * moduleSize);
    // module data bit (light). 
    if (white)
    {
        // Light modules as a ref to a placeholder empty element
        fprintf(fp, "<path id=\"w\" d=\"\" visibility=\"hidden\" />\n");

        // Light modules similar to the dark ones: fill-opacity=\"0\" fill=\"white\" fill=\"rgba(255,255,255,0)\"
        //fprintf(fp, "<rect id=\"w\" x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\" rx=\"%f\" fill-opacity=\"0\" />\n", -moduleSize / 2, -moduleSize / 2, moduleSize, moduleSize, 0.5f * moduleRound * moduleSize);

        // Light modules as a variation of the dark ones: fill-opacity=\"0\" fill=\"white\" fill=\"rgba(255,255,255,0)\"
        //fprintf(fp, "<use id=\"w\" %shref=\"#b\" fill-opacity=\"0\" />\n", xlink ? "xlink:" : "");
    }

    // Use one item for the finder marker
    if (finderPart)
    {
        // Hide finder module, use finder part
        fprintf(fp, "<path id=\"f\" d=\"\" visibility=\"hidden\" />\n");
        if (white) fprintf(fp, "<path id=\"fw\" d=\"\" visibility=\"hidden\" />\n");
        fprintf(fp, "<g id=\"fc\"><rect x=\"-3\" y=\"-3\" width=\"6\" height=\"6\" rx=\"%f\" stroke=\"%s\" stroke-width=\"1\" fill=\"none\" /><rect x=\"-1.5\" y=\"-1.5\" width=\"3\" height=\"3\" rx=\"%f\" /></g>\n", 3.0f * finderRound, color, 1.5f * finderRound);
    }
    else
    {
        // Use normal module for finder module, hide finder part
        fprintf(fp, "<use id=\"f\" %shref=\"#b\" />\n", xlink ? "xlink:" : "");
        if (white) fprintf(fp, "<use id=\"fw\" %shref=\"#w\" />\n", xlink ? "xlink:" : "");
        fprintf(fp, "<path id=\"fc\" d=\"\" visibility=\"hidden\" />\n");
    }

    // Use one item for the alignment marker
    if (alignmentPart)
    {
        // Hide alignment module, use alignment part
        fprintf(fp, "<path id=\"a\" d=\"\" visibility=\"hidden\" />\n");
        if (white) fprintf(fp, "<path id=\"aw\" d=\"\" visibility=\"hidden\" />\n");
        fprintf(fp, "<g id=\"ac\"><rect x=\"-2\" y=\"-2\" width=\"4\" height=\"4\" rx=\"%f\" stroke=\"%s\" stroke-width=\"1\" fill=\"none\" /><rect x=\"-0.5\" y=\"-0.5\" width=\"1\" height=\"1\" rx=\"%f\" /></g>\n", 2.0f * alignmentRound, color, 0.5f * alignmentRound);
    }
    else
    {
        // Use normal module for alignment module, hide alignment part
        fprintf(fp, "<use id=\"a\" %shref=\"#b\" />\n", xlink ? "xlink:" : "");
        if (white) fprintf(fp, "<use id=\"aw\" %shref=\"#w\" />\n", xlink ? "xlink:" : "");
        fprintf(fp, "<path id=\"ac\" d=\"\" visibility=\"hidden\" />\n");
    }

    fprintf(fp, "</defs>\n");

    for (int y = 0; y < dimension; y++)
    {
        for (int x = 0; x < dimension; x++)
        {
            qrcode_part_t part = QrCodeIdentifyModule(qrcode, x, y, NULL);
            bool bit = ((QrCodeModuleGet(qrcode, x, y) & 1) ^ invert) & 1;
            char* type = bit ? "b" : "w";

            // Draw finder/alignment as modules (define to nothing if drawing as whole parts)
            if (part == QRCODE_PART_FINDER) { type = bit ? "f" : "fw"; }
            else if (part == QRCODE_PART_ALIGNMENT) { type = bit ? "a" : "aw"; }

            if (bit || white) fprintf(fp, "<use x=\"%d\" y=\"%d\" %shref=\"#%s\" />\n", x, y, xlink ? "xlink:" : "", type);
        }
    }

    // Draw finder/alignment as whole parts (define to nothing if drawing as modules)
    for (int y = 0; y < dimension; y++)
    {
        for (int x = 0; x < dimension; x++)
        {
            int index;
            char* type = NULL;
            qrcode_part_t part = QrCodeIdentifyModule(qrcode, x, y, &index);
            if (part == QRCODE_PART_FINDER && index == -1) type = "fc";
            if (part == QRCODE_PART_ALIGNMENT && index == -1) type = "ac";
            if (type == NULL) continue;
            fprintf(fp, "<use x=\"%d\" y=\"%d\" %shref=\"#%s\" />\n", x, y, xlink ? "xlink:" : "", type);
        }
    }

    fprintf(fp, "</svg>\n");
}


static void OutputQrCodeSixel(qrcode_t *qrcode, FILE *fp, int dimension, int quiet, int scale, bool invert)
{
    const int LINE_HEIGHT = 6;
    // Enter sixel mode
    fprintf(fp, "\x1BP7;1q");    // 1:1 ratio, 0 pixels remain at current color
    // Set color map
    fprintf(fp, "#0;2;0;0;0");       // Background
    fprintf(fp, "#1;2;100;100;100");
    for (int y = -quiet * scale; y < (dimension + quiet) * scale; y += LINE_HEIGHT)
    {
        const int passes = 2;
        for (int pass = 0; pass < passes; pass++)
        {
            // Start a pass in a specific color
            fprintf(fp, "#%d", pass);
            // Line data
            for (int x = -quiet * scale; x < (dimension + quiet) * scale; x += scale)
            {
                int value = 0;
                for (int yy = 0; yy < LINE_HEIGHT; yy++) {
                    int cx = (x < 0 ? x - scale + 1 : x) / scale;
                    int cy = (y + yy < 0 ? y + yy - scale + 1 : y + yy) / scale;
//if (x == 0 && pass == 0) printf("y=%d, yy=%d, y+yy=%d, scale=%d, cx=%d cy=%d\n", y, yy, y + yy, scale, cx, cy);
                    int module = (QrCodeModuleGet(qrcode, cx, cy) & 1) ? 0x00 : 0x01;
                    if (invert) module = 1 - module;
                    int bit = (module == pass) ? 1 : 0;
                    value |= (bit ? 0x01 : 0x00) << yy;
                }
                // Six pixels strip at 'scale' (repeated) width
                if (scale == 1) {
                    fprintf(fp, "%c", value + 63);
                } else if (scale == 2) {
                    fprintf(fp, "%c%c", value + 63, value + 63);
                } else if (scale == 3) {
                    fprintf(fp, "%c%c%c", value + 63, value + 63, value + 63);
                } else if (scale > 3) {
                    fprintf(fp, "!%d%c", scale, value + 63);
                }
            }
            // Return to start of the line
            if (pass + 1 < passes) {
                fprintf(fp, "$");
            }
        }
        // Next line
        if (y + LINE_HEIGHT < (dimension + quiet) * scale) {
            fprintf(fp, "-");
        }
    }
    // Exit sixel mode
    fprintf(fp, "\x1B\\");
    fprintf(fp, "\n");
}


int main(int argc, char *argv[])
{
    FILE *ofp = stdout;
    const char *value = NULL;
    bool help = false;
    bool invert = false;
    int quiet = QRCODE_QUIET_STANDARD;
    bool mayUppercase = false;
    output_mode_t outputMode = OUTPUT_TEXT;
    const text_render_t *textRender = &textRenderMedium;
    qrcode_error_correction_level_t errorCorrectionLevel = QRCODE_ECL_M;
    qrcode_mask_pattern_t maskPattern = QRCODE_MASK_AUTO;
    int version = QRCODE_VERSION_AUTO;
    bool optimizeEcc = true;
    int scale = 4;
    // SVG details
    char *color = "currentColor";
    double moduleSize = 1.0f;
    double moduleRound = 0.0f;
    bool finderPart = false;
    double finderRound = 0.0f;
    bool alignmentPart = false;
    double alignmentRound = 0.0f;

    
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--help")) { help = true; }
        else if (!strcmp(argv[i], "--ecl:l")) { errorCorrectionLevel = QRCODE_ECL_L; }
        else if (!strcmp(argv[i], "--ecl:m")) { errorCorrectionLevel = QRCODE_ECL_M; }
        else if (!strcmp(argv[i], "--ecl:q")) { errorCorrectionLevel = QRCODE_ECL_Q; }
        else if (!strcmp(argv[i], "--ecl:h")) { errorCorrectionLevel = QRCODE_ECL_H; }
        else if (!strcmp(argv[i], "--fixecl")) { optimizeEcc = false; }
        else if (!strcmp(argv[i], "--version")) { version = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--mask")) { maskPattern = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--quiet")) { quiet = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--invert")) { invert = !invert; }
        else if (!strcmp(argv[i], "--uppercase")) { mayUppercase = true; }
        else if (!strcmp(argv[i], "--file"))
        {
            ofp = fopen(argv[++i], "wb");
            if (ofp == NULL) { fprintf(stderr, "ERROR: Unable to open output filename: %s\n", argv[i]); return -1; }
        }
        else if (!strcmp(argv[i], "--output:ascii")) { outputMode = OUTPUT_TEXT; textRender = &textRenderAscii; }
        else if (!strcmp(argv[i], "--output:large")) { outputMode = OUTPUT_TEXT; textRender = &textRenderLarge; }
        else if (!strcmp(argv[i], "--output:narrow")) { outputMode = OUTPUT_TEXT; textRender = &textRenderNarrow; }
        else if (!strcmp(argv[i], "--output:medium")) { outputMode = OUTPUT_TEXT; textRender = &textRenderMedium; }
        else if (!strcmp(argv[i], "--output:compact")) { outputMode = OUTPUT_TEXT; textRender = &textRenderCompact; }
        else if (!strcmp(argv[i], "--output:tiny")) { outputMode = OUTPUT_TEXT; textRender = &textRenderTiny; }
        else if (!strcmp(argv[i], "--output:dots")) { outputMode = OUTPUT_TEXT; textRender = &textRenderDots; }
        else if (!strcmp(argv[i], "--output:dots6")) { outputMode = OUTPUT_TEXT; textRender = &textRenderDots6; }
        else if (!strcmp(argv[i], "--output:bmp")) { outputMode = OUTPUT_BITMAP; }
        else if (!strcmp(argv[i], "--output:svg")) { outputMode = OUTPUT_SVG; }
        else if (!strcmp(argv[i], "--output:sixel")) { outputMode = OUTPUT_SIXEL; }
        else if (!strcmp(argv[i], "--bmp-scale")) { scale = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--svg-color")) { color = argv[++i]; }
        else if (!strcmp(argv[i], "--svg-point")) { moduleSize = atof(argv[++i]); }
        else if (!strcmp(argv[i], "--svg-round")) { moduleRound = atof(argv[++i]); }
        else if (!strcmp(argv[i], "--svg-finder-round")) { finderPart = true; finderRound = atof(argv[++i]); }
        else if (!strcmp(argv[i], "--svg-alignment-round")) { alignmentPart = true; alignmentRound = atof(argv[++i]); }
        else if (!strcmp(argv[i], "--sixel-scale")) { scale = atoi(argv[++i]); }
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
        fprintf(stderr, "Usage:  qrcode [--ecl:<l|m|q|h>] [--uppercase] [--invert] [--quiet 4] [--output:<large|narrow|medium|compact|tiny|bmp|svg>] [--file filename] <value>\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "For --output:bmp:  [--bmp-scale 4]\n");
        fprintf(stderr, "For --output:svg:  [--svg-point 1.0] [--svg-round 0.0] [--svg-finder-round 0.0] [--svg-alignment-round 0.0]\n");
        fprintf(stderr, "For --output:sixel:  [--sixel-scale 4]\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "Example:  ./qrcode --output:svg --svg-round 1 --svg-finder-round 1 --svg-point 0.9 --file hello.svg \"Hello world!\"\n");
        fprintf(stderr, "\n");
        return -1;
    }

    // Clean QR Code object
    qrcode_t qrcode;
    QrCodeInit(&qrcode, QRCODE_VERSION_MAX, errorCorrectionLevel);
    qrcode.maskPattern = maskPattern;
    qrcode.optimizeEcc = optimizeEcc;
    qrcode.version = version;

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
        _setmode(_fileno(stdout), O_BINARY);
        if (outputMode == OUTPUT_TEXT) SetConsoleOutputCP(CP_UTF8);
#endif
        switch (outputMode)
        {
            case OUTPUT_TEXT: OutputQrCodeText(&qrcode, ofp, dimension, textRender, quiet, invert); break;
            case OUTPUT_BITMAP: OutputQrCodeImageBitmap(&qrcode, ofp, dimension, quiet, scale, invert); break;
            case OUTPUT_SVG: OutputQrCodeImageSvg(&qrcode, ofp, dimension, quiet, invert, color, moduleSize, moduleRound, finderPart, finderRound, alignmentPart, alignmentRound); break;
            case OUTPUT_SIXEL: OutputQrCodeSixel(&qrcode, ofp, dimension, quiet, scale, invert); break;
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
