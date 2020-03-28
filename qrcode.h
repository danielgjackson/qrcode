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


// Error correction level
#define QRCODE_SIZE_ECL 2   // 2-bit error correction
typedef enum {
    QRCODE_ECL_M = 0x00, // 0b00 Medium (~15%)
    QRCODE_ECL_L = 0x01, // 0b01 Low (~7%)
    QRCODE_ECL_H = 0x02, // 0b10 High (~30%)
    QRCODE_ECL_Q = 0x03, // 0b11 Quartile (~25%)
} qrcode_error_correction_level_t;

// Mask pattern reference (i=row, j=column; where true: invert)
#define QRCODE_SIZE_MASK 3  // 3-bit mask size
typedef enum {
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

#define QRCODE_SIZE_MODE_INDICATOR 4                // 4-bit mode indicator
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

// A single segment of text encoded in one mode
typedef struct qrcode_segment_tag
{
    qrcode_mode_indicator_t mode;       // Segment mode indicator
    const char* text;                   // Source text
    size_t charCount;                   // Number of characters
    struct qrcode_segment_tag* next;    // Next segment
} qrcode_segment_t;


// QR Code Object
typedef struct
{
    uint8_t *buffer;
    size_t bufferSize;

    int version;    // QRCODE_VERSION_MIN-QRCODE_VERSION_MAX; QRCODE_VERSION_AUTO: automatic
    qrcode_error_correction_level_t errorCorrectionLevel;
    qrcode_mask_pattern_t maskPattern;
    int quiet;      // Size of quiet margin for output (not stored)

    int dimension;  // Cached value, calculated from the version

    qrcode_segment_t *firstSegment;

    //bool error;   // flag that the object is in an erroneous state
} qrcode_t;



// Step 1. Data analysis -- identify characters encoded, select version.
// Step 2. Data encodation -- convert to segments then a bit stream, pad for version.
// Step 3. Error correction encoding -- divide sequence into blocks, generate and append error correction codewords.
// Step 4. Strucutre final message -- interleave data and error correction and remainder bits.
// Step 5. Module palcement in matrix.
// Step 6. Masking.
// Step 7. Format and version information.



// Initialize a QR Code object
void QrCodeInit(qrcode_t *qrcode);

// Set the QR Code version and assign a buffer and its size (in bytes)
void QrCodeSetBuffer(qrcode_t *qrcode, int version, uint8_t *buffer, size_t bufferSize);

// Generate the code for the given text
bool QrCodeGenerate(qrcode_t *qrcode, const char *text);


// Output the code to the specified stream
void QrCodePrintLarge(qrcode_t* qrcode, FILE* fp, bool invert);
void QrCodePrintCompact(qrcode_t* qrcode, FILE* fp, bool invert);

#ifdef __cplusplus
}
#endif

#endif
