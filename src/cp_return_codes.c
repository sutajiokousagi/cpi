/*
 * cp_return_codes.c
 *
 * Aaron "Caustik" Robinson
 * (c) Copyright Chumby Industries, 2007
 * All rights reserved
 *
 * This module implements the CPI return code lookup table, which is provided
 * for convienence during debugging.
 */

#include "cp_interface.h"

char *CPI_RETURN_CODE_LOOKUP[0x07] =
{
    "CPI_OK",
    "CPI_FAIL",
    "CPI_NOTIMPL",
    "CPI_INVALID_PARAM",
    "CPI_OUT_OF_MEMORY",
    "CPI_ACCESS_DENIED",
    "CPI_INVALID_CALL"
};
