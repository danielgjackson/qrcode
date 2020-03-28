// Barcode - Generates a CODE128 barcode
// Dan Jackson, 2019

// Step 1. Data analysis -- identify characters encoded, select version.
// Step 2. Data encodation -- convert to segments then a bit stream, pad for version.
// Step 3. Error correction encoding -- divide sequence into blocks, generate and append error correction codewords.
// Step 4. Strucutre final message -- interleave data and error correction and remainder bits.
// Step 5. Module palcement in matrix.
// Step 6. Masking.
// Step 7. Format and version information.

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// For output functions (TODO: Move these out?)
#include <stdio.h>

#include "qrcode.h"

#define QRCODE_DEBUG_DUMP

//#define QRCODE_DIMENSION_TO_VERSION(_n) (((_n) - 17) / 4)
#define QRCODE_FINDER_SIZE 7
#define QRCODE_TIMING_OFFSET 6
#define QRCODE_VERSION_SIZE 3
#define QRCODE_ALIGNMENT_RADIUS 2
#define QRCODE_MODULE_LIGHT 0
#define QRCODE_MODULE_DARK 1

#define QRCODE_PAD_CODEWORDS 0xec11 // Pad codewords 0b11101100=0xec 0b00010001=0x11


// Write bits to buffer
static size_t QrCodeBufferAppend(uint8_t *writeBuffer, size_t writePosition, uint32_t value, size_t bitCount)
{
    for (size_t i = 0; i < bitCount; i++)
    {
        uint8_t *writeByte = writeBuffer + ((writePosition + i) >> 3);
        int writeBit = 7 - ((writePosition + i) & 0x07);
        int mask = (1 << (bitCount - 1 - i));
        *writeByte = (*writeByte & ~(1 << mask)) | (value & (1 << mask));
    }
    return bitCount;
}


typedef enum
{
    QRCODE_MODE_INDICATOR_ECI = 0x07,               // 0b0111 ECI
    QRCODE_MODE_INDICATOR_NUMERIC = 0x01,           // 0b0001 Numeric (maximal groups of 3/2/1 digits encoded to 10/7/4-bit binary)
    QRCODE_MODE_INDICATOR_ALPHANUMERIC = 0x02,      // 0b0010 Alphanumeric ('0'-'9', 'A'-'Z', ' ', '$', '%', '*', '+', '-', '.', '/', ':') -> 0-44 index. Pairs combined (a*45+b) encoded as 11-bit; odd remainder encoded as 6-bit.
    QRCODE_MODE_INDICATOR_8_BIT = 0x04,             // 0b0100 8-bit byte
    QRCODE_MODE_INDICATOR_KANJI = 0x08,             // 0b1000 Kanji
    QRCODE_MODE_INDICATOR_STRUCTURED_APPEND = 0x03, // 0b0011 Structured Append
    QRCODE_MODE_INDICATOR_FNC1_FIRST = 0x05,        // 0b0101 FNC1 (First position)
    QRCODE_MODE_INDICATOR_FNC1_SECOND = 0x09,       // 0b1001 FNC1 (Second position)
    QRCODE_MODE_INDICATOR_TERMINATOR = 0x00,        // 0b0000 Terminator (End of Message)
} qrcode_mode_indicator_t;


// Defines a single segment of text encoded in one mode
typedef struct
{
    qrcode_mode_indicator_t mode;   // Segment mode
    size_t charCount;               // Original number of characters (must be remembered for version-specific encoding)
    const uint8_t *buffer;          // Buffer of encoded bits
    size_t bitCount;                // Number of bits in the segment
} qrcode_segment_t;


// Creates an 8-bit text segment
qrcode_segment_t QrCodeSegment8bitCreate(const char *text, size_t charCount)
{
    qrcode_segment_t segment = { 0 };
    segment.mode = QRCODE_MODE_INDICATOR_8_BIT;
    segment.charCount = charCount;
    segment.buffer = (const uint8_t *)text;
    segment.bitCount = (size_t)8 * segment.charCount;
    return segment;
}

// Check the buffer is entirely compatible with a numeric encoding
bool QrCodeSegmentNumericCheck(const char *text, size_t charCount)
{
    for (size_t i = 0; i < charCount; i++)
    {
        if (text[i] < '0' || text[i] > '9') return false;
    }
    return true;
}

// Creates a numeric segment, buffer size should be at least: QRCODE_SEGMENT_NUMERIC_BUFFER_BYTES(charCount)
qrcode_segment_t QrCodeSegmentNumericCreate(const char *text, size_t charCount, uint8_t *buffer)
{
    qrcode_segment_t segment = { 0 };
    segment.mode = QRCODE_MODE_INDICATOR_NUMERIC;
    segment.charCount = charCount;
    segment.buffer = (const uint8_t *)buffer;
    segment.bitCount = 0;
    for (size_t i = 0; i < charCount; )
    {
        size_t remain = (charCount - i) > 3 ? 3 : (charCount - i);
        int value = text[i] - '0';
        int bits = 4;
        // Maximal groups of 3/2/1 digits encoded to 10/7/4-bit binary
        if (remain > 1) { value = value * 10 + text[i + 1] - '0'; bits += 3; }
        if (remain > 2) { value = value * 10 + text[i + 2] - '0'; bits += 3; }
        segment.bitCount += QrCodeBufferAppend(buffer, segment.bitCount, value, bits);
        i += remain;
    }
    return segment;
}

// Returns Alphanumeric index for a character (0-44), or -1 if none
int QrCodeSegmentAlphanumericIndex(char c, bool allowLowercase)
{
    if (c >= '0' && c <= '9') return c - '0';      // 0-9 (0-9)
    if (c >= 'A' && c <= 'Z') return 10 + c - 'A'; // A-Z (10-35)
    if (allowLowercase && c >= 'a' && c <= 'z') return 10 + c - 'a'; // transform to upper-case A-Z (10-35)
    //  36,  37,  38,  39,  40,  41,  42,  43,  44
    // ' ', '$', '%', '*', '+', '-', '.', '/', ':'
    //0x20,0x24,0x25,0x2A,0x2B,0x2D,0x2E,0x2F,0x3A
    static const char *symbols = " $%*+-./:";
    const char *p = strchr(symbols, c);
    if (p != NULL) return 36 + (int)(p - symbols);
    return -1;
}

// Check the buffer is entirely compatible with an alphanumeric encoding
bool QrCodeSegmentAlphanumericCheck(const char *text, size_t charCount, bool allowLowercase)
{
    for (size_t i = 0; i < charCount; i++)
    {
        if (QrCodeSegmentAlphanumericIndex(text[i], allowLowercase) < 0) return false;
    }
    return true;
}

// Creates an alphanumeric segment, buffer size should be at least: QRCODE_SEGMENT_ALPHANUMERIC_BUFFER_BYTES(charCount)
qrcode_segment_t QrCodeSegmentAlphanumericCreate(const char *text, size_t charCount, uint8_t *buffer)
{
    qrcode_segment_t segment = { 0 };
    segment.mode = QRCODE_MODE_INDICATOR_ALPHANUMERIC;
    segment.charCount = charCount;
    segment.buffer = (const uint8_t*)buffer;
    segment.bitCount = 0;
    for (size_t i = 0; i < charCount; )
    {
        size_t remain = (charCount - i) > 2 ? 2 : (charCount - i);
        int value = QrCodeSegmentAlphanumericIndex(text[i], true);
        int bits = 6;
        // Pairs combined(a * 45 + b) encoded as 11 - bit; odd remainder encoded as 6 - bit.
        if (remain > 1) { value = value * 45 + QrCodeSegmentAlphanumericIndex(text[i], true); bits += 5; }
        segment.bitCount += QrCodeBufferAppend(buffer, segment.bitCount, value, bits);
        i += remain;
    }
    return segment;
}



// Number of bits in Character Count Indicator
int QrCodeBitsInCharacterCount(qrcode_t* qrcode, qrcode_mode_indicator_t mode)
{
    // Bands are (1-9), (10-26), (27-40)
    switch (mode)
    {
        case QRCODE_MODE_INDICATOR_NUMERIC:
            return (qrcode->version < 10) ? 10 : (qrcode->version < 27) ? 12 : 14;  // Numeric
        case QRCODE_MODE_INDICATOR_ALPHANUMERIC:
            return (qrcode->version < 10) ? 9 : (qrcode->version < 27) ? 11 : 13; // Alphanumeric
        case QRCODE_MODE_INDICATOR_8_BIT:
            return (qrcode->version < 10) ? 8 : (qrcode->version < 27) ? 16 : 16; // 8-bit
        case QRCODE_MODE_INDICATOR_KANJI:
            return (qrcode->version < 10) ? 8 : (qrcode->version < 27) ? 10 : 12; // Kanji
    }
    return 0;
}


typedef enum
{
    // Function patterns
    QRCODE_PART_ALIGNMENT = -4, // Alignment pattern(s)
    QRCODE_PART_TIMING = -3,    // Timing pattern
    QRCODE_PART_SEPARATOR = -2, // Separator around finder position detection patterns
    QRCODE_PART_FINDER = -1,    // Position detection pattern
    // Outside
    QRCODE_PART_QUIET = 0,      // Quiet margin outside of code
    // Encoding region
    QRCODE_PART_CONTENT = 1,    // Data and error-correction codewords
    QRCODE_PART_FORMAT = 2,     // Format Info
    QRCODE_PART_VERSION = 3,    // Version Info
} qrcode_part_t;

// Total data modules minus function pattern and format/version = data capacity in bits
static int QrCodeCapacity(int version)
{
    int capacity = (16 * version + 128) * version + 64;
    if (version >= 2) capacity -= (25 * (version / 7 + 2) - 10) * (version / 7 + 2) - 55;
    if (version >= 7) capacity -= 36;
    return capacity;
}

// Returns coordinates to be used in all combinations (unless overlapping finder pattern) as x/y pairs for alignment, <0: end
static int QrCodeAlignmentCoordinates(int version, int index)
{
    if (version <= 1) return -1;        // no alignment markers
    if (index == 0) return 6;           // first alignment marker is at offset 6
    int count = version / 7 + 2;        // number of alignment markers
    if (index > count) return 0;        // no more markers
    int location = version * 4 + 10;    // lower alignment marker
    int step = (version == 32) ? 26 : (version * 4 + count * 2 + 1) / (count * 2 - 2) * 2; // step to previous
    for (int i = count - 1; i > 0; i--)
    {
        if (i == index) return location;
        location -= step;
    }
    return -1;                          // unreachable under normal use
}


// Initialize a QR Code object
void QrCodeInit(qrcode_t *qrcode)
{
    memset(qrcode, 0, sizeof(qrcode_t));
    qrcode->buffer = NULL;
    qrcode->bufferSize = 0;
    qrcode->version = QRCODE_VERSION_AUTO;
    qrcode->errorCorrectionLevel = QRCODE_ECL_M;
    qrcode->maskPattern = QRCODE_MASK_AUTO;
    qrcode->quiet = QRCODE_QUIET_STANDARD;
}

int QrCodeGetModule(qrcode_t *qrcode, int x, int y)
{
    if (x < 0 || y < 0 || x >= qrcode->dimension || y >= qrcode->dimension) return 0; // quiet
    // TODO: Bitfield instead of byte array
    int offset = y * qrcode->dimension + x;
    if (qrcode->buffer == NULL || offset < 0 || offset >= qrcode->bufferSize) return -1;
    return qrcode->buffer[offset];
}

static void QrCodeSetModule(qrcode_t *qrcode, int x, int y, int value)
{
    if (x < 0 || y < 0 || x >= qrcode->dimension || y >= qrcode->dimension) return; // quiet
    // TODO: Bitfield instead of byte array
    int offset = y * qrcode->dimension + x;
    if (qrcode->buffer == NULL || offset < 0 || offset >= qrcode->bufferSize) return;
    qrcode->buffer[offset] = value;
}

// (internal) Determines which part a given module coordinate belongs to..
static qrcode_part_t QrCodeIdentifyModule(qrcode_t* qrcode, int x, int y)
{
    // Quiet zone
    if (x < 0 || y < 0 || x >= qrcode->dimension || y >= qrcode->dimension) return QRCODE_PART_QUIET;     // Outside

    // -- Function patterns --

    // Separator
    if (x == QRCODE_FINDER_SIZE && y <= QRCODE_FINDER_SIZE) return QRCODE_PART_SEPARATOR;                       // Right of top-left
    if (y == QRCODE_FINDER_SIZE && x <= QRCODE_FINDER_SIZE) return QRCODE_PART_SEPARATOR;                       // Bottom of top-left
    if (x == qrcode->dimension - 1 - QRCODE_FINDER_SIZE && y <= QRCODE_FINDER_SIZE) return QRCODE_PART_SEPARATOR;    // Left of top-right
    if (y == QRCODE_FINDER_SIZE && x >= qrcode->dimension - 1 - QRCODE_FINDER_SIZE) return QRCODE_PART_SEPARATOR;    // Bottom of top-right
    if (y == qrcode->dimension - 1 - QRCODE_FINDER_SIZE && x <= QRCODE_FINDER_SIZE) return QRCODE_PART_SEPARATOR;    // Top of bottom-left
    if (x == QRCODE_FINDER_SIZE && y >= qrcode->dimension - 1 - QRCODE_FINDER_SIZE) return QRCODE_PART_SEPARATOR;    // Right of bottom-left

    // Finders
    if (x < QRCODE_FINDER_SIZE && y < QRCODE_FINDER_SIZE) return QRCODE_PART_FINDER;                            // Top-left finder
    if (x >= qrcode->dimension - QRCODE_FINDER_SIZE && y < QRCODE_FINDER_SIZE) return QRCODE_PART_FINDER;            // Top-right finder
    if (x < QRCODE_FINDER_SIZE && y >= qrcode->dimension - QRCODE_FINDER_SIZE) return QRCODE_PART_FINDER;            // Bottom-left finder

    // Alignment
    for (int hi = 0, h; (h = QrCodeAlignmentCoordinates(qrcode->version, hi)) > 0; hi++)
    {
        if (x >= h - QRCODE_ALIGNMENT_RADIUS && x <= h + QRCODE_ALIGNMENT_RADIUS)
        {
            for (int vi = 0, v; (v = QrCodeAlignmentCoordinates(qrcode->version, vi)) > 0; vi++)
            {
                if (h <= QRCODE_FINDER_SIZE && v <= QRCODE_FINDER_SIZE) continue;                     // Obscured by top-left finder
                if (h >= qrcode->dimension - 1 - QRCODE_FINDER_SIZE && v <= QRCODE_FINDER_SIZE) continue;  // Obscured by top-right finder
                if (h <= QRCODE_FINDER_SIZE && v >= qrcode->dimension - 1 - QRCODE_FINDER_SIZE) continue;  // Obscured by bottom-left finder
                // x >= h - QRCODE_ALIGNMENT_RADIUS && x <= h + QRCODE_ALIGNMENT_RADIUS
                if (y >= v - QRCODE_ALIGNMENT_RADIUS && y <= v + QRCODE_ALIGNMENT_RADIUS) return QRCODE_PART_ALIGNMENT;
            }
        }
    }

    // Timing
    if (y == QRCODE_TIMING_OFFSET && x > QRCODE_FINDER_SIZE && x < qrcode->dimension - 1 - QRCODE_FINDER_SIZE) return QRCODE_PART_TIMING; // Timing: horizontal
    if (x == QRCODE_TIMING_OFFSET && y > QRCODE_FINDER_SIZE && y < qrcode->dimension - 1 - QRCODE_FINDER_SIZE) return QRCODE_PART_TIMING; // Timing: vertical


    // -- Encoding region --

    // Format info (2*15+1=31 modules)
    if (x == QRCODE_FINDER_SIZE + 1 && y <= QRCODE_FINDER_SIZE + 1 && y != QRCODE_TIMING_OFFSET) return QRCODE_PART_FORMAT;// Format info (right of top-left finder)
    if (y == QRCODE_FINDER_SIZE + 1 && x <= QRCODE_FINDER_SIZE + 1 && x != QRCODE_TIMING_OFFSET) return QRCODE_PART_FORMAT;// Format info (bottom of top-left finder)
    if (y == QRCODE_FINDER_SIZE + 1 && x >= qrcode->dimension - QRCODE_FINDER_SIZE - 1) return QRCODE_PART_FORMAT;   // Format info (bottom of top-right finder)
    if (x == QRCODE_FINDER_SIZE + 1 && y >= qrcode->dimension - QRCODE_FINDER_SIZE - 1) return QRCODE_PART_FORMAT;   // Format info (right of bottom-left finder)

    // Version info (V7+) (additional 2*18=36 modules, total 67 for format+version)
    if (qrcode->version >= 7)
    {
        if (x < QRCODE_TIMING_OFFSET && y >= qrcode->dimension - QRCODE_FINDER_SIZE - 1 - QRCODE_VERSION_SIZE && y < qrcode->dimension - QRCODE_FINDER_SIZE - 1) return QRCODE_PART_VERSION;  // Bottom-left version
        if (y < QRCODE_TIMING_OFFSET && x >= qrcode->dimension - QRCODE_FINDER_SIZE - 1 - QRCODE_VERSION_SIZE && x < qrcode->dimension - QRCODE_FINDER_SIZE - 1) return QRCODE_PART_VERSION;  // Top-right version
    }

    // Content
    return QRCODE_PART_CONTENT;
}


#ifdef QRCODE_DEBUG_DUMP
// (internal)
void QrCodeDebugDump(qrcode_t* qrcode)
{
    for (int y = -qrcode->quiet; y < qrcode->dimension + qrcode->quiet; y++)
    {
        int lastColor = -1;
        for (int x = -qrcode->quiet; x < qrcode->dimension + qrcode->quiet; x++)
        {
            qrcode_part_t part = QrCodeIdentifyModule(qrcode, x, y);
            const char *s = "??";
            int color = QrCodeGetModule(qrcode, x, y);;

            switch (part)
            {
                case QRCODE_PART_QUIET:     s = ".."; break;
                case QRCODE_PART_CONTENT:   s = "[]"; break;
                case QRCODE_PART_SEPARATOR: s = "::"; break;
                case QRCODE_PART_FINDER:    s = "Fi"; break;
                case QRCODE_PART_FORMAT:    s = "Fo"; break;
                case QRCODE_PART_VERSION:   s = "Ve"; break;
                case QRCODE_PART_ALIGNMENT: s = "Al"; break;
                case QRCODE_PART_TIMING:    s = "Ti"; break;
            }
            // Debug double digits
            if (color >= 100)
            {
                static char buf[3];
                buf[0] = '0' + (color / 10) % 10;
                buf[1] = '0' + color % 10;
                buf[2] = '\0';
                if (buf[0] == '0') buf[0] = '_';
                s = buf;
                color = color >= 200 ? 4 : 3;
            }
            if (color != lastColor)
            {
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
#endif


// Assign a buffer and its size (in bytes)
void QrCodeSetBuffer(qrcode_t *qrcode, int version, uint8_t *buffer, size_t bufferSize)
{
    qrcode->version = version;
    qrcode->dimension = QRCODE_VERSION_TO_DIMENSION(qrcode->version);
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

static void QrCodeDrawTiming(qrcode_t *qrcode)
{
    for (int i = QRCODE_FINDER_SIZE + 1; i < qrcode->dimension - QRCODE_FINDER_SIZE - 1; i++)
    {
        int value = (~i & 1);
        QrCodeSetModule(qrcode, i, QRCODE_TIMING_OFFSET, value);
        QrCodeSetModule(qrcode, QRCODE_TIMING_OFFSET, i, value);
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

// Draw 15-bit format information (2-bit error-correction level, 3-bit mask, 10-bit BCH error-correction; all masked)
static void QrCodeDrawFormatInfo(qrcode_t* qrcode, uint16_t value)
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
        if (i < 8) QrCodeSetModule(qrcode, qrcode->dimension - 1 - i, QRCODE_FINDER_SIZE + 1, v);
        // upper 7-bits starting LSB top-to-bottom right of bottom-left finder
        else QrCodeSetModule(qrcode, QRCODE_FINDER_SIZE + 1, qrcode->dimension - QRCODE_FINDER_SIZE - 8 + i, v);
    }
    // dark module
    QrCodeSetModule(qrcode, QRCODE_FINDER_SIZE + 1, qrcode->dimension - 1 - QRCODE_FINDER_SIZE, QRCODE_MODULE_DARK);
}

// Draw 18-bit version information (6-bit version number, 12-bit error-correction (18,6) Golay code)
static void QrCodeDrawVersionInfo(qrcode_t *qrcode, uint32_t value)
{
    // No version information on V1-V6
    if (qrcode->version < 7) return;
    for (int i = 0; i < 18; i++)
    {
        int v = (value >> i) & 1;
        //v = 100 + i; // for debug
        int col = i / QRCODE_VERSION_SIZE;
        int row = i % QRCODE_VERSION_SIZE;
        QrCodeSetModule(qrcode, col, qrcode->dimension - 1 - QRCODE_FINDER_SIZE - QRCODE_VERSION_SIZE + row, v);
        QrCodeSetModule(qrcode, qrcode->dimension - 1 - QRCODE_FINDER_SIZE - QRCODE_VERSION_SIZE + row, col, v);
    }
}


// Calculate 15-bit format information (2-bit error-correction level, 3-bit mask, 10-bit BCH error-correction; all masked)
static uint16_t QrCodeCalcFormatInfo(qrcode_t *qrcode, qrcode_error_correction_level_t errorCorrectionLevel, qrcode_mask_pattern_t maskPattern)
{
    // LLMMM
    int value = ((errorCorrectionLevel & 0x03) << 3) | (maskPattern & 0x07);

    // Calculate 10-bit Bose-Chaudhuri-Hocquenghem (15,5) error-correction
    int bch = value;
    for (int i = 0; i < 10; i++) bch = (bch << 1) ^ ((bch >> 9) * 0x0537);

    // 0LLMMMEEEEEEEEEE
    uint16_t format = (value << 10) | (bch & 0x03ff);
    static uint16_t formatMask = 0x5412;   // 0b0101010000010010
    format ^= formatMask;

    return format;
}


// Calculate 18-bit version information (6-bit version number, 12-bit error-correction (18,6) Golay code)
static uint32_t QrCodeCalcVersionInfo(qrcode_t *qrcode, int version)
{
    // Calculate 12-bit error-correction (18,6) Golay code
    int golay = version;
    for (int i = 0; i < 12; i++) golay = (golay << 1) ^ ((golay >> 11) * 0x1f25);
    long bits = (long)version << 12 | golay;

    uint32_t value = ((uint32_t)version << 12) | golay;
    return value;
}


static bool QrCodeCalculateMask(qrcode_mask_pattern_t maskPattern, int j, int i)
{
    switch (maskPattern)
    {
        case QRCODE_MASK_000: return ((i + j) & 1) == 0;
        case QRCODE_MASK_001: return (i & 1) == 0;
        case QRCODE_MASK_010: return j % 3 == 0;
        case QRCODE_MASK_011: return (i + j) % 3 == 0;
        case QRCODE_MASK_100: return (((i >> 1) + (j / 3)) & 1) == 0;
        case QRCODE_MASK_101: return ((i * j) & 1) + ((i * j) % 3) == 0;
        case QRCODE_MASK_110: return ((((i * j) & 1) + ((i * j) % 3)) & 1) == 0;
        case QRCODE_MASK_111: return ((((i * j) % 3) + ((i + j) & 1)) & 1) == 0;
    }
    return false;
}

static void QrCodeApplyMask(qrcode_t* qrcode, qrcode_mask_pattern_t maskPattern)
{
    for (int y = 0; y < qrcode->dimension; y++)
    {
        for (int x = 0; x < qrcode->dimension; x++)
        {
            qrcode_part_t part = QrCodeIdentifyModule(qrcode, x, y);
            if (part == QRCODE_PART_CONTENT)
            {
                bool mask = QrCodeCalculateMask(maskPattern, x, y);
                
                if (mask)
                {
                    int module = QrCodeGetModule(qrcode, x, y);
                    int value = 1 ^ module;
                    //value = mask ? QRCODE_MODULE_DARK : QRCODE_MODULE_LIGHT; // DEBUG
                    QrCodeSetModule(qrcode, x, y, value);
                }
            }
        }
    }
}

static void QrCodeCursorReset(qrcode_t* qrcode, int* x, int* y)
{
    *x = qrcode->dimension - 1;
    *y = qrcode->dimension - 1;
}

static bool QrCodeCursorAdvance(qrcode_t* qrcode, int* x, int* y)
{
    while (*x >= 0)
    {
        // Right-hand side of 2-module column? (otherwise, left-hand side)
        if ((*x & 1) ^ (*x > QRCODE_TIMING_OFFSET ? 1 : 0))
        {
            (*x)--;
        }
        else // Left-hand side
        {
            (*x)++;
            // Upwards? (otherwise, downwards)
            if (((*x - (*x > QRCODE_TIMING_OFFSET ? 1 : 0)) / 2) & 1)
            {
                if (*y <= 0) *x -= 2;
                else (*y)--;
            }
            else
            {
                if (*y >= qrcode->dimension - 1) *x -= 2;
                else (*y)++;
            }
        }
        if (QrCodeIdentifyModule(qrcode, *x, *y) == QRCODE_PART_CONTENT) return true;
    }
    return false;
}


// Generate the code
bool QrCodeGenerate(qrcode_t *qrcode, const char *text)
{
    QrCodeDrawFinder(qrcode, QRCODE_FINDER_SIZE / 2, QRCODE_FINDER_SIZE / 2);
    QrCodeDrawFinder(qrcode, qrcode->dimension - 1 - QRCODE_FINDER_SIZE / 2, QRCODE_FINDER_SIZE / 2);
    QrCodeDrawFinder(qrcode, QRCODE_FINDER_SIZE / 2, qrcode->dimension - 1 - QRCODE_FINDER_SIZE / 2);

    QrCodeDrawTiming(qrcode);

    // Alignment
    for (int hi = 0, h; (h = QrCodeAlignmentCoordinates(qrcode->version, hi)) > 0; hi++)
    {
        for (int vi = 0, v; (v = QrCodeAlignmentCoordinates(qrcode->version, vi)) > 0; vi++)
        {
            if (h <= QRCODE_FINDER_SIZE && v <= QRCODE_FINDER_SIZE) continue;                     // Obscured by top-left finder
            if (h >= qrcode->dimension - 1 - QRCODE_FINDER_SIZE && v <= QRCODE_FINDER_SIZE) continue;  // Obscured by top-right finder
            if (h <= QRCODE_FINDER_SIZE && v >= qrcode->dimension - 1 - QRCODE_FINDER_SIZE) continue;  // Obscured by bottom-left finder
            QrCodeDrawAlignment(qrcode, h, v);
        }
    }


    // TODO: Code generation, ECC, masking
    qrcode_error_correction_level_t errorCorrectionLevel = QRCODE_ECL_M;
    int cursorX, cursorY;
    QrCodeCursorReset(qrcode, &cursorX, &cursorY);
    int module = 0;
    do
    {
        int bit = 7 - module % 8;
        int byte = module / 8 % 2;
int value = (byte ? 200 : 100) + bit;
//value = bit & 1;
value = 0;
        QrCodeSetModule(qrcode, cursorX, cursorY, value);
        module++;
    } while (QrCodeCursorAdvance(qrcode, &cursorX, &cursorY));

    // TODO: Better masking
    qrcode->maskPattern = QRCODE_MASK_000;
    QrCodeApplyMask(qrcode, qrcode->maskPattern);

    // TODO: Evaluate masking attempt

    uint16_t formatInfo = QrCodeCalcFormatInfo(qrcode, qrcode->errorCorrectionLevel, qrcode->maskPattern);
    QrCodeDrawFormatInfo(qrcode, formatInfo);
    
    // Version info (V7+) (additional 36 modules, total 67 for format+version)
    if (qrcode->version >= 7)
    {
        uint32_t versionInfo = QrCodeCalcVersionInfo(qrcode, qrcode->version);
        QrCodeDrawVersionInfo(qrcode, versionInfo);
    }

#ifdef QRCODE_DEBUG_DUMP
// Debug dump
QrCodeDebugDump(qrcode);
#endif

    return false;
}


void QrCodePrintLarge(qrcode_t* qrcode, FILE* fp, bool invert)
{
    for (int y = -qrcode->quiet; y < qrcode->dimension + qrcode->quiet; y++)
    {
        for (int x = -qrcode->quiet; x < qrcode->dimension + qrcode->quiet; x++)
        {
            int bit = QrCodeGetModule(qrcode, x, y) ^ (invert ? 0x1 : 0x0);
            if (bit != 0) fprintf(fp, "██"); // '\u{2588}' block
            else fprintf(fp, "  ");          // '\u{0020}' space
        }
        fprintf(fp, "\n");
    }
}

void QrCodePrintCompact(qrcode_t* qrcode, FILE *fp, bool invert)
{
    for (int y = -qrcode->quiet; y < qrcode->dimension + qrcode->quiet; y += 2)
    {
        for (int x = -qrcode->quiet; x < qrcode->dimension + qrcode->quiet; x++)
        {
            int bitU = QrCodeGetModule(qrcode, x, y);
            int bitL = (y + 1 < qrcode->dimension + qrcode->quiet) ? QrCodeGetModule(qrcode, x, y + 1) : (invert ? 0 : 1);
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

