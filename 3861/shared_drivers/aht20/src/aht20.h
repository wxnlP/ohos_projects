#ifndef AHT20_H
#define AHT20_H

#include "stdint.h"

/* AHT20的状态位，为标出的为保留位 */
typedef enum {
    STATUS_BITS_CAL_ENABLE=3,
    STATUS_BITS_BUSY_INDICATION=7
} StatusBits;

typedef enum {
    CAL_DISABLE,
    CAL_ENABLE=1,
} CalEnableBit;

typedef enum {
    BUSY_NO,
    BUSY_YES=1,
} BusyIndication;

uint32_t AHT20Init(void);
uint32_t AHT20MeasureResult(float* humidity, float* temperature);

#endif