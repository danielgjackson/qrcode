// Barcode - Generates a CODE128 barcode
// Dan Jackson, 2019

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// For debug
#include <stdio.h>

#include "qrcode.h"

#define QRCODE_MAX_VERSION 40
#define QRCODE_SIZE_TO_VERSION(_n) (((_n) - 17) / 4)

// Initialize a QR Code object
void QrCodeInit(qrcode_t *qrcode)
{
    memset(qrcode, 0, sizeof(qrcode_t));
}


typedef enum {
    // Function patterns
    QRCODE_PART_SEPARATOR = -5,
    QRCODE_PART_FINDER = -4,
    QRCODE_PART_ALIGNMENT = -3,
    QRCODE_PART_TIMING_H = -2,
    QRCODE_PART_TIMING_V = -1,
    // Quiet
    QRCODE_PART_QUIET = 0,
    // Encoding region
    QRCODE_PART_CONTENT = 1,
    QRCODE_PART_FORMAT = 2,
    QRCODE_PART_VERSION = 3,
} qrcode_part_t;

// Total data modules minus function pattern and format/version = data capacity in bits
static uint16_t qrcode_data_capacity[QRCODE_MAX_VERSION + 1] = {
    0,
    //  1,     2,     3,     4,     5,     6,     7,     8,     9,     0,
      208,   359,   567,   807,  1079,  1383,  1568,  1936,  2336,  2768,   // v1-v10
     3232,  3728,  4256,  4651,  5243,  5867,  6523,  7211,  7931,  8683,   // v11-v20
     9252, 10068, 10916, 11796, 12708, 13652, 14628, 15371, 16411, 17483,   // v21-v30
    18587, 19723, 20891, 22091, 23008, 24272, 25568, 26896, 28256, 29648,   // v31-v40
};

static uint8_t *qrcode_alignment_coordinates[QRCODE_MAX_VERSION + 1] = {
    (uint8_t[]){ 0 },                               // 0=invalid
    (uint8_t[]){ 0 },                               // v1   21
    (uint8_t[]){ 6, 18, 0 },                        // v2   25
    (uint8_t[]){ 6, 22, 0 },                        // v3   29
    (uint8_t[]){ 6, 26, 0 },                        // v4   33
    (uint8_t[]){ 6, 30, 0 },                        // v5   37
    (uint8_t[]){ 6, 34, 0 },                        // v6   41
    (uint8_t[]){ 6, 22, 38, 0 },                    // v7   45
    (uint8_t[]){ 6, 24, 42, 0 },                    // v8   49
    (uint8_t[]){ 6, 26, 46, 0 },                    // v9   53
    (uint8_t[]){ 6, 28, 50, 0 },                    // v10  57
    (uint8_t[]){ 6, 30, 54, 0 },                    // v11  61
    (uint8_t[]){ 6, 32, 58, 0 },                    // v12  65
    (uint8_t[]){ 6, 34, 62, 0 },                    // v13  69
    (uint8_t[]){ 6, 26, 46, 66, 0 },                // v14  73
    (uint8_t[]){ 6, 26, 48, 70, 0 },                // v15  77
    (uint8_t[]){ 6, 26, 50, 74, 0 },                // v16  81
    (uint8_t[]){ 6, 30, 54, 78, 0 },                // v17  85
    (uint8_t[]){ 6, 30, 56, 82, 0 },                // v18  89
    (uint8_t[]){ 6, 30, 58, 86, 0 },                // v19  93
    (uint8_t[]){ 6, 34, 62, 90, 0 },                // v20  97
    (uint8_t[]){ 6, 28, 50, 72, 94, 0 },            // v21 101
    (uint8_t[]){ 6, 26, 50, 74, 98, 0 },            // v22 105
    (uint8_t[]){ 6, 30, 54, 78, 102, 0 },           // v23 109
    (uint8_t[]){ 6, 28, 54, 80, 106, 0 },           // v24 113
    (uint8_t[]){ 6, 32, 58, 84, 110, 0 },           // v25 117
    (uint8_t[]){ 6, 30, 58, 86, 114, 0 },           // v26 121
    (uint8_t[]){ 6, 34, 62, 90, 118, 0 },           // v27 125
    (uint8_t[]){ 6, 26, 50, 74, 98, 122, 0 },       // v28 129
    (uint8_t[]){ 6, 30, 54, 78, 102, 126, 0 },      // v29 133
    (uint8_t[]){ 6, 26, 52, 78, 104, 130, 0 },      // v30 137
    (uint8_t[]){ 6, 30, 56, 82, 108, 134, 0 },      // v31 141
    (uint8_t[]){ 6, 34, 60, 86, 112, 138, 0 },      // v32 145
    (uint8_t[]){ 6, 30, 58, 86, 114, 142, 0 },      // v33 149
    (uint8_t[]){ 6, 34, 62, 90, 118, 146, 0 },      // v34 153
    (uint8_t[]){ 6, 30, 54, 78, 102, 126, 150, 0 }, // v35 157
    (uint8_t[]){ 6, 24, 50, 76, 102, 128, 154, 0 }, // v36 161
    (uint8_t[]){ 6, 28, 54, 80, 106, 132, 158, 0 }, // v37 165
    (uint8_t[]){ 6, 32, 58, 84, 110, 136, 162, 0 }, // v38 169
    (uint8_t[]){ 6, 26, 54, 82, 110, 138, 166, 0 }, // v39 173
    (uint8_t[]){ 6, 30, 58, 86, 114, 142, 170, 0 }, // v40 177
};

// (internal)
qrcode_part_t QrCodeIdentifyModule(qrcode_t* qrcode, int x, int y)
{
    const int finderSize = 7;
    const int timingRow = 6;
    const int timingCol = 6;
    const int versionSize = 3;
    const int alignmentRadius = 2;
    const int version = QRCODE_SIZE_TO_VERSION(qrcode->size);

    // Quiet zone
    if (x < 0 || y < 0 || x >= qrcode->size || y >= qrcode->size) return QRCODE_PART_QUIET;     // Outside


    // -- Function patterns --

    // Separator
    if (x == finderSize && y <= finderSize) return QRCODE_PART_SEPARATOR;                       // Right of top-left
    if (y == finderSize && x <= finderSize) return QRCODE_PART_SEPARATOR;                       // Bottom of top-left
    if (x == qrcode->size - 1 - finderSize && y <= finderSize) return QRCODE_PART_SEPARATOR;    // Left of top-right
    if (y == finderSize && x >= qrcode->size - 1 - finderSize) return QRCODE_PART_SEPARATOR;    // Bottom of top-right
    if (y == qrcode->size - 1 - finderSize && x <= finderSize) return QRCODE_PART_SEPARATOR;    // Top of bottom-left
    if (x == finderSize && y >= qrcode->size - 1 - finderSize) return QRCODE_PART_SEPARATOR;    // Right of bottom-left

    // Finders
    if (x < finderSize && y < finderSize) return QRCODE_PART_FINDER;                            // Top-left finder
    if (x >= qrcode->size - finderSize && y < finderSize) return QRCODE_PART_FINDER;            // Top-right finder
    if (x < finderSize && y >= qrcode->size - finderSize) return QRCODE_PART_FINDER;            // Bottom-left finder

    // Timing
    if (y == timingRow && x > finderSize && x < qrcode->size - 1 - finderSize) return QRCODE_PART_TIMING_H; // Timing: horizontal
    if (x == timingCol && y > finderSize && y < qrcode->size - 1 - finderSize) return QRCODE_PART_TIMING_V; // Timing: vertical

    // Alignment
    for (uint8_t *h = qrcode_alignment_coordinates[version]; *h > 0; h++)
    {
        for (uint8_t *v = qrcode_alignment_coordinates[version]; *v > 0; v++)
        {
            if (*h <= finderSize && *v <= finderSize) continue;                     // Obscured by top-left finder
            if (*h >= qrcode->size - 1 - finderSize && *v <= finderSize) continue;  // Obscured by top-right finder
            if (*h <= finderSize && *v >= qrcode->size - 1 - finderSize) continue;  // Obscured by bottom-left finder
            if (x >= *h - alignmentRadius && x <= *h + alignmentRadius && y >= *v - alignmentRadius && y <= *v + alignmentRadius) return QRCODE_PART_ALIGNMENT;
        }
    }


    // -- Encoding region --

    // Format info (31 modules)
    if (x == finderSize + 1 && y <= finderSize + 1 && y != timingRow) return QRCODE_PART_FORMAT;// Format info (right of top-left finder)
    if (y == finderSize + 1 && x <= finderSize + 1 && x != timingCol) return QRCODE_PART_FORMAT;// Format info (bottom of top-left finder)
    if (y == finderSize + 1 && x >= qrcode->size - finderSize - 1) return QRCODE_PART_FORMAT;   // Format info (bottom of top-right finder)
    if (x == finderSize + 1 && y >= qrcode->size - finderSize - 1) return QRCODE_PART_FORMAT;   // Format info (right of bottom-left finder)

    // Version info (V7+) (additional 36 modules, total 67 for format+version)
    if (version >= 7)
    {
        if (x < timingCol && y >= qrcode->size - finderSize - 1 - versionSize && y < qrcode->size - finderSize - 1) return QRCODE_PART_VERSION;  // Bottom-left version
        if (y < timingRow && x >= qrcode->size - finderSize - 1 - versionSize && x < qrcode->size - finderSize - 1) return QRCODE_PART_VERSION;  // Top-right version
    }

    // Content
    return QRCODE_PART_CONTENT;
}


// (internal)
void QrCodeDebugDump(qrcode_t* qrcode)
{
    for (int y = 0; y < qrcode->size; y++)
    {
        for (int x = 0; x < qrcode->size; x++)
        {
            qrcode_part_t part = QrCodeIdentifyModule(qrcode, x, y);
            const char *s = "??";
            switch (part) {
                case QRCODE_PART_QUIET:     s = ".."; break;
                case QRCODE_PART_CONTENT:   s = "[]"; break;
                case QRCODE_PART_SEPARATOR: s = "::"; break;
                case QRCODE_PART_FINDER:    s = "Fi"; break;
                case QRCODE_PART_FORMAT:    s = "Fo"; break;
                case QRCODE_PART_VERSION:   s = "Ve"; break;
                case QRCODE_PART_ALIGNMENT: s = "Al"; break;
                case QRCODE_PART_TIMING_H:  s = "Th"; break;
                case QRCODE_PART_TIMING_V:  s = "Tv"; break;
            }
            printf("%s", s);
        }
        printf("\n");
    }
}


// Assign a buffer and its size (in bytes)
void QrCodeSetBuffer(qrcode_t *qrcode, unsigned int size, uint8_t *buffer, size_t bufferSize)
{
    qrcode->size = size;
    qrcode->buffer = buffer;
    qrcode->bufferSize = bufferSize;
    memset(qrcode->buffer, 0, qrcode->bufferSize);  // clear to light
}

// Generate the code
bool QrCodeGenerate(qrcode_t *qrcode, const char *text)
{

// TODO: Code generation -- for now, debug dump
QrCodeDebugDump(qrcode);

    return false;
}

