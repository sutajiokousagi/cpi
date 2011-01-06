/*
 * $Id$
 * main.c
 *
 * Aaron "Caustik" Robinson
 * (c) Copyright Chumby Industries, 2007-8
 * All rights reserved
 *
 * This module defines the entry point for the cpi command line application.
 */

#include "cp_interface.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <memory.h>
#include <pthread.h>

#define VER_STR "1.03"

/*! default serial port device path - can be overridden via -t option at runtime */
#if defined(CNPLATFORM_stormwind)
#define SERIAL_DEVICE_PATH "/dev/ttySAC1"
#elif defined(CNPLATFORM_falconwing)
#define SERIAL_DEVICE_PATH "/tmp/.cpid"
//#elif defined(CNPLATFORM_ironforge) || defined(CNPLATFORM_silvermoon)
#else
#define SERIAL_DEVICE_PATH "/dev/ttyS2"
#endif

/*! print program usage screen */
static void show_usage();

/*! utility function to print raw data */
static void print_raw(uint8_t *raw_data, int size);

/*! @hack @todo this thread exists in order to monitor that
 *  the CPI is not in a hung state. If the CPI is deemed to
 *  be hung, this thread will call exit(1). This is necessary
 *  due to errors in the serial interface. */
static void *timeout_hack(void *p_context);

/*! @hack @todo utility to enable timeout for given number of seconds */
static void enable_timeout(int seconds);

/*! @hack @todo this variable enables the timeout_hack check */
static volatile int timeout_hack_enabled = 0;

/*! @hack @todo this variable specifies the number of seconds
 *  remaining for the timeout_hack monitoring thread, to decide
 *  when to consider the process hung. */
static volatile int timeout_hack_seconds = 0;

/*! @hack @todo this variable tells the timeout_hack thread when
 *  to stop monitoring and return */
static volatile int timeout_hack_stop = 0;

int main(int argc, char **argv)
{
    /*! default at failure */
    int main_ret = 1;

    /*! input XML file, if specified */
    FILE *inp_xml_file = 0;

    /*! output XML file, if specified */
    FILE *out_xml_file = 0;

	/*! serial device path */
	const char *serial_device_path = SERIAL_DEVICE_PATH;

    /*! CPI instance */
    cpi_t *p_cpi = 0;

    /*! options for stdout */
    int print_putative_id = 0;
    int print_all = 0;
    int print_usage = 0;

    /*! options for power down and alarm */
    int do_shutdown = 0;
    int do_alarm = 0;
    uint32_t alarm_time = 0;

    /*! key id */
    uint16_t key_id = 0;

    /*! raw buffer, used to parse results */
    uint8_t *tmp_buffer1 = 0, *tmp_buffer2 = 0, *tmp_buffer3 = 0;

    /*! @hack @todo thread for monitoring timeout */
    pthread_t timeout_thread;

    /*! initialize timeout thread */
    pthread_create(&timeout_thread, 0, timeout_hack, 0);

    /*! print usage if there are no arguments */
    if(argc <= 1) { print_usage = 1; }

    /*! initialize temporary buffers */
    {
        tmp_buffer1 = (uint8_t*)malloc(CPI_MAX_RESULT_SIZE);

        if(tmp_buffer1 == 0)
        {
            fprintf(stderr, "Error: could not allocate %d bytes for tmp_buffer1\n", CPI_MAX_RESULT_SIZE);
            return 1;
        }

        tmp_buffer2 = (uint8_t*)malloc(CPI_MAX_RESULT_SIZE);

        if(tmp_buffer2 == 0)
        {
            fprintf(stderr, "Error: could not allocate %d bytes for tmp_buffer2\n", CPI_MAX_RESULT_SIZE);
            return 1;
        }

        tmp_buffer3 = (uint8_t*)malloc(CPI_MAX_RESULT_SIZE);

        if(tmp_buffer3 == 0)
        {
            fprintf(stderr, "Error: could not allocate %d bytes for tmp_buffer3\n", CPI_MAX_RESULT_SIZE);
            return 1;
        }
    }

    /*! parse command line */
    {
        int cur_arg = 0;

        for(cur_arg = 1; cur_arg < argc; cur_arg++)
        {
            /*! expect all options to begin with '-' character */
            if(argv[cur_arg][0] != '-')
            {
                fprintf(stderr, "Warning: Unrecognized option \"%s\"\n", argv[cur_arg]);
                continue;
            }

            /*! process command options */
            switch(argv[cur_arg][1])
            {
                case 'k':
                {
                    /*! skip over to key ID */
                    if(++cur_arg >= argc) { break; }

                    /*! attempt to parse key ID */
                    sscanf(argv[cur_arg], "%hu", &key_id);
                }
                break;

				case 't':
				{
					/*! Skip to port path */
					if (++cur_arg >= argc) { break; }

					serial_device_path = argv[cur_arg];
					fprintf( stderr, "Using non-default serial port %s\n", serial_device_path );
				}
				break;

                case 'r':
                {
                    /*! skip over to filename */
                    if(++cur_arg >= argc) { break; }

                    /*! attempt to open inpt file */
                    inp_xml_file = fopen(argv[cur_arg], "rt");

                    /*! report file open error to user */
                    if(inp_xml_file == 0)
                    {
                        fprintf(stderr, "Error: Could not open \"%s\" for reading.\n", argv[cur_arg]);
                    }
                }
                break;

                case 'w':
                {
                    /*! skip over to filename */
                    if(++cur_arg >= argc) { break; }

                    /*! attempt to open output file */
                    out_xml_file = fopen(argv[cur_arg], "wt");

                    /*! report file open error to user */
                    if(out_xml_file == 0)
                    {
                        fprintf(stderr, "Error: Could not open \"%s\" for writing\n", argv[cur_arg]);
                    }
                }
                break;

                case 'i':
                {
                    if(inp_xml_file == 0) { inp_xml_file = stdin; }
                }
                break;

                case 'o':
                {
                    if(out_xml_file == 0) { out_xml_file = stdout; }
                }
                break;

                case 'p':
                    print_putative_id = 1;
                    break;

                case 'a':
                {
                    do_alarm = 1;

                    /*! skip over to alarm time */
                    if(++cur_arg >= argc) { break; }

                    /*! attempt to parse key ID */
                    sscanf(argv[cur_arg], "%u", &alarm_time);
                }
                break;

                case 's':
                    do_shutdown = 1;
                    break;

                case 'd':
                    print_all = 1;
                    break;

                case '-':
                    print_usage = 1;
                    break;

                default:
                    fprintf(stderr, "Warning: Unrecognized option \"%s\"\n", argv[cur_arg]);
                    break;
            }
        }
    }

    /*! optionally, print usage */
    if(print_usage)
    {
        show_usage();
        goto cleanup;
    }

    /*! create CPI instance */
    {
        cpi_info_t cpi_info = { 0 };

        /*!< @hack @todo enable hung CPI timeout */
        enable_timeout(10);

        int ret = cpi_create(&cpi_info, &p_cpi);

        if(CPI_FAILED(ret))
        {
            fprintf(stderr, "Error: cpi_create failed (%s)\n", CPI_RETURN_CODE_LOOKUP[ret]);
            goto cleanup;
        }
    }

    /*! initialize CPI instance */
    {
        /*!< @hack @todo enable hung CPI timeout */
        enable_timeout(10);

        int ret = cpi_init(p_cpi, serial_device_path);

        if(CPI_FAILED(ret))
        {
            fprintf(stderr, "Error: cpi_init failed (%s)\n", CPI_RETURN_CODE_LOOKUP[ret]);
            goto cleanup;
        }
    }

    /*! optionally read input XML data */
    if(inp_xml_file != 0)
    {
        int size = 0;

        /*! read up to max buffer size minus null terminator */
        {
            size_t ret = fread(tmp_buffer1, 1, CPI_MAX_RESULT_SIZE-1, inp_xml_file);

            /*! update size */
            if(ret > 0) { size = ret; }

            /*! append null terminator */
            tmp_buffer1[size] = '\0';
        }

        /*! process XML data */
        {
            /*!< @hack @todo enable hung CPI timeout */
            enable_timeout(10);

            int ret = cpi_process_xml(p_cpi, (char*)tmp_buffer1, (char*)tmp_buffer2);

            if(CPI_FAILED(ret))
            {
                fprintf(stderr, "Error: cpi_process_xml failed (%s)\n", CPI_RETURN_CODE_LOOKUP[ret]);
                goto cleanup;
            }
        }

        /*! optionally write XML output */
        if(out_xml_file != 0)
        {
            int size = strlen((char*)tmp_buffer2);

            /*! write output data to out_xml_file */
            if(size > 0)
            {
                size_t ret = fwrite(tmp_buffer2, 1, size, out_xml_file);

                if(ret != size)
                {
                    fprintf(stderr, "Error: fwrite returned %d, expected %d\n", ret, size);
                    goto cleanup;
                }
            }
        }

    }

    /*! get putative ID */
    if(print_all || print_putative_id)
    {
        char str[CPI_PUTATIVE_ID_SIZE];

        /*!< @hack @todo enable hung CPI timeout */
        enable_timeout(4);

        int ret = cpi_get_putative_id(p_cpi, key_id, str);

        if(CPI_FAILED(ret))
        {
            fprintf(stderr, "Error: cpi_get_putative_id failed (%s)\n", CPI_RETURN_CODE_LOOKUP[ret]);
            goto cleanup;
        }

        /*! print to stdout in all format */
        if(print_all)
        {
            printf("Putative ID : %s\n", str);
        }
        /*! print only putative ID */
        else if(print_putative_id)
        {
            printf("%s\n", str);
        }
    }

    /*! get public key */
    if(print_all)
    {
        /*!< @hack @todo enable hung CPI timeout */
        enable_timeout(10);

        int ret = cpi_get_public_key(p_cpi, key_id, (char*)tmp_buffer1);

        if(CPI_FAILED(ret))
        {
            fprintf(stderr, "Error: cpi_get_public_key failed (%s)\n", CPI_RETURN_CODE_LOOKUP[ret]);
            goto cleanup;
        }

        /*! public key contains an endline already */
        printf("Public Key : \n%s", tmp_buffer1);
    }

    /*! get version data */
    if(print_all)
    {
        /*!< @hack @todo enable hung CPI timeout */
        enable_timeout(10);

        int ret = cpi_get_version_data(p_cpi, tmp_buffer1);

        if(CPI_FAILED(ret))
        {
            fprintf(stderr, "Error: cpi_get_version_data failed (%s)\n", CPI_RETURN_CODE_LOOKUP[ret]);
            goto cleanup;
        }

        printf("Version Data :\n");

        print_raw(tmp_buffer1, CPI_VERSION_SIZE);
    }

    /*! get current time */
    if(print_all)
    {
        uint32_t cur_time = 0;

        /*!< @hack @todo enable hung CPI timeout */
        enable_timeout(10);

        int ret = cpi_get_current_time(p_cpi, &cur_time);

        if(CPI_FAILED(ret))
        {
            fprintf(stderr, "Error: cpi_get_current_time failed (%s)\n", CPI_RETURN_CODE_LOOKUP[ret]);
            goto cleanup;
        }

        printf("Current Time : %d seconds since last reboot\n", cur_time);
    }

    /*! get current owner key index */
    if(print_all)
    {
        uint32_t cur_oki = 0;

        /*!< @hack @todo enable hung CPI timeout */
        enable_timeout(10);

        int ret = cpi_get_owner_key_index(p_cpi, &cur_oki);

        if(CPI_FAILED(ret))
        {
            fprintf(stderr, "Error: cpi_get_owner_key_index failed (%s)\n", CPI_RETURN_CODE_LOOKUP[ret]);
            goto cleanup;
        }

        printf("Current Owner Key Index : %d\n", cur_oki);
    }

    /*! get serial number data */
    if(print_all)
    {
        /*!< @hack @todo enable hung CPI timeout */
        enable_timeout(10);

        int ret = cpi_get_serial_number(p_cpi, tmp_buffer1);

        if(CPI_FAILED(ret))
        {
            fprintf(stderr, "Error: cpi_get_serial_number failed (%s)\n", CPI_RETURN_CODE_LOOKUP[ret]);
            goto cleanup;
        }

        printf("Serial Number :\n");

        print_raw(tmp_buffer1, CPI_SERIAL_NUMBER_SIZE);
    }

    /*! get hardware version data */
    if(print_all)
    {
        /*!< @hack @todo enable hung CPI timeout */
        enable_timeout(10);

        int ret = cpi_get_hardware_version_data(p_cpi, tmp_buffer1);

        if(CPI_FAILED(ret))
        {
            fprintf(stderr, "Error: cpi_get_hardware_version_data failed (%s)\n", CPI_RETURN_CODE_LOOKUP[ret]);
            goto cleanup;
        }

        printf("Hardware Version Data :\n");

        print_raw(tmp_buffer1, CPI_HARDWARE_VERSION_SIZE);
    }

    /*! issue challenge */
    if(print_all)
    {
        uint8_t rand_data[0x10] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F };

        memset(tmp_buffer1, 0, CPI_MAX_RESULT_SIZE);
        memset(tmp_buffer2, 0, CPI_MAX_RESULT_SIZE);
        memset(tmp_buffer3, 0, CPI_MAX_RESULT_SIZE);

        /*!< @hack @todo enable hung CPI timeout */
        enable_timeout(10);

        int ret = cpi_issue_challenge(p_cpi, key_id, rand_data, tmp_buffer1, tmp_buffer2, tmp_buffer3);

        if(CPI_FAILED(ret))
        {
            fprintf(stderr, "Error: cpi_issue_challenge failed (%s)\n", CPI_RETURN_CODE_LOOKUP[ret]);
            goto cleanup;
        }

        printf("result1 : \n");

        /*! print result1 */
        print_raw(tmp_buffer1, CPI_RESULT1_SIZE);

        printf("result2 : \n");

        /*! print result1 */
        print_raw(tmp_buffer2, CPI_RESULT2_SIZE);

#if defined(CNPLATFORM_falconwing)        
	printf("result3 : \n");

        /*! print result1 */
        print_raw(tmp_buffer3, CPI_RESULT3_SIZE);
#endif
    }

    /*! set alarm */
    if(do_alarm)
    {
        /*!< @hack @todo enable hung CPI timeout */
        enable_timeout(10);

        int ret = cpi_set_alarm_time(p_cpi, alarm_time);

        if(CPI_FAILED(ret))
        {
            fprintf(stderr, "Error: cpi_set_alarm_time failed (%s)\n", CPI_RETURN_CODE_LOOKUP[ret]);
            goto cleanup;
        }

        printf("Alarm was set for %d seconds from now\n", alarm_time);
    }

    /*! optionally, power down */
    if(do_shutdown)
    {
        /*!< @hack @todo enable hung CPI timeout */
        enable_timeout(10);

        int ret = cpi_trigger_power_down(p_cpi);

        if(CPI_FAILED(ret))
        {
            fprintf(stderr, "Error: cpi_trigger_power_down failed (%s)\n", CPI_RETURN_CODE_LOOKUP[ret]);
            goto cleanup;
        }
    }

    main_ret = 0;

cleanup:

    /*! cleanup input file handle(s) */
    if( (inp_xml_file != 0) && (inp_xml_file != stdin) )
    {
        fclose(inp_xml_file);
        inp_xml_file = 0;
    }

    /*! cleanup file handle(s) */
    if( (out_xml_file != 0) && (out_xml_file != stdout) )
    {
        fclose(out_xml_file);
        out_xml_file = 0;
    }

    if(tmp_buffer2 != 0)
    {
        free(tmp_buffer2);
        tmp_buffer2 = 0;
    }

    if(tmp_buffer1 != 0)
    {
        free(tmp_buffer1);
        tmp_buffer1 = 0;
    }

    /*! cleanup CPI instance */
    if(p_cpi != 0)
    {
        /*!< @hack @todo enable hung CPI timeout */
        enable_timeout(10);

        int ret = cpi_close(p_cpi);

        if(CPI_FAILED(ret))
        {
            fprintf(stderr, "Warning: cpi_close failed (0x%.08X)\n", ret);
            return 1;
        }
    }

    /*! cleanup timeout thread */
    {
        timeout_hack_stop = 1;
        pthread_join(timeout_thread, 0);
    }

    return main_ret;
}

static void show_usage()
{
    printf("CPI " VER_STR " [sean@chumby.com]\n");
    printf("\n");
    printf("Usage : cpi [--help] | [-p] [-a] [-d] [-r]\n");
    printf("\n");
    printf("Interface with Crypto Processor\n");
    printf("\n");
    printf("Options:\n");
    printf("\n");
    printf("    --help      Display this help screen\n");
    printf("\n");
    printf("    -k <KEYID>  Use the specified key ID (default is 0)\n");
    printf("    -r <FILE>   Read query XML from FILE (ignored if valid -i specified)\n");
    printf("    -w <FILE>   Write result XML to FILE (ignored if valid -o specified)\n");
    printf("    -i          Read query XML from stdin\n");
    printf("    -o          Write result XML to stdout\n");
    printf("    -p          Write putative ID of specified key index to stdout\n");
    printf("    -a <TIME>   Set alarm to go off TIME seconds from now\n");
    printf("    -s          Shutdown the chumby (will occur after all other options)\n");
    printf("    -d          Write all CP data to stdout\n");
    printf("    -t <CDEV>   Use CDEV as character-special device to read from\n");
    printf("\n");
    return;
}

static void print_raw(uint8_t *raw_data, int size)
{
    int v;

    for(v=0;v<size;v++)
    {
        printf(" 0x%.02X", (int)raw_data[v]);
        if(v%16 == 15) { printf("\n"); }
    }

    if(v%16 != 0) { printf("\n"); }

    return;
}

static void *timeout_hack(void *p_context)
{
    int count = 0;

    /*! loop continuously, until we are told to stop */
    while(!timeout_hack_stop)
    {
        usleep(100*1000);

        count++;

        /*! if timeout hack is enabled, decrement the seconds count as appropriate,
         *  and exit if the timer has expired */
        if(timeout_hack_enabled)
        {
            /*! only decrement every 10 iterations, since we sleep for 100ms (1/10 second) */
            if((count % 10) == 0)
            {
                timeout_hack_seconds--;

                /*! if timer has expired, exit the process */
                if(timeout_hack_seconds <= 0)
                {
                    fprintf(stderr, "Warning: hang detected, calling exit(2)...\n");

                    /*! return error code '1' to denote error */
                    exit(2);
                }
            }
        }
    }

    return 0;
}

static void enable_timeout(int seconds)
{
    timeout_hack_enabled = 1;
    timeout_hack_seconds = seconds;

    return;
}

