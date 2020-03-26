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
#define QRCODE_FINDER_SIZE 7
#define QRCODE_TIMING_OFFSET 6
#define QRCODE_VERSION_SIZE 3
#define QRCODE_ALIGNMENT_RADIUS 2
#define QRCODE_MODULE_LIGHT 0
#define QRCODE_MODULE_DARK 1

// Initialize a QR Code object
void QrCodeInit(qrcode_t *qrcode)
{
    memset(qrcode, 0, sizeof(qrcode_t));
}

int QrCodeGetModule(qrcode_t *qrcode, int x, int y)
{
    if (x < 0 || y < 0 || x >= qrcode->size || y >= qrcode->size) return 0; // quiet
    // TODO: Bitfield instead of byte array
    int offset = y * qrcode->size + x;
    if (qrcode->buffer == NULL || offset < 0 || offset >= qrcode->bufferSize) return -1;
    return qrcode->buffer[offset];
}

static void QrCodeSetModule(qrcode_t *qrcode, int x, int y, int value)
{
    if (x < 0 || y < 0 || x >= qrcode->size || y >= qrcode->size) return; // quiet
    // TODO: Bitfield instead of byte array
    int offset = y * qrcode->size + x;
    if (qrcode->buffer == NULL || offset < 0 || offset >= qrcode->bufferSize) return;
    qrcode->buffer[offset] = value;
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

// (internal) Determines which part a given module coordinate is.
static qrcode_part_t QrCodeIdentifyModule(qrcode_t* qrcode, int x, int y)
{
    const int version = QRCODE_SIZE_TO_VERSION(qrcode->size);

    // Quiet zone
    if (x < 0 || y < 0 || x >= qrcode->size || y >= qrcode->size) return QRCODE_PART_QUIET;     // Outside


    // -- Function patterns --

    // Separator
    if (x == QRCODE_FINDER_SIZE && y <= QRCODE_FINDER_SIZE) return QRCODE_PART_SEPARATOR;                       // Right of top-left
    if (y == QRCODE_FINDER_SIZE && x <= QRCODE_FINDER_SIZE) return QRCODE_PART_SEPARATOR;                       // Bottom of top-left
    if (x == qrcode->size - 1 - QRCODE_FINDER_SIZE && y <= QRCODE_FINDER_SIZE) return QRCODE_PART_SEPARATOR;    // Left of top-right
    if (y == QRCODE_FINDER_SIZE && x >= qrcode->size - 1 - QRCODE_FINDER_SIZE) return QRCODE_PART_SEPARATOR;    // Bottom of top-right
    if (y == qrcode->size - 1 - QRCODE_FINDER_SIZE && x <= QRCODE_FINDER_SIZE) return QRCODE_PART_SEPARATOR;    // Top of bottom-left
    if (x == QRCODE_FINDER_SIZE && y >= qrcode->size - 1 - QRCODE_FINDER_SIZE) return QRCODE_PART_SEPARATOR;    // Right of bottom-left

    // Finders
    if (x < QRCODE_FINDER_SIZE && y < QRCODE_FINDER_SIZE) return QRCODE_PART_FINDER;                            // Top-left finder
    if (x >= qrcode->size - QRCODE_FINDER_SIZE && y < QRCODE_FINDER_SIZE) return QRCODE_PART_FINDER;            // Top-right finder
    if (x < QRCODE_FINDER_SIZE && y >= qrcode->size - QRCODE_FINDER_SIZE) return QRCODE_PART_FINDER;            // Bottom-left finder

    // Alignment
    for (uint8_t *h = qrcode_alignment_coordinates[version]; *h > 0; h++)
    {
        if (x >= *h - QRCODE_ALIGNMENT_RADIUS && x <= *h + QRCODE_ALIGNMENT_RADIUS)
        {
            for (uint8_t* v = qrcode_alignment_coordinates[version]; *v > 0; v++)
            {
                if (*h <= QRCODE_FINDER_SIZE && *v <= QRCODE_FINDER_SIZE) continue;                     // Obscured by top-left finder
                if (*h >= qrcode->size - 1 - QRCODE_FINDER_SIZE && *v <= QRCODE_FINDER_SIZE) continue;  // Obscured by top-right finder
                if (*h <= QRCODE_FINDER_SIZE && *v >= qrcode->size - 1 - QRCODE_FINDER_SIZE) continue;  // Obscured by bottom-left finder
                // x >= *h - QRCODE_ALIGNMENT_RADIUS && x <= *h + QRCODE_ALIGNMENT_RADIUS
                if (y >= *v - QRCODE_ALIGNMENT_RADIUS && y <= *v + QRCODE_ALIGNMENT_RADIUS) return QRCODE_PART_ALIGNMENT;
            }
        }
    }

    // Timing
    if (y == QRCODE_TIMING_OFFSET && x > QRCODE_FINDER_SIZE && x < qrcode->size - 1 - QRCODE_FINDER_SIZE) return QRCODE_PART_TIMING_H; // Timing: horizontal
    if (x == QRCODE_TIMING_OFFSET && y > QRCODE_FINDER_SIZE && y < qrcode->size - 1 - QRCODE_FINDER_SIZE) return QRCODE_PART_TIMING_V; // Timing: vertical


    // -- Encoding region --

    // Format info (2*15+1=31 modules)
    if (x == QRCODE_FINDER_SIZE + 1 && y <= QRCODE_FINDER_SIZE + 1 && y != QRCODE_TIMING_OFFSET) return QRCODE_PART_FORMAT;// Format info (right of top-left finder)
    if (y == QRCODE_FINDER_SIZE + 1 && x <= QRCODE_FINDER_SIZE + 1 && x != QRCODE_TIMING_OFFSET) return QRCODE_PART_FORMAT;// Format info (bottom of top-left finder)
    if (y == QRCODE_FINDER_SIZE + 1 && x >= qrcode->size - QRCODE_FINDER_SIZE - 1) return QRCODE_PART_FORMAT;   // Format info (bottom of top-right finder)
    if (x == QRCODE_FINDER_SIZE + 1 && y >= qrcode->size - QRCODE_FINDER_SIZE - 1) return QRCODE_PART_FORMAT;   // Format info (right of bottom-left finder)

    // Version info (V7+) (additional 2*18=36 modules, total 67 for format+version)
    if (version >= 7)
    {
        if (x < QRCODE_TIMING_OFFSET && y >= qrcode->size - QRCODE_FINDER_SIZE - 1 - QRCODE_VERSION_SIZE && y < qrcode->size - QRCODE_FINDER_SIZE - 1) return QRCODE_PART_VERSION;  // Bottom-left version
        if (y < QRCODE_TIMING_OFFSET && x >= qrcode->size - QRCODE_FINDER_SIZE - 1 - QRCODE_VERSION_SIZE && x < qrcode->size - QRCODE_FINDER_SIZE - 1) return QRCODE_PART_VERSION;  // Top-right version
    }

    // Content
    return QRCODE_PART_CONTENT;
}


// (internal)
void QrCodeDebugDump(qrcode_t* qrcode)
{
    for (int y = -qrcode->quiet; y < qrcode->size + qrcode->quiet; y++)
    {
        int lastColor = -1;
        for (int x = -qrcode->quiet; x < qrcode->size + qrcode->quiet; x++)
        {
            qrcode_part_t part = QrCodeIdentifyModule(qrcode, x, y);
            const char *s = "??";
            int color = QrCodeGetModule(qrcode, x, y);;

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
            // Debug double digits
            if (color >= 100) {
                static char buf[3];
                buf[0] = '0' + (color / 10) % 10;
                buf[1] = '0' + color % 10;
                buf[2] = '\0';
                if (buf[0] == '0') buf[0] = '_';
                s = buf;
                color = color >= 200 ? 4 : 3;
            }
            if (color != lastColor) {
                //if (color == 0) printf("\033[30m\033[47;1m");   // White background, black text
                //if (color == 1) printf("\033[37;1m\033[40m");   // Black background, white text
                if (color == QRCODE_MODULE_LIGHT) printf("\033[37m\033[47;1m");      // Grey text, white background
                else if (color == QRCODE_MODULE_DARK) printf("\033[30;1m\033[40m");  // Dark grey text, black background
                else if (color == 3) printf("\033[92m\033[42m");                     // Debug 1xx Green
                else if (color == 4) printf("\033[93m\033[43m");                     // Debug 2xx 
                else printf("\033[91m\033[41m");                                     // Red
            }
            printf("%s", s);
            lastColor = color;
        }
        printf("\033[0m \n");    // reset
    }
}


// Assign a buffer and its size (in bytes)
void QrCodeSetBuffer(qrcode_t *qrcode, unsigned int size, uint8_t *buffer, size_t bufferSize)
{
    qrcode->size = size;
    qrcode->buffer = buffer;
    qrcode->bufferSize = bufferSize;
    memset(qrcode->buffer, 2, qrcode->bufferSize);  // clear
}


// Draw finder and quiet padding
static void QrCodeDrawFinder(qrcode_t *qrcode, int ox, int oy)
{
    for (int y = -QRCODE_FINDER_SIZE / 2 - 1; y <= QRCODE_FINDER_SIZE / 2 + 1; y++)
    {
        for (int x = -QRCODE_FINDER_SIZE / 2 - 1; x <= QRCODE_FINDER_SIZE / 2 + 1; x++)
        {
            int value = (abs(x) > abs(y) ? abs(x) : abs(y)) & 1;
            if (x == 0 && y == 0) value = QRCODE_MODULE_DARK;
            QrCodeSetModule(qrcode, ox + x, oy + y, value);
        }
    }
}

static void QrCodeDrawTiming(qrcode_t *qrcode, bool vertical)
{
    for (int i = QRCODE_FINDER_SIZE + 1; i < qrcode->size - QRCODE_FINDER_SIZE - 1; i++)
    {
        int value = (~i & 1);
        if (!vertical) QrCodeSetModule(qrcode, i, QRCODE_TIMING_OFFSET, value);
        else QrCodeSetModule(qrcode, QRCODE_TIMING_OFFSET, i, value);
    }
}

static void QrCodeDrawAlignment(qrcode_t* qrcode, int ox, int oy)
{
    for (int y = -QRCODE_ALIGNMENT_RADIUS; y <= QRCODE_ALIGNMENT_RADIUS; y++)
    {
        for (int x = -QRCODE_ALIGNMENT_RADIUS; x <= QRCODE_ALIGNMENT_RADIUS; x++)
        {
            int value = 1 - ((abs(x) > abs(y) ? abs(x) : abs(y)) & 1);
            QrCodeSetModule(qrcode, ox + x, oy + y, value);
        }
    }
}

// Draw 15-bit format code (2-bit error-correction level, 3-bit mask, 10-bit BCH error-correction; all masked)
static void QrCodeDrawFormat(qrcode_t* qrcode, uint16_t value)
{
    for (int i = 0; i < 15; i++)
    {
        int v = (value >> i) & 1;
        //v = 100 + i; // for debug
        // 15-bits starting LSB clockwise from top-left finder avoiding timing strips
        if (i < 6) QrCodeSetModule(qrcode, QRCODE_FINDER_SIZE + 1, i, v);
        else if (i == 6) QrCodeSetModule(qrcode, QRCODE_FINDER_SIZE + 1, QRCODE_FINDER_SIZE, v);
        else if (i == 7) QrCodeSetModule(qrcode, QRCODE_FINDER_SIZE + 1, QRCODE_FINDER_SIZE + 1, v);
        else if (i == 8) QrCodeSetModule(qrcode, QRCODE_FINDER_SIZE, QRCODE_FINDER_SIZE + 1, v);
        else QrCodeSetModule(qrcode, 14 - i, QRCODE_FINDER_SIZE + 1, v);

        // lower 8-bits starting LSB right-to-left underneath top-right finder
        if (i < 8) QrCodeSetModule(qrcode, qrcode->size - 1 - i, QRCODE_FINDER_SIZE + 1, v);
        // upper 7-bits starting LSB top-to-bottom right of bottom-left finder
        else QrCodeSetModule(qrcode, QRCODE_FINDER_SIZE + 1, qrcode->size - QRCODE_FINDER_SIZE - 8 + i, v);
    }
    // dark module
    QrCodeSetModule(qrcode, QRCODE_FINDER_SIZE + 1, qrcode->size - 1 - QRCODE_FINDER_SIZE, QRCODE_MODULE_DARK);
}

// Draw 18-bit version information (6-bit version number, 12-bit error-correction (18,6) Golay code)
static void QrCodeDrawVersion(qrcode_t *qrcode, uint32_t value)
{
    // No version information on V1-V6
    if (QRCODE_SIZE_TO_VERSION(qrcode->size) < 7) return;

    for (int i = 0; i < 18; i++)
    {
        int v = (value >> i) & 1;
        //v = 100 + i; // for debug
        int col = i / QRCODE_VERSION_SIZE;
        int row = i % QRCODE_VERSION_SIZE;
        QrCodeSetModule(qrcode, col, qrcode->size - 1 - QRCODE_FINDER_SIZE - QRCODE_VERSION_SIZE + row, v);
        QrCodeSetModule(qrcode, qrcode->size - 1 - QRCODE_FINDER_SIZE - QRCODE_VERSION_SIZE + row, col, v);
    }
}


// Calculate 15-bit format code (2-bit error-correction level, 3-bit mask, 10-bit BCH error-correction; all masked)
static uint16_t QrCodeCalcFormat(qrcode, errorCorrectionLevel, maskPattern)
{
    // 0LLMMMEEEEEEEEEE
    uint16_t format = (errorCorrectionLevel << 13) | (maskPattern << 10);

// TODO: Calculate BCH error correction code (bottom 10 bits); or lookup table.
;

    static uint16_t formatMask = 0x5412;   // 0b0101010000010010
    format ^= formatMask;

    return format;
}

// Calculate 18-bit version information (6-bit version number, 12-bit error-correction (18,6) Golay code)
static uint32_t QrCodeCalcVersion(qrcode_t *qrcode, int version)
{
    uint32_t value = version << 12;

// TODO: Calculate 12-bit error-correction (18,6) Golay code; or lookup table.
;

    return value;
}

typedef struct {
    int x;
    int y;
} qrcode_cursor_t;

static void QrCodeCursorReset(qrcode_t* qrcode, qrcode_cursor_t* cursor)
{
    memset(cursor, 0, sizeof(qrcode_cursor_t));
    cursor->x = qrcode->size - 1;
    cursor->y = qrcode->size - 1;
}

static bool QrCodeCursorAdvance(qrcode_t* qrcode, qrcode_cursor_t* cursor)
{
    while (cursor->x >= 0)
    {
        // RHS or LHS of 2-module column?
        bool rhs = (cursor->x & 1) ^ (cursor->x > QRCODE_TIMING_OFFSET ? 1 : 0);
        // Upwards or downwards?
        bool upwards = ((cursor->x - (cursor->x > QRCODE_TIMING_OFFSET ? 1 : 0)) / 2) & 1;

        if (rhs) cursor->x--;
        else
        {
            cursor->x++;
            if (upwards)
            {
                if (cursor->y <= 0) cursor->x -= 2;
                else cursor->y--;
            }
            else
            {
                if (cursor->y >= qrcode->size - 1) cursor->x -= 2;
                else cursor->y++;
            }
        }

        if (QrCodeIdentifyModule(qrcode, cursor->x, cursor->y) == QRCODE_PART_CONTENT) return true;
    }
    return false;
}


// Generate the code
bool QrCodeGenerate(qrcode_t *qrcode, const char *text)
{
    const int version = QRCODE_SIZE_TO_VERSION(qrcode->size);

    QrCodeDrawFinder(qrcode, QRCODE_FINDER_SIZE / 2, QRCODE_FINDER_SIZE / 2);
    QrCodeDrawFinder(qrcode, qrcode->size - 1 - QRCODE_FINDER_SIZE / 2, QRCODE_FINDER_SIZE / 2);
    QrCodeDrawFinder(qrcode, QRCODE_FINDER_SIZE / 2, qrcode->size - 1 - QRCODE_FINDER_SIZE / 2);

    QrCodeDrawTiming(qrcode, false);
    QrCodeDrawTiming(qrcode, true);

    // Alignment
    for (uint8_t* h = qrcode_alignment_coordinates[version]; *h > 0; h++)
    {
        for (uint8_t* v = qrcode_alignment_coordinates[version]; *v > 0; v++)
        {
            if (*h <= QRCODE_FINDER_SIZE && *v <= QRCODE_FINDER_SIZE) continue;                     // Obscured by top-left finder
            if (*h >= qrcode->size - 1 - QRCODE_FINDER_SIZE && *v <= QRCODE_FINDER_SIZE) continue;  // Obscured by top-right finder
            if (*h <= QRCODE_FINDER_SIZE && *v >= qrcode->size - 1 - QRCODE_FINDER_SIZE) continue;  // Obscured by bottom-left finder
            QrCodeDrawAlignment(qrcode, *h, *v);
        }
    }


    // TODO: Code generation, ECC, masking
    qrcode_cursor_t cursor;
    QrCodeCursorReset(qrcode, &cursor);
    int module = 0;
    do {
        int bit = 7 - module % 8;
        int byte = module / 8 % 2;
        QrCodeSetModule(qrcode, cursor.x, cursor.y, (byte ? 200 : 100) + bit);
        module++;
    } while (QrCodeCursorAdvance(qrcode, &cursor));


    // TODO: Format info ()
    // ...
    int errorCorrectionLevel = 0;   // 2-bit
    int maskPattern = 0;            // 3-bit
    uint16_t format = QrCodeCalcFormat(qrcode, errorCorrectionLevel, maskPattern);
    QrCodeDrawFormat(qrcode, format);
    
    // TODO: Version info (V7+) (additional 36 modules, total 67 for format+version)
    if (version >= 7)
    {
        uint32_t versionValue = QrCodeCalcVersion(qrcode, version);
        QrCodeDrawVersion(qrcode, versionValue);
    }


// Debug dump
QrCodeDebugDump(qrcode);

    return false;
}

