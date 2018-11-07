#include <stdint.h>
#include <stdio.h>

#define TF_USE_SOI_BYTE  1
#define TF_SOI_BYTE     0x7E

#define TF_USE_EOI_BYTE  1
#define TF_EOI_BYTE     0x0D

#define TF_PVERSION_BYTE  1
// Trina solar protocol 1 mixed energy = 0x21
#define PVERSION_1_BYTE     0x21
// Trina solar protocol 2 battery management = 0x10
#define PVERSION_2_BYTE     0x10

// Trina solar data info byte
#define TF_DATA_BYTES     4

// Maximum received payload size (static buffer)
#define TF_MAX_PAYLOAD_RX 1024
#define TF_SENDBUF_LEN 1024




