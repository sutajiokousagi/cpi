/*
 * cp_utility.c
 *
 * Aaron "Caustik" Robinson
 * (c) Copyright Chumby Industries, 2007
 * All rights reserved
 */

#include "cp_utility.h"

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>

#include "b64/b64.h"

/*! There is a corner case, which triggers quite frequently,
 *  if writing is done too quickly. The CP will get into a 
 *  bad state where it will no longer service requests until 
 *  a system reboot. This sleep works around the issue. */
#define ENABLE_MAGIC_SLEEP_FIX

int cpi_util_wakeup_cp(cpi_t *p_cpi)
{
    char c = '\0';

    while(c != '?')
    {
        /*! wake command */
        cpi_util_write_str(p_cpi, "!");
        // It may be possible to get caught in a state where a single ! will not respond
        // with a reset ack. Somehow enabling this extra '!' will slow down the commands
        // a little bit - so it is currently commented out.
        //cpi_util_write_str(p_cpi, "!");

        int bytes_read = read(p_cpi->serial_file, &c, sizeof(char));

        /*! handle read failure */
        if(bytes_read != sizeof(char)) { printf( "returned %d bytes\n", bytes_read ); return CPI_FAIL; }
    }

    return CPI_OK;
}

int cpi_util_write_str(cpi_t *p_cpi, char *str)
{
    int v=0;

    while(str[v] != '\0')
    {
        int bytes_written = write(p_cpi->serial_file, &str[v++], sizeof(char));
	//	printf( "%c", str[v-1] );

#ifdef ENABLE_MAGIC_SLEEP_FIX
    usleep(10*1000);
#endif

        /*! handle write failure */
        if(bytes_written != sizeof(char)) { return CPI_FAIL; }
    }

    return CPI_OK;
}

int cpi_util_write_bin(cpi_t *p_cpi, void *data, int size)
{
    size_t ret = b64_encode(data, size, p_cpi->tmp_buff, CPI_MAX_RESULT_SIZE);

    /*! failed to encode */
    if(ret == 0 || (ret > CPI_MAX_RESULT_SIZE)) { return CPI_FAIL; }

    p_cpi->tmp_buff[ret+0] = '\n';
    p_cpi->tmp_buff[ret+1] = '\0';

    /*! write base64 string */
    cpi_util_write_str(p_cpi, p_cpi->tmp_buff);

    return CPI_OK;
}

int cpi_util_write_lf(cpi_t *p_cpi)
{
  //  printf( "LF" );
    return cpi_util_write_str(p_cpi, "\n");
}

int cpi_util_write_eof(cpi_t *p_cpi)
{
  //  printf( "EOF" );
    return cpi_util_write_str(p_cpi, "\x0D");
}

int cpi_util_read_str(cpi_t *p_cpi, char *str, int *p_size)
{
    int v=0;

    while(v < *p_size)
    {
        int bytes_read = read(p_cpi->serial_file, &str[v], sizeof(char));

        /*! handle read failure */
        if(bytes_read != sizeof(char)) { printf( "returned %d bytes.\n", bytes_read); fflush(stdout); return CPI_FAIL; }
	//	printf( "%c", str[v] );
	fflush(stdout);
        /*! ignore sync character */
        if(str[v] == '?')
        {
            /*! detect failure response from CP */
            /*! @todo needs testing - should parse 0x0D if necessary */
            //if( (v == 3) && (strcmp(str, "CMD?") == 0) ) { return CPI_FAIL; }

            /*! otherwise, keep parsing */
            continue; 
        }

        /*! detect FAIL result */
        if( (v == 3) && (strncmp(str, "FAIL", 4) == 0) ) { return CPI_FAIL; }
        /*! detect failure due to auth count being exceeded */
        if( (v == 8) && (strncmp(str, "AUTHCOUNT", 9) == 0) ) { return CPI_ACCESS_DENIED; }

        /*! handle stop character */
        if(str[v] == (char)0x0D) { break; }

        v++;
    }

    /*! append null terminator, if there is room */
    if(v < *p_size) { str[v++] = '\0'; } 

    /*! return result size */
    *p_size = v;

    return CPI_OK;
}

int cpi_util_read_eof(cpi_t *p_cpi)
{
#ifndef CNPLATFORM_falconwing
#ifndef CNPLATFORM_silvermoon
    char c = '\0';

    int bytes_read = read(p_cpi->serial_file, &c, sizeof(char));

    /*! handle read failure */
    if(bytes_read != sizeof(char)) { return CPI_FAIL; }

    /*! we are expecting an EOF 0x0D character */
    if(c != (char)0x0D) { return CPI_FAIL; }
#endif
#endif
    return CPI_OK;
}

int cpi_util_decode_str(cpi_t *p_cpi, char *str, int size, void *data, int *p_size)
{
    size_t ret = b64_decode(str, size, p_cpi->tmp_buff, CPI_MAX_RESULT_SIZE);

    /*! failed to decode */
    if( (ret == 0) || (ret > CPI_MAX_RESULT_SIZE) ) { return CPI_FAIL; }

    /*! return decoded data, respecting max sizes */
    memcpy(data, p_cpi->tmp_buff, (ret+1 > *p_size) ? *p_size : ret+1);

    return CPI_OK;
}

