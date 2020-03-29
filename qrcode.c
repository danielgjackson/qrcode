// QR Code Generator
// Dan Jackson, 2020

#ifdef _WIN32
#ifdef _DEBUG
// TODO: REMOVE THIS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// For output functions (TODO: Move these out?)
#include <stdio.h>

#include "qrcode.h"

//#define QRCODE_DEBUG_DUMP

//#define QRCODE_DIMENSION_TO_VERSION(_n) (((_n) - 17) / 4)
#define QRCODE_FINDER_SIZE 7
#define QRCODE_TIMING_OFFSET 6
#define QRCODE_VERSION_SIZE 3
#define QRCODE_ALIGNMENT_RADIUS 2
#define QRCODE_MODULE_LIGHT 0
#define QRCODE_MODULE_DARK 1

#define QRCODE_PAD_CODEWORDS 0xec11 // Pad codewords 0b11101100=0xec 0b00010001=0x11


// ECI Assignment Numbers
#define QRCODE_ECI_UTF8 26 // "\000026" UTF8 - ISO/IEC 10646 UTF-8 encoding


static bool QrCodeBufferRead(uint8_t* scratchBuffer, size_t bitPosition)
{
    return (scratchBuffer[bitPosition >> 3] & (1 << (7 - (bitPosition & 7)))) ? 1 : 0;
}

#if 0
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
#endif


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
    //case QRCODE_MODE_INDICATOR_KANJI:
    //    return (version < 10) ? 8 : (version < 27) ? 10 : 12; // Kanji
    }
    return 0;
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
        }
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

// TODO: Who writes the 4-bit "0" terminator, any 0-bit padding, and version/ecc-specific pad codewords QRCODE_PAD_CODEWORDS (i.e. everything pre checksum)

    return bitsWritten;
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
void QrCodeInit(qrcode_t *qrcode, int maxVersion, qrcode_error_correction_level_t errorCorrectionLevel, int quiet)
{
    memset(qrcode, 0, sizeof(qrcode_t));
    qrcode->version = QRCODE_VERSION_AUTO;
    qrcode->maxVersion = maxVersion;
    qrcode->errorCorrectionLevel = errorCorrectionLevel; // QRCODE_ECL_M;
    qrcode->maskPattern = QRCODE_MASK_AUTO;
    qrcode->quiet = quiet; // QRCODE_QUIET_STANDARD;
    qrcode->optimizeEcc = true;
    qrcode->prepared = false;
    qrcode->buffer = NULL;
    qrcode->bufferSize = 0;
}


int QrCodeModuleGet(qrcode_t *qrcode, int x, int y)
{
    if (x < 0 || y < 0 || x >= qrcode->dimension || y >= qrcode->dimension) return 0; // quiet
    // TODO: Bitfield instead of byte array
    int offset = y * qrcode->dimension + x;
    if (qrcode->buffer == NULL || offset < 0 || (offset >> 3) >= qrcode->bufferSize) return -1;
    return qrcode->buffer[offset >> 3] & (1 << (7 - (offset & 7))) ? 1 : 0;
}

static void QrCodeModuleSet(qrcode_t *qrcode, int x, int y, int value)
{
    if (x < 0 || y < 0 || x >= qrcode->dimension || y >= qrcode->dimension) return; // quiet
    // TODO: Bitfield instead of byte array
    int offset = y * qrcode->dimension + x;
    if (qrcode->buffer == NULL || offset < 0 || (offset >> 3) >= qrcode->bufferSize) return;
    uint8_t mask = (1 << (7 - (offset & 7)));
    qrcode->buffer[offset >> 3] = (qrcode->buffer[offset >> 3] & ~mask) | (value ? mask : 0);
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
            int color = QrCodeModuleGet(qrcode, x, y);;

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


// Draw finder and quiet padding
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
        //v = 100 + i; // for debug
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
    QrCodeModuleSet(qrcode, QRCODE_FINDER_SIZE + 1, qrcode->dimension - 1 - QRCODE_FINDER_SIZE, QRCODE_MODULE_DARK);
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
        QrCodeModuleSet(qrcode, col, qrcode->dimension - 1 - QRCODE_FINDER_SIZE - QRCODE_VERSION_SIZE + row, v);
        QrCodeModuleSet(qrcode, qrcode->dimension - 1 - QRCODE_FINDER_SIZE - QRCODE_VERSION_SIZE + row, col, v);
    }
}


// Calculate 15-bit format information (2-bit error-correction level, 3-bit mask, 10-bit BCH error-correction; all masked)
static uint16_t QrCodeCalcFormatInfo(qrcode_t *qrcode, qrcode_error_correction_level_t errorCorrectionLevel, qrcode_mask_pattern_t maskPattern)
{
    // TODO: Reframe in terms of QRCODE_SIZE_ECL (2) and QRCODE_SIZE_MASK (3)

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
        if (QrCodeIdentifyModule(qrcode, *x, *y) == QRCODE_PART_CONTENT) return true;
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
        if (spareCapacity <= 0) return false;           // None fit
    }
    else
    {
        // Check the requested version
        qrcode->sizeBits = QrCodeBitsUsed(qrcode);
        qrcode->dataCapacity = QrCodeDataCapacity(qrcode->version, qrcode->errorCorrectionLevel);
        spareCapacity = (int)qrcode->dataCapacity - (int)qrcode->sizeBits;
        if (spareCapacity <= 0) return false;           // Does not fit
    }

    // Cache dimension for the chosen version
    qrcode->dimension = QRCODE_VERSION_TO_DIMENSION(qrcode->version);

    // Required size of QR Code buffer
    qrcode->bufferSize = QRCODE_BUFFER_SIZE(qrcode->version);

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

    qrcode->prepared = true;
    return true;
}


// Get the minimum buffer size for output, and scratch buffer size (will be less than the output buffer size)
size_t QrCodeBufferSize(qrcode_t *qrcode, size_t *scratchBufferSize)
{
    QrCodePrepare(qrcode);
    if (scratchBufferSize != NULL)
    {
        *scratchBufferSize = qrcode->scratchBufferSize;
    }
    size_t bufferSize = qrcode->bufferSize;
    return bufferSize;
}



// Reed-Solomon Error-Correction Code

// Product modulo GF(2^8/0x011D)
// This function from "QR Code generator library" https://www.nayuki.io/page/qr-code-generator-library Copyright (c) Project Nayuki. (MIT License)
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
// This function from "QR Code generator library" https://www.nayuki.io/page/qr-code-generator-library Copyright (c) Project Nayuki. (MIT License)
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
// This function from "QR Code generator library" https://www.nayuki.io/page/qr-code-generator-library Copyright (c) Project Nayuki. (MIT License)
static void QrCodeRSRemainder(const uint8_t data[], size_t dataLen, const uint8_t generator[], int degree, uint8_t result[])
{
    memset(result, 0, (size_t)degree * sizeof(result[0]));
    for (size_t i = 0; i < dataLen; i++)
    {
        uint8_t factor = data[i] ^ result[0];
        memmove(&result[0], &result[1], (size_t)(degree - 1) * sizeof(result[0]));
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
        QrCodeModuleSet(qrcode, *cursorX, *cursorY, bit);
        index++;
        if (!QrCodeCursorAdvance(qrcode, cursorX, cursorY)) break;
    }
    return index - sourceBit;
}


// Generate the code
bool QrCodeGenerate(qrcode_t *qrcode, uint8_t *buffer, uint8_t *scratchBuffer)
{
    qrcode->buffer = buffer;
    qrcode->scratchBuffer = scratchBuffer;

    // Clear
    memset(qrcode->buffer, 0, qrcode->bufferSize);
    memset(qrcode->scratchBuffer, 0, qrcode->scratchBufferSize);



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

    // Code generation
    size_t bitPosition = 0;
//dump_scratch(qrcode->scratchBuffer, bitPosition, "start");
    for (qrcode_segment_t* seg = qrcode->firstSegment; seg != NULL; seg = seg->next)
    {
        bitPosition += QrCodeSegmentWrite(seg, qrcode->version, qrcode->scratchBuffer, bitPosition);
    }
//dump_scratch(qrcode->scratchBuffer, bitPosition, "segment");

    // Add terminator 4-bit (0b0000)
    size_t remaining = qrcode->dataCapacity - bitPosition;
    if (remaining > 4) remaining = 4;
    bitPosition += QrCodeBufferAppend(qrcode->scratchBuffer, bitPosition, QRCODE_MODE_INDICATOR_TERMINATOR, remaining);
//dump_scratch(qrcode->scratchBuffer, bitPosition, "terminator");

    // Round up to a whole byte
    size_t bits = (8 - (bitPosition & 7)) & 7;
    remaining = qrcode->dataCapacity - bitPosition;
    if (remaining > bits) remaining = bits;
    bitPosition += QrCodeBufferAppend(qrcode->scratchBuffer, bitPosition, 0, remaining);
//dump_scratch(qrcode->scratchBuffer, bitPosition, "round");

    // Fill any remaining data space with padding
    while ((remaining = qrcode->dataCapacity - bitPosition) > 0)
    {
        if (remaining > 16) remaining = 16;
        bitPosition += QrCodeBufferAppend(qrcode->scratchBuffer, bitPosition, QRCODE_PAD_CODEWORDS >> (16 - remaining), remaining);
//dump_scratch(qrcode->scratchBuffer, bitPosition, "padding");
    }

    // ECC settings for the level and verions
    int eccCodewords = qrcode_ecc_block_codewords[qrcode->errorCorrectionLevel][qrcode->version];
    int eccBlockCount = qrcode_ecc_block_count[qrcode->errorCorrectionLevel][qrcode->version];
    size_t totalCapacity = QRCODE_TOTAL_CAPACITY(qrcode->version);

    // Calculate Reed-Solomon divisor
    uint8_t eccDivisor[QRCODE_ECC_CODEWORDS_MAX];
    QrCodeRSDivisor(eccCodewords, eccDivisor);

    // Position in buffer for ECC data
    size_t eccOffset = (totalCapacity - (8 * eccCodewords * eccBlockCount)) / 8;
#if 1
if ((bitPosition != 8 * eccOffset) || (bitPosition != qrcode->dataCapacity) || (qrcode->dataCapacity != 8 * eccOffset)) printf("ERROR: Expected current bit position (%d) to match ECC offset *8 (%d) and data capacity (%d).\n", (int)bitPosition, (int)eccOffset * 8, (int)qrcode->dataCapacity);
#endif

    // Calculate ECC for each block -- write all consecutively after the data (will be interleaved later)
    for (int block = 0; block < eccBlockCount; block++)
    {
// TODO: Calculate offset and length correctly (earlier consecutive blocks may be short by 1 codeword)
size_t dataLen = qrcode->dataCapacity / 8 / eccBlockCount;
size_t dataOffset = 0;
        // Calculate this block's ECC
        uint8_t *eccDest = qrcode->scratchBuffer + eccOffset + (block * (size_t)eccCodewords);
        QrCodeRSRemainder(qrcode->scratchBuffer + dataOffset, dataLen, eccDivisor, eccCodewords, eccDest);
    }

    // Generate the interleaved QR Code data bits
// TODO: Interleave reads for each codeword accross data blocks
    int cursorX, cursorY;
    QrCodeCursorReset(qrcode, &cursorX, &cursorY);
    size_t totalWritten = 0;
    size_t sourceBit;
    size_t countBits;

// TODO: For each block, interleave
    sourceBit = 0;
    countBits = totalCapacity;
    totalWritten += QrCodeCursorWrite(qrcode, &cursorX, &cursorY, qrcode->scratchBuffer, sourceBit, countBits);

// TODO: Interleave reads for each codeword accross ecc blocks
    int block = 0;
    sourceBit = 8 * eccOffset + (block * (size_t)eccCodewords * 8);
    countBits = eccCodewords * 8;
    totalWritten += QrCodeCursorWrite(qrcode, &cursorX, &cursorY, qrcode->scratchBuffer, sourceBit, countBits);

printf("*** dataCapacity=%d capacity=%d measuredCapacity=%d\n", (int)qrcode->dataCapacity, (int)totalCapacity, (int)totalWritten);

    // TODO: Better masking
    qrcode->maskPattern = QRCODE_MASK_000;
    QrCodeApplyMask(qrcode, qrcode->maskPattern);

    // TODO: Evaluate masking attempt

    // Write format information
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
            int bit = QrCodeModuleGet(qrcode, x, y) ^ (invert ? 0x1 : 0x0);
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
            int bitU = QrCodeModuleGet(qrcode, x, y);
            int bitL = (y + 1 < qrcode->dimension + qrcode->quiet) ? QrCodeModuleGet(qrcode, x, y + 1) : (invert ? 0 : 1);
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


