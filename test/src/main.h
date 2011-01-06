/*
 * main.h
 *
 * Aaron "Caustik" Robinson
 * (c) Copyright Chumby Industries, 2007
 * All rights reserved
 *
 */

#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>

/*! structure used by key_file */
typedef struct _key_entry
{
    uint8_t i[16];      // PID
    uint8_t p[64];
    uint8_t q[64];
    uint8_t dp[64];
    uint8_t dq[64];
    uint8_t qi[64];
    uint8_t n[128];
    uint8_t e[4];
    uint8_t created[4];
    uint8_t padding[40];
}
key_entry;

#endif

