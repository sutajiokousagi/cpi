/*
 * cp_utility.h
 *
 * Aaron "Caustik" Robinson
 * (c) Copyright Chumby Industries, 2007
 * All rights reserved
 *
 * This API defines utility functions for cp_interface.
 */

#ifndef CP_UTILITY_H
#define CP_UTILITY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cp_interface.h"

/*! wake up the CP if it is sleeping */
int cpi_util_wakeup_cp(cpi_t *p_cpi);

/*! write the specified string to the serial device file */
int cpi_util_write_str(cpi_t *p_cpi, char *str);

/*! write the specified binary data to the serial device file */
int cpi_util_write_bin(cpi_t *p_cpi, void *data, int size);

/*! write the LF character (0x0A) to the serial device file */
int cpi_util_write_lf(cpi_t *p_cpi);

/*! write the EOF character (0x0D) to the serial device file */
int cpi_util_write_eof(cpi_t *p_cpi);

/*! read string with max of specified size from the serial device file (returns # bytes written) */
int cpi_util_read_str(cpi_t *p_cpi, char *str, int *p_size);

/*! write the EOF character (0x0D) from the serial device file */
int cpi_util_read_eof(cpi_t *p_cpi);

/*! base64 decode the specified string into the specified buffer (returns # bytes written) */
int cpi_util_decode_str(cpi_t *p_cpi, char *str, int size, void *data, int *p_size);

#ifdef __cplusplus
}
#endif

#endif
