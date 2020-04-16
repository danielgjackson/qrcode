// QR Code Generator
// Dan Jackson, 2020

#ifndef QRCODE_H
#define QRCODE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QRCODE_QUIET_NONE 0
#define QRCODE_QUIET_STANDARD 4

#define QRCODE_VERSION_AUTO 0
#define QRCODE_VERSION_MIN 1
#define QRCODE_VERSION_MAX 40
#define QRCODE_VERSION_TO_DIMENSION(_n) (17 + 4 * (_n)) // (21 + 4 * ((_n) - 1))   // V1=21x21; V40=177x177

#define QRCODE_BUFFER_SIZE_BYTES(_bits) (((_bits) + 7) >> 3)

// Total data modules (raw: data, ecc and remainder) minus function pattern and format/version = data capacity in bits
#define QRCODE_TOTAL_CAPACITY(_v) (((16 * (size_t)(_v) + 128) * (size_t)(_v)) + 64 - ((size_t)(_v) < 2 ? 0 : (25 * ((size_t)(_v) / 7 + 2) - 10) * (size_t)((_v) / 7 + 2) - 55) - ((size_t)(_v) < 7 ? 0 : 36))
#define QRCODE_SCRATCH_BUFFER_SIZE(_v) QRCODE_BUFFER_SIZE_BYTES(QRCODE_TOTAL_CAPACITY(_v))

#define QRCODE_BUFFER_SIZE(_v) QRCODE_BUFFER_SIZE_BYTES(QRCODE_VERSION_TO_DIMENSION(_v) * QRCODE_VERSION_TO_DIMENSION(_v))
/*
static size_t QrCodeTotalCapacity(int version)
{
    int capacity = (16 * version + 128) * version + 64;
    if (version >= 2) capacity -= (25 * (version / 7 + 2) - 10) * (version / 7 + 2) - 55;
    if (version >= 7) capacity -= 36;
    return (size_t)capacity;
}
*/


#define QRCODE_TEXT_LENGTH ((size_t)-1)

// Error correction level
typedef enum
{
    QRCODE_ECL_M = 0x00, // 0b00 Medium (~15%)
    QRCODE_ECL_L = 0x01, // 0b01 Low (~7%)
    QRCODE_ECL_H = 0x02, // 0b10 High (~30%)
    QRCODE_ECL_Q = 0x03, // 0b11 Quartile (~25%)
} qrcode_error_correction_level_t;

// Mask pattern reference (i=row, j=column; where true: invert)
typedef enum
{
    QRCODE_MASK_AUTO = -1,  // Automatically determine best mask
    QRCODE_MASK_000 = 0x00, // 0b000 (i + j) mod 2 = 0
    QRCODE_MASK_001 = 0x01, // 0b001 i mod 2 = 0
    QRCODE_MASK_010 = 0x02, // 0b010 j mod 3 = 0
    QRCODE_MASK_011 = 0x03, // 0b011 (i + j) mod 3 = 0
    QRCODE_MASK_100 = 0x04, // 0b100 ((i div 2) + (j div 3)) mod 2 = 0
    QRCODE_MASK_101 = 0x05, // 0b101 (i j) mod 2 + (i j) mod 3 = 0
    QRCODE_MASK_110 = 0x06, // 0b110 ((i j) mod 2 + (i j) mod 3) mod 2 = 0
    QRCODE_MASK_111 = 0x07, // 0b111 ((i j) mod 3 + (i + j) mod 2) mod 2 = 0
} qrcode_mask_pattern_t;

typedef enum
{
    QRCODE_MODE_INDICATOR_AUTOMATIC = -1,           // Automatically select efficient mode
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

// ECI Assignment Numbers
#define QRCODE_ECI_UTF8 26 // "\000026" UTF8 - ISO/IEC 10646 UTF-8 encoding

// A single segment of text encoded in one mode
typedef struct qrcode_segment_tag
{
    qrcode_mode_indicator_t mode;       // Segment mode indicator
    const char *text;                   // Source text
    size_t charCount;                   // Number of characters
    struct qrcode_segment_tag *next;    // Next segment
} qrcode_segment_t;

// QR Code Object
typedef struct
{
    // Initial settings
    int maxVersion;             // Maximum allowed version
    qrcode_error_correction_level_t errorCorrectionLevel;
    bool optimizeEcc;           // Allow finding a better ECC for free within the same size

    // Data payload
    qrcode_segment_t *firstSegment;

    // Calculated after preparation (also recalcualtes version if auto and ECL if optimized)
    bool prepared;              // Preparation calculated (cached; adding another segment will invalidate the cache)
    int version;                // QRCODE_VERSION_MIN-QRCODE_VERSION_MAX; QRCODE_VERSION_AUTO: automatic
    size_t sizeBits;            // Total number of data bits from segments in the QR Code (not including bits added when space for: 4-bit terminator mode indicator, 0-padding to byte, padding bytes; or ECC)
    int dimension;              // Size of code itself (modules in width and height), does not include quiet margin (<=0 after prepared: max capacity exceeded)
    size_t dataCapacity;        // Total number of true data bits available in the codewords (after ecc and remainder)
    size_t bufferSize;          // Required size of QR Code buffer
    size_t scratchBufferSize;   // Required size of scratch buffer

    // Used during code creation
    qrcode_mask_pattern_t maskPattern;
    uint8_t *buffer;
    uint8_t *scratchBuffer;
} qrcode_t;

// Initialize a QR Code object
void QrCodeInit(qrcode_t *qrcode, int maxVersion, qrcode_error_correction_level_t errorCorrectionLevel);

// Add a text segment to the QR Code object (mode=QRCODE_MODE_INDICATOR_AUTOMATIC, charCount=QRCODE_TEXT_LENGTH if null-terminated string)
void QrCodeSegmentAppend(qrcode_t *qrcode, qrcode_segment_t *segment, qrcode_mode_indicator_t mode, const char *text, size_t charCount, bool mayUppercase);

// Get the dimension of the code (0=error), minimum buffer size for output, and scratch buffer size (will be less than the output buffer size)
int QrCodeSize(qrcode_t *qrcode, size_t *bufferSize, size_t *scratchBufferSize);

// Generate the code for the given text
bool QrCodeGenerate(qrcode_t *qrcode, uint8_t *buffer, uint8_t *scratchBuffer);

// Get the module at the given coordinate (0=light, 1=dark)
int QrCodeModuleGet(qrcode_t* qrcode, int x, int y);



typedef enum
{
    // Function patterns
    QRCODE_PART_ALIGNMENT = -4,        // Alignment pattern(s)
    QRCODE_PART_TIMING = -3,           // Timing pattern
    QRCODE_PART_SEPARATOR = -2,        // Separator around finder position detection patterns
    QRCODE_PART_FINDER = -1,           // Position detection pattern
    // Outside
    QRCODE_PART_QUIET = 0,             // Quiet margin outside of code
    // Encoding region
    QRCODE_PART_CONTENT = 1,           // Data and error-correction codewords
    QRCODE_PART_FORMAT = 2,            // Format Info
    QRCODE_PART_VERSION = 3,           // Version Info
} qrcode_part_t;

// Determines which part a given module coordinate belongs to.
qrcode_part_t QrCodeIdentifyModule(qrcode_t* qrcode, int x, int y, int *index);

#ifdef __cplusplus
}
#endif

#endif
