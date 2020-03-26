// QR Code
// Dan Jackson, 2020

#ifndef QRCODE_H
#define QRCODE_H

#include <stdbool.h>
#include <stdint.h>

#define QRCODE_QUIET_NONE 0
#define QRCODE_QUIET_STANDARD 4

#define QRCODE_VERSION_TO_SIZE(_n) (17 + 4 * (_n)) // (21 + 4 * ((_n) - 1))   // V1=21x21; V40=177x177

typedef struct
{
    uint8_t *buffer;
    size_t bufferSize;

    int size;      // NxN dimensions (excluding any quiet margin)

    int quiet;

    bool error;
} qrcode_t;

// Initialize a QR Code object
void QrCodeInit(qrcode_t *qrcode);





// Assign a buffer and its size (in bytes)
void QrCodeSetBuffer(qrcode_t *qrcode, unsigned int size, uint8_t *buffer, size_t bufferSize);

// Generate the code
bool QrCodeGenerate(qrcode_t *qrcode, const char *text);

#endif
