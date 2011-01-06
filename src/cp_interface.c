/*
 * cp_interface.c
 *
 * Aaron "Caustik" Robinson
 * (c) Copyright Chumby Industries, 2007
 * All rights reserved
 */

#include "cp_interface.h"
#include "cp_utility.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>



/*! utility structure, for generic data requests */
typedef struct _request_info
{
    int   raw_size; /*!< size of raw data */
    void *raw_data; /*!< memory pointer containing raw request data */
}
request_info;

/*! utility structure, for generic data responses */
typedef struct _response_info
{
    int   str_size; /*!< size of base64 string */
    int   raw_size; /*!< size of raw data after base64 decode */
    void *raw_data; /*!< memory pointer to fill with decoded raw data */
}
response_info;

/*! utility function for generic data retrieval command */
static int cpi_get_generic_data(struct _cpi_t *p_cpi, char *cmd, char *cmd_ack, request_info *p_request_info, int req_count, response_info *p_response_info, int res_count);

int cpi_create(struct _cpi_info_t *p_cpi_info, struct _cpi_t **pp_cpi)
{
    /*! sanity check - null ptr */
    if(pp_cpi == 0) { return CPI_INVALID_PARAM; }

    /*! allocate associated context */
    cpi_t *cpi = (cpi_t*)malloc(sizeof(cpi_t));

    /*! return allocated context */
    *pp_cpi = cpi;

    /*! set context to default state */
    memset(*pp_cpi, 0, sizeof(cpi_t));
    /*! default state - invalid file */
    cpi->serial_file = -1;
    cpi->tmp_buff = (char*)malloc(CPI_MAX_RESULT_SIZE+1);
    cpi->tmp_buff_xml = (char*)malloc(CPI_MAX_RESULT_SIZE+1);

    return CPI_OK;
}

int cpi_close(struct _cpi_t *p_cpi)
{
    /*! sanity check - null ptr */
    if(p_cpi == 0) { return CPI_INVALID_PARAM; }

    /*! cleanup temp XML buffer */
    if(p_cpi->tmp_buff_xml != 0)
    {
        /*! free temporary XML buffer */
        free(p_cpi->tmp_buff_xml);
    }

    /*! cleanup temp buffer */
    if(p_cpi->tmp_buff != 0)
    {
        /*! free temporary buffer */
        free(p_cpi->tmp_buff);
    }

    /*! cleanup serial device file */
    if(p_cpi->serial_file != -1)
    {
        /*! close serial device file */
        close(p_cpi->serial_file);
    }

    /*! attempt to release lock */
    {
        struct flock fl = { F_UNLCK, SEEK_SET, 0, 0, getpid() };

        fcntl(p_cpi->serial_file, F_SETLK, &fl);
    }

    /*! free associated context */
    free(p_cpi);

    return CPI_OK;
}

int cpi_init(struct _cpi_t *p_cpi, char *serial_device_path)
{
    /*! sanity check - null ptr */
    if(p_cpi == 0) { return CPI_INVALID_PARAM; }

    /*! attempt to open serial device */
#if defined(CNPLATFORM_falconwing)
    struct sockaddr_un cpi_sa;
    int bufsize;


    cpi_sa.sun_family = AF_UNIX;
    strncpy(cpi_sa.sun_path, serial_device_path, sizeof(cpi_sa.sun_path));

    // Create the socket device we'll use to comminucate with the cp emulator.
    if((p_cpi->serial_file = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("Couldn't create socket");
        return -1;
    }



    // Finally, connect to the client.
    if((connect(p_cpi->serial_file, (struct sockaddr*) &cpi_sa, sizeof(cpi_sa))) < 0) {
        perror("Restarting cpid because unable to connect to driver");
        system("killall cpid");
        if(!fork()) {
            execlp("cpid", "cpid", "-d", (char *)NULL);
            perror("Unable to launch cpid");
            exit(0);
        }
        sleep(2);

        // Create a new socket and retry the connection.
        cpi_sa.sun_family = AF_UNIX;
        strncpy(cpi_sa.sun_path, serial_device_path, sizeof(cpi_sa.sun_path));
        if((p_cpi->serial_file = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
            perror("Couldn't create socket");
            return -1;
        }

        if((connect(p_cpi->serial_file, (struct sockaddr*) &cpi_sa, sizeof(cpi_sa))) < 0) {
            perror("Still unable to connect to cpi driver");
            usleep(100000);
            return -1;
        }
    }


    // Set the receive buffer so that it's just big enough to hold a
    // single accel_t structure.
    bufsize = 1;
    if((setsockopt(p_cpi->serial_file, SOL_SOCKET, SO_RCVBUF,
                   &bufsize, sizeof(bufsize))) < 0) {
        perror("Unable to set cpi communications buffer receive size");
        return -1;
    }

    bufsize = 1;
    if((setsockopt(p_cpi->serial_file, SOL_SOCKET, SO_SNDBUF,
                   &bufsize, sizeof(bufsize))) < 0) {
        perror("Unable to set cpi communications buffer send size");
        return -1;
    }
#else
    p_cpi->serial_file = open(serial_device_path, O_RDWR | O_NOCTTY | O_NDELAY);
#endif

    /*! failed to open serial device */
    if(p_cpi->serial_file == -1) { return CPI_FAIL; }

    /*! attempt to obtain lock */
    {
        struct flock fl = { F_WRLCK, SEEK_SET, 0, 0, getpid() };

        if(fcntl(p_cpi->serial_file, F_SETLK, &fl) == -1)
        {
            return CPI_ACCESS_DENIED;
        }
    }

    /*! configure serial device file */
    {
        struct termios options;

        memset(&options, 0, sizeof(options));

        fcntl(p_cpi->serial_file, F_SETFL, 0);

    /*! Only ironforge uses bitrate of 38400bps - all others use 115.2kbps */
        options.c_cflag |= (CLOCAL | CREAD | CS8 | 
#if !defined(CNPLATFORM_ironforge)
        B115200
#else
        B38400
#endif
        );

        /*! simple non-interpet mode, we'll handle that, thank you */
        options.c_iflag = IGNPAR;
        options.c_oflag = 0;
        options.c_lflag = 0;

        /*! this is key. otherwise we end up having the character input block all the time. yuck! */
        options.c_cc[VTIME] = 0;
        options.c_cc[VMIN] = 1;

        /*! commit options */
        tcflush(p_cpi->serial_file, TCIFLUSH);
        tcsetattr(p_cpi->serial_file, TCSANOW, &options);
    }

    /*! we're all initialized now */
    p_cpi->is_initialized = 1;

    return CPI_OK;
}

int cpi_get_putative_id(struct _cpi_t *p_cpi, uint16_t cpi_key_id, char *str)
{
    /*! temporary raw, binary, putative id */
    char raw_pid[0x10];

    /*! use generic data utility function to obtain version data */
    {
        request_info req_info = { .raw_size = sizeof(cpi_key_id), .raw_data = &cpi_key_id };
        response_info res_info = { .str_size = 24, .raw_size = 0x10, .raw_data = raw_pid };

        int ret = cpi_get_generic_data(p_cpi, "!!!!PIDX", "PIDX", &req_info, 1, &res_info, 1);

        if(CPI_FAILED(ret)) { return ret; }
    }

    /*! format raw PID into GUID format */
    sprintf(str, "%.02X%.02X%.02X%.02X-%.02X%.02X-%.02X%.02X-%.02X%.02X-%.02X%.02X%.02X%.02X%.02X%.02X",
        raw_pid[0x00], raw_pid[0x01], raw_pid[0x02], raw_pid[0x03], 
        raw_pid[0x04], raw_pid[0x05], 
        raw_pid[0x06], raw_pid[0x07], 
        raw_pid[0x08], raw_pid[0x09], raw_pid[0x0A], raw_pid[0x0B], 
        raw_pid[0x0C], raw_pid[0x0D], raw_pid[0x0E], raw_pid[0x0F]);

    return CPI_OK;
}

int cpi_get_public_key(struct _cpi_t *p_cpi, uint16_t cpi_key_id, char *str)
{
    request_info req_info = { .raw_size = sizeof(cpi_key_id), .raw_data = &cpi_key_id };
    response_info res_info = { .str_size = CPI_MAX_RESULT_SIZE, .raw_size = 0, .raw_data = str };

    return cpi_get_generic_data(p_cpi, "!!!!PKEY", 0, &req_info, 1, &res_info, 1);
}

int cpi_get_version_data(struct _cpi_t *p_cpi, uint8_t *raw_vers)
{
    response_info res_info = { .str_size = 8, .raw_size = 6, .raw_data = raw_vers };

    /*! use generic data utility function to obtain version data */
    return cpi_get_generic_data(p_cpi, "!!!!VERS", "VRSR", 0, 0, &res_info, 1);
}

/*! utility function to construct a 16 bit integer in host byte order, from
 *  a little endian array of data. This may not ever be necessary, but it is
 *  generally best not to make byte order assumptions. */
static uint16_t util_make16(uint8_t *raw_data) { return (raw_data[0] | (raw_data[1] << 8)); }

int cpi_get_version_number(struct _cpi_t *p_cpi, uint16_t *p_vers_major, uint16_t *p_vers_minor, uint16_t *p_vers_fix)
{
    uint8_t raw_vers[6] = { 0, 0, 0, 0, 0, 0 };

    int ret = cpi_get_version_data(p_cpi, raw_vers);

    if(CPI_FAILED(ret)) { return ret; }

    if(p_vers_major != 0) { *p_vers_major = util_make16(&raw_vers[4]); }
    if(p_vers_minor != 0) { *p_vers_minor = util_make16(&raw_vers[2]); }
    if(p_vers_fix   != 0) { *p_vers_fix   = util_make16(&raw_vers[0]); }

    return CPI_OK;
}

int cpi_set_alarm_time(struct _cpi_t *p_cpi, uint32_t alarm_time)
{
    request_info req_info = { .raw_size = sizeof(alarm_time), .raw_data = &alarm_time };

    int ret = cpi_get_generic_data(p_cpi, "!!!!ALRM", "ASET", &req_info, 1, 0, 0);

    if(CPI_FAILED(ret)) { return ret; }

    return CPI_OK;
}

int cpi_trigger_power_down(struct _cpi_t *p_cpi)
{
    return cpi_get_generic_data(p_cpi, "!!!!DOWN", 0, 0, 0, 0, 0);
}

int cpi_trigger_reset(struct _cpi_t *p_cpi)
{
    return cpi_get_generic_data(p_cpi, "!!!!RSET", 0, 0, 0, 0, 0);
}

int cpi_get_current_time(struct _cpi_t *p_cpi, uint32_t *p_cur_time)
{
    response_info ri = { .str_size = 8, .raw_size = 4, .raw_data = p_cur_time };

    /*! use generic data utility function to obtain current time */
    return cpi_get_generic_data(p_cpi, "!!!!TIME", "TIME", 0, 0, &ri, 1);
}

int cpi_get_owner_key_index(struct _cpi_t *p_cpi, uint32_t *p_oki)
{
    response_info ri = { .str_size = 8, .raw_size = 4, .raw_data = p_oki };

    /*! use generic data utility function to obtain current owner key */
    return cpi_get_generic_data(p_cpi, "!!!!CKEY", "CKEY", 0, 0, &ri, 1);
}

int cpi_get_serial_number(struct _cpi_t *p_cpi, uint8_t *raw_serial)
{
    response_info ri = { .str_size = 24, .raw_size = 16, .raw_data = raw_serial };

    /*! use generic data utility function to obtain serial number data */
    return cpi_get_generic_data(p_cpi, "!!!!SNUM", "SNUM", 0, 0, &ri, 1);
}

int cpi_get_hardware_version_data(struct _cpi_t *p_cpi, uint8_t *raw_hard_vers)
{
    response_info ri = { .str_size = 24, .raw_size = 16, .raw_data = raw_hard_vers };

    /*! use generic data utility function to obtain hardware version data */
    return cpi_get_generic_data(p_cpi, "!!!!HWVR", "HVRS", 0, 0, &ri, 1);
}

int cpi_issue_challenge(struct _cpi_t *p_cpi, uint16_t cpi_key_id, uint8_t *rand_data, uint8_t *result1, uint8_t *result2, uint8_t *result3)
{
    request_info req_info[2] = 
    { 
        { .raw_size = sizeof(cpi_key_id), .raw_data = &cpi_key_id },
        { .raw_size = CPI_RNDX_SIZE, .raw_data = rand_data }
    };

#if defined(CNPLATFORM_falconwing)
    response_info res_info[3] = 
    {
        { .str_size = 373, .raw_size = CPI_RESULT1_SIZE, .raw_data = result1 },
        { .str_size = 173, .raw_size = CPI_RESULT2_SIZE, .raw_data = result2 },
        { .str_size = 567, .raw_size = CPI_RESULT3_SIZE, .raw_data = result3 },
    };

    /*! use generic data utility function to issue challenge */
    return cpi_get_generic_data(p_cpi, "!!!!CHAL", "RESP", req_info, 2, res_info, 3);
#else
    response_info res_info[2] = 
    {
        { .str_size = 373, .raw_size = CPI_RESULT1_SIZE, .raw_data = result1 },
        { .str_size = 173, .raw_size = CPI_RESULT2_SIZE, .raw_data = result2 },
    };

    /*! use generic data utility function to issue challenge */
    return cpi_get_generic_data(p_cpi, "!!!!CHAL", "RESP", req_info, 2, res_info, 2);
#endif
}

static int cpi_get_generic_data(struct _cpi_t *p_cpi, char *cmd, char *cmd_ack, request_info *p_request_info, int req_count, response_info *p_response_info, int res_count)
{
    /*! sanity check - null ptr */
    if(p_cpi == 0) { return CPI_INVALID_PARAM; }

    /*! sanity check - initialization */
    if(!p_cpi->is_initialized) { return CPI_INVALID_CALL; }

    /*! wake up CP */
    {
        int ret = cpi_util_wakeup_cp(p_cpi);
    //  printf( "failed to wake up\n" );
        if(CPI_FAILED(ret)) { return ret; }
    }

    /*! use command to make request */
    {
        int ret = cpi_util_write_str(p_cpi, cmd);

    //  printf( "failed request\n" );
        if(CPI_FAILED(ret)) { return ret; }

        int cur_req = 0;

        /*! optionally, send associated request data */
        for(cur_req = 0; cur_req < req_count; cur_req++)
        {
            ret = cpi_util_write_bin(p_cpi, p_request_info[cur_req].raw_data, p_request_info[cur_req].raw_size);

        //      printf( "failed req data\n" );
            if(CPI_FAILED(ret)) { return ret; }
        }

        ret = cpi_util_write_eof(p_cpi);

    //  printf( "failed write eof\n" );
        if(CPI_FAILED(ret)) { return ret; }
    }

    /*! read and validate cmd_ack */
    if(cmd_ack != 0)
    {
        int size = 4;

        int ret = cpi_util_read_str(p_cpi, p_cpi->tmp_buff, &size);

    //  printf( "failed to read ack\n" );
        if(CPI_FAILED(ret)) { return ret; }

        /*! @todo we should probably return CPI_ACCESS_DENIED when appropriate */

        if( (size != 4) || (strncmp(p_cpi->tmp_buff, cmd_ack, 4) != 0) ) { return CPI_FAIL; }
    }

    int cur_resp = 0;

    /*! retrieve generic data */
    for(cur_resp = 0; cur_resp < res_count; cur_resp++)
    {
        int gen_size = p_response_info[cur_resp].str_size + 1;

        /*! read generic data */
        {
            int ret = cpi_util_read_str(p_cpi, p_cpi->tmp_buff, &gen_size);
        //      printf( "failed to read generic data\n" );

            if(CPI_FAILED(ret)) { return ret; }

            // this check is probably obsolete, since we dont need to know size beforehand
            //if(gen_size != (p_response_info[cur_resp].str_size + 1) ) { return CPI_FAIL; }

            /*! change LF to null termninator */
            p_cpi->tmp_buff[gen_size] = '\0';
        }

        /*! decode base64 data into raw data */
        if(p_response_info[cur_resp].raw_size > 0)
        {
            int size = p_response_info[cur_resp].raw_size;

            int ret = cpi_util_decode_str(p_cpi, p_cpi->tmp_buff, /*p_response_info[cur_resp].str_size*/gen_size, p_response_info[cur_resp].raw_data, &size);

        //      printf( "failed to decode string\n" );
            if(CPI_FAILED(ret)) { return ret; }

            if(size != p_response_info[cur_resp].raw_size) { return CPI_FAIL; }
        }
        else
        {
            memcpy(p_response_info[cur_resp].raw_data, p_cpi->tmp_buff, gen_size);
        }
    }

    /*! retrieve EOF character */
    {
        cpi_util_read_eof(p_cpi);

        // this check is probably obsolete, since we may have thrown out EOF above */
        //if(CPI_FAILED(ret)) { return ret; }
    }

    return CPI_OK;
}

