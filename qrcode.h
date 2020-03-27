// QR Code Generator
// Dan Jackson, 2020

#ifndef QRCODE_H
#define QRCODE_H

#include <stdbool.h>
#include <stdint.h>

#define QRCODE_QUIET_NONE 0
#define QRCODE_QUIET_STANDARD 4

#define QRCODE_VERSION_AUTO 0
#define QRCODE_VERSION_MIN 1
#define QRCODE_VERSION_MAX 40
#define QRCODE_VERSION_TO_DIMENSION(_n) (17 + 4 * (_n)) // (21 + 4 * ((_n) - 1))   // V1=21x21; V40=177x177

// Error correction level
typedef enum {
    QRCODE_ECL_M = 0x00, // 0b00 Medium (~15%)
    QRCODE_ECL_L = 0x01, // 0b01 Low (~7%)
    QRCODE_ECL_H = 0x02, // 0b10 High (~30%)
    QRCODE_ECL_Q = 0x03, // 0b11 Quartile (~25%)
} qrcode_error_correction_level_t;

// Mask pattern reference (i=row, j=column; where true: invert)
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

// QR Code Object
typedef struct
{
    uint8_t *buffer;
    size_t bufferSize;

    int version;    // QRCODE_VERSION_MIN-QRCODE_VERSION_MAX; QRCODE_VERSION_AUTO: automatic
    qrcode_error_correction_level_t errorCorrectionLevel;
    qrcode_mask_pattern_t maskPattern;
    int quiet;      // Size of quiet margin for output (not stored)

    int dimension;  // Calculated from the version

    //bool error;   // flag that the object is in an erroneous state
} qrcode_t;

// Initialize a QR Code object
void QrCodeInit(qrcode_t *qrcode);

// Set the QR Code version and assign a buffer and its size (in bytes)
void QrCodeSetBuffer(qrcode_t *qrcode, int version, uint8_t *buffer, size_t bufferSize);

// Generate the code for the given text
bool QrCodeGenerate(qrcode_t *qrcode, const char *text);

#endif
