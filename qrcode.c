// QR Code Generator
// Dan Jackson, 2020

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "qrcode.h"

#ifdef _DEBUG
//#define QR_DEBUG_DUMP
#endif

#ifdef QR_DEBUG_DUMP
#include <stdio.h>
#endif


//#define QRCODE_DIMENSION_TO_VERSION(_n) (((_n) - 17) / 4)
#define QRCODE_FINDER_SIZE 7
#define QRCODE_TIMING_OFFSET 6
#define QRCODE_VERSION_SIZE 3
#define QRCODE_ALIGNMENT_RADIUS 2
#define QRCODE_MODULE_LIGHT 0
#define QRCODE_MODULE_DARK 1

#define QRCODE_SIZE_ECL 2               // 2-bit code for error correction
#define QRCODE_SIZE_MASK 3              // 3-bit code for mask size
#define QRCODE_SIZE_BCH 10              // 10,5 BCH for format information
#define QRCODE_SIZE_MODE_INDICATOR 4    // 4-bit mode indicator

#define QRCODE_PAD_CODEWORDS 0xec11 // Pad codewords 0b11101100=0xec 0b00010001=0x11

#ifdef QR_DEBUG_DUMP
static int qrWritingCodeword = 0;    // for QrCodeCursorWrite()
#endif

static bool QrCodeBufferRead(uint8_t* scratchBuffer, size_t bitPosition)
{
    return (scratchBuffer[bitPosition >> 3] & (1 << (7 - (bitPosition & 7)))) ? 1 : 0;
}

// Write bits to buffer
static size_t QrCodeBufferAppend(uint8_t *writeBuffer, size_t writePosition, uint32_t value, size_t bitCount)
{
//dump_scratch(writeBuffer, writePosition, "BEFORE");
//char bin[65]; memset(bin, '0', 32); _itoa(value, bin+32, 2);
//printf("Writing last %d bits from 0x%08X=%s\n", (int)bitCount, value, bin + strlen(bin) - bitCount);
    for (size_t i = 0; i < bitCount; i++)
    {
        uint8_t *writeByte = writeBuffer + ((writePosition + i) >> 3);
        int writeBit = 7 - ((writePosition + i) & 0x07);
        int writeMask = (1 << writeBit);
        int readMask = (1 << (bitCount - 1 - i));
//printf("bit#%d +%d writeBit=%d writeMask=%02x readMask=%02x read=%d\n", (int)i, (int)((writePosition + i) >> 3), writeBit, writeMask, readMask, (value & readMask) ? 1 : 0);
        *writeByte = (*writeByte & ~writeMask) | ((value & readMask) ? writeMask : 0);
    }
//dump_scratch(writeBuffer, writePosition + bitCount, "AFTER");
    return bitCount;
}

// [Table 13] Number of error correction blocks (count of error-correction-blocks; for each error-correction level and version)
static const int8_t qrcode_ecc_block_count[1 << QRCODE_SIZE_ECL][QRCODE_VERSION_MAX + 1] = {
    //-, 1, 2, 3, 4, 5, 6, 7, 8, 9,10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40
    { 0, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5,  5,  8,  9,  9, 10, 10, 11, 13, 14, 16, 17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49 },    // 0b00 Medium
    { 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4,  4,  4,  4,  4,  6,  6,  6,  6,  7,  8,  8,  9,  9, 10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25 },    // 0b01 Low
    { 0, 1, 1, 2, 4, 4, 4, 5, 6, 8, 8, 11, 11, 16, 16, 18, 16, 19, 21, 25, 25, 25, 34, 30, 32, 35, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66, 70, 74, 77, 81 },    // 0b10 High
    { 0, 1, 1, 2, 2, 4, 4, 6, 6, 8, 8,  8, 10, 12, 16, 12, 17, 16, 18, 21, 20, 23, 23, 25, 27, 29, 34, 34, 35, 38, 40, 43, 45, 48, 51, 53, 56, 59, 62, 65, 68 },    // 0b11 Quartile
};

// [Table 13] Number of error correction codewords (count of data 8-bit codewords in each block; for each error-correction level and version)
static const int8_t qrcode_ecc_block_codewords[1 << QRCODE_SIZE_ECL][QRCODE_VERSION_MAX + 1] = {
    //-,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40
    { 0, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26, 30, 22, 22, 24, 24, 28, 28, 26, 26, 26, 26, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28 },  // 0b00 Medium
    { 0,  7, 10, 15, 20, 26, 18, 20, 24, 30, 18, 20, 24, 26, 30, 22, 24, 28, 30, 28, 28, 28, 28, 30, 30, 26, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 },  // 0b01 Low
    { 0, 17, 28, 22, 16, 22, 28, 26, 26, 24, 28, 24, 28, 22, 24, 24, 30, 28, 28, 26, 28, 30, 24, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 },  // 0b10 High
    { 0, 13, 22, 18, 26, 18, 24, 18, 22, 20, 24, 28, 26, 24, 20, 30, 24, 28, 28, 26, 30, 28, 30, 30, 30, 30, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30 },  // 0b11 Quartile
};
#define QRCODE_ECC_CODEWORDS_MAX 30

// Total number of data bits available in the codewords (cooked: after ecc and remainder)
static size_t QrCodeDataCapacity(int version, qrcode_error_correction_level_t errorCorrectionLevel)
{
    size_t capacityCodewords = QRCODE_TOTAL_CAPACITY(version) / 8;
    size_t eccCodewords = (size_t)qrcode_ecc_block_count[errorCorrectionLevel][version] * qrcode_ecc_block_codewords[errorCorrectionLevel][version];
    size_t dataCapacityCodewords = capacityCodewords - eccCodewords;
    return dataCapacityCodewords * 8;
}

// Check the buffer is entirely compatible with a numeric encoding
static bool QrCodeSegmentNumericCheck(const char* text, size_t charCount)
{
    for (size_t i = 0; i < charCount; i++)
    {
        if (text[i] < '0' || text[i] > '9') return false;
    }
    return true;
}

// Returns Alphanumeric index for a character (0-44), or -1 if none
static int QrCodeSegmentAlphanumericIndex(char c, bool mayUppercase)
{
    if (c >= '0' && c <= '9') return c - '0';      // 0-9 (0-9)
    if (c >= 'A' && c <= 'Z') return 10 + c - 'A'; // A-Z (10-35)
    if (mayUppercase && c >= 'a' && c <= 'z') return 10 + c - 'a'; // transform to upper-case A-Z (10-35)
    //  36,  37,  38,  39,  40,  41,  42,  43,  44
    // ' ', '$', '%', '*', '+', '-', '.', '/', ':'
    //0x20,0x24,0x25,0x2A,0x2B,0x2D,0x2E,0x2F,0x3A
    static const char* symbols = " $%*+-./:";
    const char* p = strchr(symbols, c);
    if (p != NULL) return 36 + (int)(p - symbols);
    return -1;
}

// Check the buffer is entirely compatible with an alphanumeric encoding
static bool QrCodeSegmentAlphanumericCheck(const char* text, size_t charCount, bool mayUppercase)
{
    for (size_t i = 0; i < charCount; i++)
    {
        if (QrCodeSegmentAlphanumericIndex(text[i], mayUppercase) < 0) return false;
    }
    return true;
}

// Calculate segment buffer sizes (payload only -- excluding 4-bit mode indicator and version-specific sized char count)
#define QRCODE_SEGMENT_NUMERIC_BUFFER_BITS(_c) ((10 * ((_c) / 3)) + (((_c) % 3) * 4) - (((_c) % 3) / 2))
#define QRCODE_SEGMENT_ALPHANUMERIC_BUFFER_BITS(_c) (11 * ((_c) >> 1) + 6 * ((_c) & 1))
#define QRCODE_SEGMENT_8_BIT_BUFFER_BITS(_c) (8 * (_c))

void QrCodeSegmentAppend(qrcode_t *qrcode, qrcode_segment_t *segment, qrcode_mode_indicator_t mode, const char *text, size_t charCount, bool mayUppercase)
{
    memset(segment, 0, sizeof(*segment));
    if (charCount == QRCODE_TEXT_LENGTH) charCount = strlen(text);
    segment->mode = mode;
    segment->charCount = charCount;
    segment->text = text;
    segment->next = NULL;

    // Find the most efficient mode for the entire given string (TODO: Analyize sub-strings for more efficient mode switching)
    if (segment->mode == QRCODE_MODE_INDICATOR_AUTOMATIC)
    {
        if (QrCodeSegmentNumericCheck(text, segment->charCount)) segment->mode = QRCODE_MODE_INDICATOR_NUMERIC;
        else if (QrCodeSegmentAlphanumericCheck(text, segment->charCount, mayUppercase)) segment->mode = QRCODE_MODE_INDICATOR_ALPHANUMERIC;
        else segment->mode = QRCODE_MODE_INDICATOR_8_BIT;
    }

    qrcode_segment_t* seg = qrcode->firstSegment;
    if (seg == NULL) { qrcode->firstSegment = segment; }
    else
    {
        while (seg->next != NULL) seg = seg->next;
        seg->next = segment;
    }

    qrcode->prepared = false;
}

// Writes an 8-bit text segment
static size_t QrCodeSegmentWrite8bit(uint8_t *buffer, size_t bitPosition, const char *text, size_t charCount)
{
    size_t bitsWritten = 0;
    for (size_t i = 0; i < charCount; i++)
    {
        bitsWritten += QrCodeBufferAppend(buffer, bitPosition + bitsWritten, text[i], 8);
    }
    return bitsWritten;
}

// Writes a numeric segment, buffer size should be at least: QRCODE_SEGMENT_NUMERIC_BUFFER_BYTES(charCount)
static size_t QrCodeSegmentWriteNumeric(uint8_t *buffer, size_t bitPosition, const char *text, size_t charCount)
{
    size_t bitsWritten = 0;
    for (size_t i = 0; i < charCount; )
    {
        size_t remain = (charCount - i) > 3 ? 3 : (charCount - i);
        int value = text[i] - '0';
        int bits = 4;
        // Maximal groups of 3/2/1 digits encoded to 10/7/4-bit binary
        if (remain > 1) { value = value * 10 + text[i + 1] - '0'; bits += 3; }
        if (remain > 2) { value = value * 10 + text[i + 2] - '0'; bits += 3; }
        bitsWritten += QrCodeBufferAppend(buffer, bitPosition + bitsWritten, value, bits);
        i += remain;
    }
    return bitsWritten;
}

// Writes an alphanumeric segment, buffer size should be at least: QRCODE_SEGMENT_ALPHANUMERIC_BUFFER_BYTES(charCount)
static size_t QrCodeSegmentWriteAlphanumeric(uint8_t *buffer, size_t bitPosition, const char *text, size_t charCount)
{
    size_t bitsWritten = 0;
    for (size_t i = 0; i < charCount; )
    {
        size_t remain = (charCount - i) > 2 ? 2 : (charCount - i);
        int value = QrCodeSegmentAlphanumericIndex(text[i], true);
        int bits = 6;
        // Pairs combined(a * 45 + b) encoded as 11 - bit; odd remainder encoded as 6 - bit.
        if (remain > 1) { value = value * 45 + QrCodeSegmentAlphanumericIndex(text[i + 1], true); bits += 5; }
        bitsWritten += QrCodeBufferAppend(buffer, bitPosition + bitsWritten, value, bits);
        i += remain;
    }
    return bitsWritten;
}

// Number of bits in Character Count Indicator
static size_t QrCodeBitsInCharacterCount(int version, qrcode_mode_indicator_t mode)
{
    // Bands are (1-9), (10-26), (27-40)
    switch (mode)
    {
    case QRCODE_MODE_INDICATOR_NUMERIC:
        return (version < 10) ? 10 : (version < 27) ? 12 : 14;  // Numeric
    case QRCODE_MODE_INDICATOR_ALPHANUMERIC:
        return (version < 10) ? 9 : (version < 27) ? 11 : 13; // Alphanumeric
    case QRCODE_MODE_INDICATOR_8_BIT:
        return (version < 10) ? 8 : (version < 27) ? 16 : 16; // 8-bit
    case QRCODE_MODE_INDICATOR_KANJI:
        return (version < 10) ? 8 : (version < 27) ? 10 : 12; // Kanji
    default:
        return 0;
    }
}

// Size of a segment (including 4-bit mode indicator, version-specific sized char count, mode-specific encoding)
static size_t QrCodeSegmentSize(qrcode_segment_t *segment, int version)
{
    size_t bits = 0;
    bits += QRCODE_SIZE_MODE_INDICATOR;
    bits += QrCodeBitsInCharacterCount(version, segment->mode);
    switch (segment->mode)
    {
        case QRCODE_MODE_INDICATOR_NUMERIC:
            bits += QRCODE_SEGMENT_NUMERIC_BUFFER_BITS(segment->charCount);
            break;
        case QRCODE_MODE_INDICATOR_ALPHANUMERIC:
            bits += QRCODE_SEGMENT_ALPHANUMERIC_BUFFER_BITS(segment->charCount);
            break;
        case QRCODE_MODE_INDICATOR_8_BIT:
            bits += QRCODE_SEGMENT_8_BIT_BUFFER_BITS(segment->charCount);
            break;
        case QRCODE_MODE_INDICATOR_ECI:
        {
            uint32_t eciAssigmentNumber = (uint32_t)segment->charCount;
            bits += (eciAssigmentNumber <= 0xFF) ? 8 : (eciAssigmentNumber <= 0x3FFF) ? 16 : 24;
            break;
        }
        default:
            // Other segments?
            break;
    }
    return bits;
}

// Write a segment (the 4-bit mode indicator, version-specific sized char count, mode-specific encoding)
static size_t QrCodeSegmentWrite(qrcode_segment_t *segment, int version, uint8_t *buffer, size_t bitPosition)
{
    size_t bitsWritten = 0;

    // Write 4-bit mode
    bitsWritten += QrCodeBufferAppend(buffer, bitPosition + bitsWritten, segment->mode, QRCODE_SIZE_MODE_INDICATOR);

    // Write mode-specific content
    if (segment->mode == QRCODE_MODE_INDICATOR_NUMERIC)
    {
        size_t countBits = QrCodeBitsInCharacterCount(version, segment->mode);
        bitsWritten += QrCodeBufferAppend(buffer, bitPosition + bitsWritten, (uint32_t)segment->charCount, countBits);
        bitsWritten += QrCodeSegmentWriteNumeric(buffer, bitPosition + bitsWritten, segment->text, segment->charCount);
    }
    else if (segment->mode == QRCODE_MODE_INDICATOR_ALPHANUMERIC)
    {
        size_t countBits = QrCodeBitsInCharacterCount(version, segment->mode);
        bitsWritten += QrCodeBufferAppend(buffer, bitPosition + bitsWritten, (uint32_t)segment->charCount, countBits);
        bitsWritten += QrCodeSegmentWriteAlphanumeric(buffer, bitPosition + bitsWritten, segment->text, segment->charCount);
    }
    else if (segment->mode == QRCODE_MODE_INDICATOR_8_BIT)
    {
        size_t countBits = QrCodeBitsInCharacterCount(version, segment->mode);
        bitsWritten += QrCodeBufferAppend(buffer, bitPosition + bitsWritten, (uint32_t)segment->charCount, countBits);
        bitsWritten += QrCodeSegmentWrite8bit(buffer, bitPosition + bitsWritten, segment->text, segment->charCount);
    }
    else if (segment->mode == QRCODE_MODE_INDICATOR_ECI)
    {
        // TODO: Should use a union rather than reuse 'charCount' for this
        uint32_t eciAssigmentNumber = (uint32_t)segment->charCount;
        size_t countBits = 0;
        if (eciAssigmentNumber <= 0xFF) { countBits = 8; }                                          // 0-127 8-bit 0vvvvvvv
        else if (eciAssigmentNumber <= 0x3FFF) { countBits = 16; eciAssigmentNumber = 0x8000 | eciAssigmentNumber; } // 128-16383 16-bit 10vvvvvv vvvvvvvv
        else { countBits = 24; eciAssigmentNumber = 0xC00000 | (eciAssigmentNumber % 1000000); }    // 16384 to 999999 24-bit 110vvvvv vvvvvvvv vvvvvvvv
        bitsWritten += QrCodeBufferAppend(buffer, bitPosition + bitsWritten, eciAssigmentNumber, countBits);
    }
    else
    {
        ;   // No content
    }
    return bitsWritten;
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
void QrCodeInit(qrcode_t *qrcode, int maxVersion, qrcode_error_correction_level_t errorCorrectionLevel)
{
    memset(qrcode, 0, sizeof(qrcode_t));
    qrcode->version = QRCODE_VERSION_AUTO;
    qrcode->maxVersion = maxVersion;
    qrcode->errorCorrectionLevel = errorCorrectionLevel; // QRCODE_ECL_M;
    qrcode->maskPattern = QRCODE_MASK_AUTO;
    qrcode->optimizeEcc = true;
    qrcode->prepared = false;
}

int QrCodeModuleGet(qrcode_t *qrcode, int x, int y)
{
    if (x < 0 || y < 0 || x >= qrcode->dimension || y >= qrcode->dimension) return 0; // quiet
    int offset = y * qrcode->dimension + x;
#ifdef QR_DEBUG_DUMP
    if (qrcode->buffer == NULL || offset < 0 || offset >= qrcode->bufferSize) return -1;
    return qrcode->buffer[offset];
#else
    if (qrcode->buffer == NULL || offset < 0 || (offset >> 3) >= qrcode->bufferSize) return -1;
    return qrcode->buffer[offset >> 3] & (1 << (7 - (offset & 7))) ? 1 : 0;
#endif
}

static void QrCodeModuleSet(qrcode_t *qrcode, int x, int y, int value)
{
    if (x < 0 || y < 0 || x >= qrcode->dimension || y >= qrcode->dimension) return; // quiet
    int offset = y * qrcode->dimension + x;
#ifdef QR_DEBUG_DUMP
    if (qrcode->buffer == NULL || offset < 0 || offset >= qrcode->bufferSize) return;
    qrcode->buffer[offset] = value;
#else
    if (qrcode->buffer == NULL || offset < 0 || (offset >> 3) >= qrcode->bufferSize) return;
    uint8_t mask = (1 << (7 - (offset & 7)));
    qrcode->buffer[offset >> 3] = (qrcode->buffer[offset >> 3] & ~mask) | (value ? mask : 0);
#endif
}


// Determines which part a given module coordinate belongs to.
qrcode_part_t QrCodeIdentifyModule(qrcode_t* qrcode, int x, int y, int *index)
{
    int dimension = qrcode->dimension;
    int dummy;
    if (index == NULL) {
        index = &dummy;
    }

    // Quiet zone
    if (x < 0 || y < 0 || x >= dimension || y >= dimension) { *index = QRCODE_MODULE_LIGHT; return QRCODE_PART_QUIET; } // Outside

    // Finders
    for (int f = 0; f < 3; f++)
    {
        int dx = abs(x - (f & 1 ? dimension - 1 - QRCODE_FINDER_SIZE / 2 : QRCODE_FINDER_SIZE / 2));
        int dy = abs(y - (f & 2 ? dimension - 1 - QRCODE_FINDER_SIZE / 2 : QRCODE_FINDER_SIZE / 2));
        if (dx == 0 && dy == 0) { *index = -1; return QRCODE_PART_FINDER; }  // origin
        if (dx <= 1 + QRCODE_FINDER_SIZE / 2 && dy <= 1 + QRCODE_FINDER_SIZE / 2)
        {
            if (dx == 1 + QRCODE_FINDER_SIZE / 2 || dy == 1 + QRCODE_FINDER_SIZE / 2) { *index = QRCODE_MODULE_LIGHT; return QRCODE_PART_SEPARATOR; }
            *index = (dx > dy ? dx : dy) & 1 ? QRCODE_MODULE_DARK : QRCODE_MODULE_LIGHT;
            return QRCODE_PART_FINDER;
        }
    }

    // Alignment
    for (int hi = 0, h; (h = QrCodeAlignmentCoordinates(qrcode->version, hi)) > 0; hi++)
    {
        for (int vi = 0, v; (v = QrCodeAlignmentCoordinates(qrcode->version, vi)) > 0; vi++)
        {
            if (h <= QRCODE_FINDER_SIZE && v <= QRCODE_FINDER_SIZE) continue;                  // Obscured by top-left finder
            if (h >= dimension - 1 - QRCODE_FINDER_SIZE && v <= QRCODE_FINDER_SIZE) continue;  // Obscured by top-right finder
            if (h <= QRCODE_FINDER_SIZE && v >= dimension - 1 - QRCODE_FINDER_SIZE) continue;  // Obscured by bottom-left finder
            if (x == h && y == v) { *index = -1; return QRCODE_PART_ALIGNMENT; }
            if (x >= h - QRCODE_ALIGNMENT_RADIUS && x <= h + QRCODE_ALIGNMENT_RADIUS && y >= v - QRCODE_ALIGNMENT_RADIUS && y <= v + QRCODE_ALIGNMENT_RADIUS)
            {
                *index = ((abs(x - h) > abs(y - h) ? abs(x - h) : abs(y - h)) & 1) ? QRCODE_MODULE_LIGHT : QRCODE_MODULE_DARK;
                return QRCODE_PART_ALIGNMENT;
            }
        }
    }

    // Timing
    if (y == QRCODE_TIMING_OFFSET && x > QRCODE_FINDER_SIZE && x < dimension - 1 - QRCODE_FINDER_SIZE) { *index = ((x ^ y) & 1) ? QRCODE_MODULE_LIGHT : QRCODE_MODULE_DARK; return QRCODE_PART_TIMING; } // Timing: horizontal
    if (x == QRCODE_TIMING_OFFSET && y > QRCODE_FINDER_SIZE && y < dimension - 1 - QRCODE_FINDER_SIZE) { *index = ((x ^ y) & 1) ? QRCODE_MODULE_LIGHT : QRCODE_MODULE_DARK; return QRCODE_PART_TIMING; } // Timing: vertical

    // --- Encoding region ---
    // Format info (2*15+1=31 modules)
    if (x == QRCODE_FINDER_SIZE + 1 && y <= QRCODE_FINDER_SIZE + 1 && y != QRCODE_TIMING_OFFSET) { *index = y - (y >= QRCODE_TIMING_OFFSET ? 1 : 0); return QRCODE_PART_FORMAT; } // Format info (right of top-left finder)
    if (y == QRCODE_FINDER_SIZE + 1 && x <= QRCODE_FINDER_SIZE + 1 && x != QRCODE_TIMING_OFFSET) { *index = 14 - x + (x >= QRCODE_TIMING_OFFSET ? 1 : 0); return QRCODE_PART_FORMAT; } // Format info (bottom of top-left finder)
    if (y == QRCODE_FINDER_SIZE + 1 && x >= dimension - QRCODE_FINDER_SIZE - 1) { *index = dimension - 1 - x; return QRCODE_PART_FORMAT; }  // Format info (bottom of top-right finder)
    if (x == QRCODE_FINDER_SIZE + 1 && y == dimension - QRCODE_FINDER_SIZE - 1) { *index = -1; return QRCODE_PART_FORMAT; }  // Format info (black module right of bottom-left finder)
    if (x == QRCODE_FINDER_SIZE + 1 && y >= dimension - QRCODE_FINDER_SIZE - 1) { *index = y + 14 - (dimension - 1); return QRCODE_PART_FORMAT; }  // Format info (right of bottom-left finder)

    // Version info (V7+) (additional 2*18=36 modules, total 67 for format+version)
    if (qrcode->version >= 7)
    {
        if (x < QRCODE_TIMING_OFFSET && y >= dimension - QRCODE_FINDER_SIZE - 1 - QRCODE_VERSION_SIZE && y < dimension - QRCODE_FINDER_SIZE - 1) { *index = x * QRCODE_VERSION_SIZE + (y - (dimension - QRCODE_FINDER_SIZE - 1 - QRCODE_VERSION_SIZE)); return QRCODE_PART_VERSION; }  // Bottom-left version
        if (y < QRCODE_TIMING_OFFSET && x >= dimension - QRCODE_FINDER_SIZE - 1 - QRCODE_VERSION_SIZE && x < dimension - QRCODE_FINDER_SIZE - 1) { *index = y * QRCODE_VERSION_SIZE + (x - (dimension - QRCODE_FINDER_SIZE - 1 - QRCODE_VERSION_SIZE)); return QRCODE_PART_VERSION; }  // Top-right version
    }

    // Content
    *index = -1;
    return QRCODE_PART_CONTENT;
}


// Draw finder and separator
static void QrCodeDrawFinder(qrcode_t *qrcode, int ox, int oy)
{
    for (int y = -QRCODE_FINDER_SIZE / 2 - 1; y <= QRCODE_FINDER_SIZE / 2 + 1; y++)
    {
        for (int x = -QRCODE_FINDER_SIZE / 2 - 1; x <= QRCODE_FINDER_SIZE / 2 + 1; x++)
        {
            int value = (abs(x) > abs(y) ? abs(x) : abs(y)) & 1;
            if (x == 0 && y == 0) value = QRCODE_MODULE_DARK;
            QrCodeModuleSet(qrcode, ox + x, oy + y, value);
        }
    }
}

static void QrCodeDrawTiming(qrcode_t *qrcode)
{
    for (int i = QRCODE_FINDER_SIZE + 1; i < qrcode->dimension - QRCODE_FINDER_SIZE - 1; i++)
    {
        int value = (~i & 1);
        QrCodeModuleSet(qrcode, i, QRCODE_TIMING_OFFSET, value);
        QrCodeModuleSet(qrcode, QRCODE_TIMING_OFFSET, i, value);
    }
}

static void QrCodeDrawAlignment(qrcode_t* qrcode, int ox, int oy)
{
    for (int y = -QRCODE_ALIGNMENT_RADIUS; y <= QRCODE_ALIGNMENT_RADIUS; y++)
    {
        for (int x = -QRCODE_ALIGNMENT_RADIUS; x <= QRCODE_ALIGNMENT_RADIUS; x++)
        {
            int value = 1 - ((abs(x) > abs(y) ? abs(x) : abs(y)) & 1);
            QrCodeModuleSet(qrcode, ox + x, oy + y, value);
        }
    }
}

// Draw 15-bit format information (2-bit error-correction level, 3-bit mask, 10-bit BCH error-correction; all masked)
static void QrCodeDrawFormatInfo(qrcode_t* qrcode, uint16_t value)
{
    for (int i = 0; i < 15; i++)
    {
        int v = (value >> i) & 1;
#ifdef QR_DEBUG_DUMP
        v = ((40 + i) << 1) | v; // for debug
#endif
        // 15-bits starting LSB clockwise from top-left finder avoiding timing strips
        if (i < 6) QrCodeModuleSet(qrcode, QRCODE_FINDER_SIZE + 1, i, v);
        else if (i == 6) QrCodeModuleSet(qrcode, QRCODE_FINDER_SIZE + 1, QRCODE_FINDER_SIZE, v);
        else if (i == 7) QrCodeModuleSet(qrcode, QRCODE_FINDER_SIZE + 1, QRCODE_FINDER_SIZE + 1, v);
        else if (i == 8) QrCodeModuleSet(qrcode, QRCODE_FINDER_SIZE, QRCODE_FINDER_SIZE + 1, v);
        else QrCodeModuleSet(qrcode, 14 - i, QRCODE_FINDER_SIZE + 1, v);

        // lower 8-bits starting LSB right-to-left underneath top-right finder
        if (i < 8) QrCodeModuleSet(qrcode, qrcode->dimension - 1 - i, QRCODE_FINDER_SIZE + 1, v);
        // upper 7-bits starting LSB top-to-bottom right of bottom-left finder
        else QrCodeModuleSet(qrcode, QRCODE_FINDER_SIZE + 1, qrcode->dimension - QRCODE_FINDER_SIZE - 8 + i, v);
    }
    // dark module
    {
        int v = QRCODE_MODULE_DARK;
#ifdef QR_DEBUG_DUMP
        v = ((40 + 19) << 1) | v; // for debug
#endif
        QrCodeModuleSet(qrcode, QRCODE_FINDER_SIZE + 1, qrcode->dimension - 1 - QRCODE_FINDER_SIZE, v);
    }
}

// Draw 18-bit version information (6-bit version number, 12-bit error-correction (18,6) Golay code)
static void QrCodeDrawVersionInfo(qrcode_t *qrcode, uint32_t value)
{
    // No version information on V1-V6
    if (qrcode->version < 7) return;
    for (int i = 0; i < 18; i++)
    {
        int v = (value >> i) & 1;
#ifdef QR_DEBUG_DUMP
        v = ((60 + i) << 1) | v; // for debug
#endif
        int col = i / QRCODE_VERSION_SIZE;
        int row = i % QRCODE_VERSION_SIZE;
        QrCodeModuleSet(qrcode, col, qrcode->dimension - 1 - QRCODE_FINDER_SIZE - QRCODE_VERSION_SIZE + row, v);
        QrCodeModuleSet(qrcode, qrcode->dimension - 1 - QRCODE_FINDER_SIZE - QRCODE_VERSION_SIZE + row, col, v);
    }
}

// Calculate 15-bit format information (2-bit error-correction level, 3-bit mask, 10-bit BCH error-correction; all masked)
static uint16_t QrCodeCalcFormatInfo(qrcode_t *qrcode, qrcode_error_correction_level_t errorCorrectionLevel, qrcode_mask_pattern_t maskPattern)
{
    // LLMMM
    int value = ((errorCorrectionLevel & ((1 << QRCODE_SIZE_ECL) - 1)) << QRCODE_SIZE_MASK) | (maskPattern & ((1 << QRCODE_SIZE_MASK) - 1));

    // Calculate 10-bit Bose-Chaudhuri-Hocquenghem (15,5) error-correction
    int bch = value;
    for (int i = 0; i < QRCODE_SIZE_BCH; i++) bch = (bch << 1) ^ ((bch >> (QRCODE_SIZE_BCH - 1)) * 0x0537);

    // 0LLMMMEEEEEEEEEE
    uint16_t format = (value << QRCODE_SIZE_BCH) | (bch & ((1 << QRCODE_SIZE_BCH) - 1));
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
        default: return false;
    }
}

static void QrCodeApplyMask(qrcode_t* qrcode, qrcode_mask_pattern_t maskPattern)
{
    for (int y = 0; y < qrcode->dimension; y++)
    {
        for (int x = 0; x < qrcode->dimension; x++)
        {
            qrcode_part_t part = QrCodeIdentifyModule(qrcode, x, y, NULL);
            if (part == QRCODE_PART_CONTENT)
            {
                bool mask = QrCodeCalculateMask(maskPattern, x, y);
                
                if (mask)
                {
                    int module = QrCodeModuleGet(qrcode, x, y);
                    int value = 1 ^ module;
                    //value = mask ? QRCODE_MODULE_DARK : QRCODE_MODULE_LIGHT; // DEBUG
                    QrCodeModuleSet(qrcode, x, y, value);
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
        if (QrCodeIdentifyModule(qrcode, *x, *y, NULL) == QRCODE_PART_CONTENT) return true;
    }
    return false;
}

// Total number of data bits from segments in the QR Code
// (does not include bits added when space for: 4-bit terminator mode indicator, 0-padding to byte, padding bytes; or ECC)
static size_t QrCodeBitsUsed(qrcode_t *qrcode)
{
    size_t sizeBits = 0;
    for (qrcode_segment_t* seg = qrcode->firstSegment; seg != NULL; seg = seg->next)
    {
        sizeBits += QrCodeSegmentSize(seg, qrcode->version);
    }
    return sizeBits;
}

// Set version
static bool QrCodePrepare(qrcode_t* qrcode)
{
    if (qrcode->prepared && qrcode->dimension <= 0) return false;   // does not fit
    if (qrcode->prepared) return true;
    int spareCapacity = -1;
    qrcode->dimension = 0;
    // Find the smallest version that will fit
    if (qrcode->version == QRCODE_VERSION_AUTO)
    {
        for (qrcode->version = QRCODE_VERSION_MIN; qrcode->version <= qrcode->maxVersion; qrcode->version++)
        {
            qrcode->sizeBits = QrCodeBitsUsed(qrcode);
            qrcode->dataCapacity = QrCodeDataCapacity(qrcode->version, qrcode->errorCorrectionLevel);
            spareCapacity = (int)qrcode->dataCapacity - (int)qrcode->sizeBits;
            if (spareCapacity >= 0) break;
        }
        if (spareCapacity < 0) return false;  // Chosen version / none fit
    }
    else
    {
        // Check the requested version
        qrcode->sizeBits = QrCodeBitsUsed(qrcode);
        qrcode->dataCapacity = QrCodeDataCapacity(qrcode->version, qrcode->errorCorrectionLevel);
        spareCapacity = (int)qrcode->dataCapacity - (int)qrcode->sizeBits;
        if (spareCapacity < 0) return false;  // Chosen version / none fit
    }

    // Cache dimension for the chosen version
    qrcode->dimension = QRCODE_VERSION_TO_DIMENSION(qrcode->version);

    // Required size of QR Code buffer
    qrcode->bufferSize = QRCODE_BUFFER_SIZE(qrcode->version);
#ifdef QR_DEBUG_DUMP
    qrcode->bufferSize *= 8;
#endif

    // Allowed to try to find a better correction level
    if (qrcode->optimizeEcc)
    {
        // Ranking of ECC levels to try
        static const qrcode_error_correction_level_t ranking[1 << QRCODE_SIZE_ECL] = {
            QRCODE_ECL_L, QRCODE_ECL_M, QRCODE_ECL_Q, QRCODE_ECL_H 
        };
        for (int i = 1; i < sizeof(ranking) / sizeof(ranking[0]); i++)
        {
            // Are we on the lower level?
            if (qrcode->errorCorrectionLevel == ranking[i - 1])
            {
                // Try the better ECC
                size_t dataCapacity = QrCodeDataCapacity(qrcode->version, ranking[i]);
                spareCapacity = (int)dataCapacity - (int)qrcode->sizeBits;
                // Does this better ECC level fit?
                if (spareCapacity >= 0)
                {
                    qrcode->dataCapacity = dataCapacity;
                    qrcode->errorCorrectionLevel = ranking[i];
                }
            }
        }
    }

    // Required size of scratch buffer
    qrcode->scratchBufferSize = QRCODE_SCRATCH_BUFFER_SIZE(qrcode->version);
#ifdef QR_DEBUG_DUMP
    qrcode->scratchBufferSize *= 8;
#endif
    qrcode->prepared = true;
    return true;
}

// Get the dimension of the code (0=error), minimum buffer size for output, and scratch buffer size (will be less than the output buffer size)
int QrCodeSize(qrcode_t* qrcode, size_t* bufferSize, size_t* scratchBufferSize)
{
    QrCodePrepare(qrcode);
    if (bufferSize != NULL) *bufferSize = qrcode->bufferSize;
    if (scratchBufferSize != NULL) *scratchBufferSize = qrcode->scratchBufferSize;
    return qrcode->dimension;
}

// --- Reed-Solomon Error-Correction Code ---
// Product modulo GF(2^8/0x011D)
// These error-correction functions are from https://www.nayuki.io/page/qr-code-generator-library Copyright (c) Project Nayuki. (MIT License)
static uint8_t QrCodeRSMultiply(uint8_t a, uint8_t b)
{
    uint8_t value = 0;
    for (int i = 7; i >= 0; i--)
    {
        value = (uint8_t)((value << 1) ^ ((value >> 7) * 0x011D));
        value ^= ((b >> i) & 1) * a;
    }
    return value;
}

// Reed-Solomon ECC generator polynomial for given degree.
// These error-correction functions are from https://www.nayuki.io/page/qr-code-generator-library Copyright (c) Project Nayuki. (MIT License)
void QrCodeRSDivisor(int degree, uint8_t result[])
{
    memset(result, 0, (size_t)degree * sizeof(result[0]));
    result[degree - 1] = 1;
    uint8_t root = 1;
    for (int i = 0; i < degree; i++)
    {
        for (int j = 0; j < degree; j++)
        {
            result[j] = QrCodeRSMultiply(result[j], root);
            if (j + 1 < degree)
            {
                result[j] ^= result[j + 1];
            }
        }
        root = QrCodeRSMultiply(root, 0x02);
    }
}

// Reed-Solomon ECC.
// These error-correction functions are from https://www.nayuki.io/page/qr-code-generator-library Copyright (c) Project Nayuki. (MIT License)
static void QrCodeRSRemainder(const uint8_t data[], size_t dataLen, const uint8_t generator[], int degree, uint8_t result[])
{
    memset(result, 0, (size_t)degree * sizeof(result[0]));
    for (size_t i = 0; i < dataLen; i++)
    {
        uint8_t factor = data[i] ^ result[0];
        memmove(&result[0], &result[1], ((size_t)degree - 1) * sizeof(result[0]));
        result[degree - 1] = 0;
        for (int j = 0; j < degree; j++)
        {
            result[j] ^= QrCodeRSMultiply(generator[j], factor);
        }
    }
}

size_t QrCodeCursorWrite(qrcode_t *qrcode, int *cursorX, int *cursorY, uint8_t *buffer, size_t sourceBit, size_t countBits)
{
    size_t index = sourceBit;
    for (int countWritten = 0; countWritten < countBits; countWritten++)
    {
        int bit = QrCodeBufferRead(buffer, index);
#ifdef QR_DEBUG_DUMP
        bit = (((qrWritingCodeword & 1 ? 20 : 0) + ((int)countBits - 1 - countWritten)) << 1) | bit; // for debug
#endif
        QrCodeModuleSet(qrcode, *cursorX, *cursorY, bit);
        index++;
        if (!QrCodeCursorAdvance(qrcode, cursorX, cursorY)) break;
    }
    return index - sourceBit;
}

int QrCodeEvaluatePenalty(qrcode_t *qrcode)
{
    // Note: Penalty calculated over entire code (although format information is not yet written)
    const int scoreN1 = 3;
    //const int scoreN2 = 3;
    const int scoreN3 = 40;
    const int scoreN4 = 10;
    int totalPenalty = 0;

    // Feature 1: Adjacent identical modules in row/column: (5 + i) count, penalty points: N1 + i
    // Feature 3: 1:1:3:1:1 ratio patterns (either polarity) in row/column, penalty points: N3
    for (int swapAxis = 0; swapAxis <= 1; swapAxis++)
    {
        int runs[5];
        int runsCount = 0;
        for (int y = 0; y < qrcode->dimension; y++)
        {
            int lastBit = -1;
            int runLength = 0;
            for (int x = 0; x < qrcode->dimension; x++)
            {
                int bit = QrCodeModuleGet(qrcode, swapAxis ? y : x, swapAxis ? x : y);
                // Run extended
                if (bit == lastBit) runLength++;
                // End of run
                if (bit != lastBit || x >= qrcode->dimension - 1)
                {
                    // If not start condition
                    if (lastBit >= 0)
                    {
                        // Feature 1
                        if (runLength >= 5) // or should this be strictly greater-than?
                        {
                            totalPenalty += scoreN1 + (runLength - 5);
                        }

                        // Feature 3
                        runsCount++;
                        runs[runsCount % 5] = runLength;
                        // Once we have a history of 5 lengths, check proportion
                        if (runsCount >= 5)
                        {
                            // Proportion:             1 : 1 : 3 : 1 : 1
                            // Modulo relative index: +3, +4,  0, +1, +2
                            // Check for proportions
                            int v = runs[(runsCount + 1) % 5];
                            if (runs[runsCount % 5] == 3 * v && v == runs[(runsCount + 2) % 5] && v == runs[(runsCount + 3) % 5] && v == runs[(runsCount + 4) % 5])
                            {
                                totalPenalty += scoreN3;
                            }
                        }
                    }
                    runLength = 1;
                    lastBit = bit;
                }
            }
        }
    }

    // Feature 2: Block of identical modules: m * n size, penalty points: N2 * (m-1) * (n-1)
 // TODO: Calculate feature 2 penalty. (Clear up ambiguity over "block" and counting the same "block" overlapped multiple times)
    ; // scoreN2

    // Feature 4: Dark module percentage: 50 +|- (5*k) to 50 +|- (5*(k+1)), penalty points: N4 * k
    {
        int32_t darkCount = 0;
        for (int y = 0; y < qrcode->dimension; y++)
        {
            for (int x = 0; x < qrcode->dimension; x++)
            {
                int bit = QrCodeModuleGet(qrcode, x, y);
                if (bit == QRCODE_MODULE_DARK) darkCount++;
            }
        }
        // Deviation from 50%
        int percentage = (int)((100 * darkCount + (qrcode->dimension * qrcode->dimension / 2)) / (qrcode->dimension * qrcode->dimension));
        int deviation = abs(percentage - 50);
        int rating = deviation / 5;
        int penalty = scoreN4 * rating;
        totalPenalty += penalty;
    }

    return totalPenalty;
}


#ifdef QR_DEBUG_DUMP
void QrCodeDebugDump(qrcode_t* qrcode)
{
    int quiet = QRCODE_QUIET_STANDARD;
    for (int y = -quiet; y < qrcode->dimension + quiet; y++)
    {
        int lastColor = -1;
        for (int x = -quiet; x < qrcode->dimension + quiet; x++)
        {
            int index;
            qrcode_part_t part = QrCodeIdentifyModule(qrcode, x, y, &index);
            static char buf[3] = "??";
            int color = QrCodeModuleGet(qrcode, x, y);
            if ((index & 0x1f) < 10) buf[1] = (index & 15) + '0'; else buf[1] = (index & 15) - 10 + 'a';

            switch (part)
            {
            case QRCODE_PART_QUIET:            buf[0] = '.'; break;
            case QRCODE_PART_CONTENT:          buf[0] = '_'; break;
            case QRCODE_PART_SEPARATOR:        buf[0] = ':'; break;
            case QRCODE_PART_FINDER:           buf[0] = 'F'; break;
            case QRCODE_PART_FORMAT:           buf[0] = 'f'; break;
            case QRCODE_PART_VERSION:          buf[0] = 'V'; break;
            case QRCODE_PART_ALIGNMENT:        buf[0] = 'A'; break;
            case QRCODE_PART_TIMING:           buf[0] = 'T'; break;
            }

            // Debug double digits
            if (part == QRCODE_PART_CONTENT || part == QRCODE_PART_FORMAT || part == QRCODE_PART_VERSION)
            {
#if 1       // Index from debug buffer (otherwise from identify call)
                if (((color >> 1) % 20) > 15) { buf[0] = '_'; buf[1] = '_'; }
                else
                {
                    buf[0] = '0' + (((color >> 1) / 10) % 2);
                    buf[1] = '0' + ((color >> 1) % 10);
                    if (buf[0] == '0') buf[0] = '_';
                }
#endif
                color = 3 + ((color >> 1) / 20);    // 0-19; 20-39; 40-59; ...
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
            printf("%s", buf);
            lastColor = color;
        }
        printf("\033[0m \n");    // reset
    }
}
#endif

/*
void dump_scratch(uint8_t* scratchBuffer, size_t numBits, char* label)
{
    printf("@%d %10s ", (int)numBits, label ? label : "");
    for (size_t i = 0; i < numBits; i++)
    {
        if (i % 8 == 0)
        {
            uint8_t byte = scratchBuffer[i >> 3];
            printf(" %02x=", byte);
        }
        bool bit = QrCodeBufferRead(scratchBuffer, i);
        printf("%d", (int)bit);
    }
    printf("\n");
}
*/


// Generate the code
bool QrCodeGenerate(qrcode_t* qrcode, uint8_t* buffer, uint8_t* scratchBuffer)
{
    if (!QrCodePrepare(qrcode)) return false;

    // --- Generate final codewords ---
    qrcode->scratchBuffer = scratchBuffer;
    memset(qrcode->scratchBuffer, 0, qrcode->scratchBufferSize);

    // Write data segments
    size_t bitPosition = 0;
    for (qrcode_segment_t* seg = qrcode->firstSegment; seg != NULL; seg = seg->next)
    {
        bitPosition += QrCodeSegmentWrite(seg, qrcode->version, qrcode->scratchBuffer, bitPosition);
    }

    // Add terminator 4-bit (0b0000)
    size_t remaining = qrcode->dataCapacity - bitPosition;
    if (remaining > 4) remaining = 4;
    bitPosition += QrCodeBufferAppend(qrcode->scratchBuffer, bitPosition, QRCODE_MODE_INDICATOR_TERMINATOR, remaining);

    // Round up to a whole byte
    size_t bits = (8 - (bitPosition & 7)) & 7;
    remaining = qrcode->dataCapacity - bitPosition;
    if (remaining > bits) remaining = bits;
    bitPosition += QrCodeBufferAppend(qrcode->scratchBuffer, bitPosition, 0, remaining);

    // Fill any remaining data space with padding
    while ((remaining = qrcode->dataCapacity - bitPosition) > 0)
    {
        if (remaining > 16) remaining = 16;
        bitPosition += QrCodeBufferAppend(qrcode->scratchBuffer, bitPosition, QRCODE_PAD_CODEWORDS >> (16 - remaining), remaining);
    }

    // --- Calculate ECC at end of codewords ---
    // ECC settings for the level and verions
    int eccCodewords = qrcode_ecc_block_codewords[qrcode->errorCorrectionLevel][qrcode->version];
    int eccBlockCount = qrcode_ecc_block_count[qrcode->errorCorrectionLevel][qrcode->version];
    size_t totalCapacity = QRCODE_TOTAL_CAPACITY(qrcode->version);

    // Position in buffer for ECC data
    size_t eccOffset = (totalCapacity - ((size_t)8 * eccCodewords * eccBlockCount)) / 8;
    //if ((bitPosition != 8 * eccOffset) || (bitPosition != qrcode->dataCapacity) || (qrcode->dataCapacity != 8 * eccOffset)) printf("ERROR: Expected current bit position (%d) to match ECC offset *8 (%d) and data capacity (%d).\n", (int)bitPosition, (int)eccOffset * 8, (int)qrcode->dataCapacity);

        // Calculate Reed-Solomon divisor
    uint8_t eccDivisor[QRCODE_ECC_CODEWORDS_MAX];
    QrCodeRSDivisor(eccCodewords, eccDivisor);

    // Calculate ECC for each block -- write all consecutively after the data (will be interleaved later)
    size_t dataCapacityBytes = qrcode->dataCapacity / 8;
    size_t dataLenShort = dataCapacityBytes / eccBlockCount;
    int countShortBlocks = (int)(eccBlockCount - (dataCapacityBytes - (dataLenShort * eccBlockCount)));
    size_t dataLenLong = dataLenShort + (countShortBlocks >= eccBlockCount ? 0 : 1);
    for (int block = 0; block < eccBlockCount; block++)
    {
        // Calculate offset and length (earlier consecutive blocks may be short by 1 codeword)
        size_t dataOffset;
        if (block < countShortBlocks)
        {
            dataOffset = block * dataLenShort;
        }
        else
        {
            dataOffset = block * dataLenShort + ((size_t)block - countShortBlocks);
        }
        size_t dataLen = dataLenShort + (block < countShortBlocks ? 0 : 1);
        // Calculate this block's ECC
        uint8_t* eccDest = qrcode->scratchBuffer + eccOffset + (block * (size_t)eccCodewords);
        QrCodeRSRemainder(qrcode->scratchBuffer + dataOffset, dataLen, eccDivisor, eccCodewords, eccDest);
    }

    // --- Generate pattern ---
    qrcode->buffer = buffer;
    memset(qrcode->buffer, 0, qrcode->bufferSize);
    QrCodeDrawFinder(qrcode, QRCODE_FINDER_SIZE / 2, QRCODE_FINDER_SIZE / 2);
    QrCodeDrawFinder(qrcode, qrcode->dimension - 1 - QRCODE_FINDER_SIZE / 2, QRCODE_FINDER_SIZE / 2);
    QrCodeDrawFinder(qrcode, QRCODE_FINDER_SIZE / 2, qrcode->dimension - 1 - QRCODE_FINDER_SIZE / 2);
    QrCodeDrawTiming(qrcode);
    for (int hi = 0, h; (h = QrCodeAlignmentCoordinates(qrcode->version, hi)) > 0; hi++)
    {
        for (int vi = 0, v; (v = QrCodeAlignmentCoordinates(qrcode->version, vi)) > 0; vi++)
        {
            if (h <= QRCODE_FINDER_SIZE && v <= QRCODE_FINDER_SIZE) continue;                           // Obscured by top-left finder
            if (h >= qrcode->dimension - 1 - QRCODE_FINDER_SIZE && v <= QRCODE_FINDER_SIZE) continue;   // Obscured by top-right finder
            if (h <= QRCODE_FINDER_SIZE && v >= qrcode->dimension - 1 - QRCODE_FINDER_SIZE) continue;   // Obscured by bottom-left finder
            QrCodeDrawAlignment(qrcode, h, v);
        }
    }


    // Write the codewords interleaved between blocks
    int cursorX, cursorY;
    QrCodeCursorReset(qrcode, &cursorX, &cursorY);
    size_t totalWritten = 0;

    // Write data codewords interleaved accross ecc blocks -- some early blocks may be short
    for (int i = 0; i < dataLenLong; i++)
    {
        for (int block = 0; block < eccBlockCount; block++)
        {
            // Calculate offset and length (earlier consecutive blocks may be short by 1 codeword)
            // Skip codewords due to short block
            if (i >= dataLenShort && block < countShortBlocks) continue;
            size_t codeword = (block * dataLenShort) + (block > countShortBlocks ? block - countShortBlocks : 0) + i;
            size_t sourceBit = codeword * 8;
            size_t countBits = 8;
#ifdef QR_DEBUG_DUMP
            qrWritingCodeword = ((i * eccBlockCount) + block) & 1;
#endif
            totalWritten += QrCodeCursorWrite(qrcode, &cursorX, &cursorY, qrcode->scratchBuffer, sourceBit, countBits);
        }
    }

    // Write ECC codewords interleaved accross ecc blocks
    for (int i = 0; i < eccCodewords; i++)
    {
        for (int block = 0; block < eccBlockCount; block++)
        {
            size_t sourceBit = 8 * eccOffset + (block * (size_t)eccCodewords * 8) + ((size_t)i * 8);
            size_t countBits = 8;
#ifdef QR_DEBUG_DUMP
            qrWritingCodeword = ((i * eccBlockCount) + block + (dataLenLong * eccBlockCount)) & 1;
#endif
            totalWritten += QrCodeCursorWrite(qrcode, &cursorX, &cursorY, qrcode->scratchBuffer, sourceBit, countBits);
        }
    }
    //printf("*** dataCapacity=%d capacity=%d totalWritten=%d remainder=%d, eccOffset(bytes)=%d, eccCodewords=%d, eccBlockCount=%d, eccSize=%d, eccEnd=%d\n", (int)qrcode->dataCapacity, (int)totalCapacity, (int)totalWritten, (int)(totalCapacity - totalWritten), (int)eccOffset, (int)eccCodewords, (int)eccBlockCount, (int)(8 * eccCodewords * eccBlockCount), (int)((eccOffset * 8) + 8 * eccCodewords * eccBlockCount));

        // Add any remainder 0 bits (could be 0/3/4/7)
    while (totalWritten < totalCapacity)
    {
        QrCodeModuleSet(qrcode, cursorX, cursorY, 0);
        totalWritten++;
        if (!QrCodeCursorAdvance(qrcode, &cursorX, &cursorY)) break;
    }

    // --- Mask pattern ---
    if (qrcode->maskPattern == QRCODE_MASK_AUTO)
    {
        int lowestPenalty = -1;
        for (int maskPattern = QRCODE_MASK_000; maskPattern <= QRCODE_MASK_111; maskPattern++)
        {
            // XOR mask pattern
            QrCodeApplyMask(qrcode, maskPattern);

            // Find penalty score for this mask pattern
            int penalty = QrCodeEvaluatePenalty(qrcode);

            // XOR same mask removes it
            QrCodeApplyMask(qrcode, maskPattern);

            // See if this is the best so far
            if (lowestPenalty < 0 || penalty < lowestPenalty)
            {
                lowestPenalty = penalty;
                qrcode->maskPattern = maskPattern;
            }
        }
    }

    // Use selected mask
    QrCodeApplyMask(qrcode, qrcode->maskPattern);

    // Version info (V7+) (additional 36 modules, total 67 for format+version)
    if (qrcode->version >= 7)
    {
        uint32_t versionInfo = QrCodeCalcVersionInfo(qrcode, qrcode->version);
        QrCodeDrawVersionInfo(qrcode, versionInfo);
    }

    // Write format information
    uint16_t formatInfo = QrCodeCalcFormatInfo(qrcode, qrcode->errorCorrectionLevel, qrcode->maskPattern);
    QrCodeDrawFormatInfo(qrcode, formatInfo);

#ifdef QR_DEBUG_DUMP
    QrCodeDebugDump(qrcode);
#endif

    return true;
}
