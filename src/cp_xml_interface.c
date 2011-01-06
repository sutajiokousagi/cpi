/*
 * cp_xml_interface.c
 *
 * Aaron "Caustik" Robinson
 * (c) Copyright Chumby Industries, 2007
 * All rights reserved
 */

#include "cp_interface.h"
#include "cp_utility.h"

#include <string.h>
#include <stdio.h>
#include <expat.h>

/*! utility structure for parser context */
typedef struct _parser_context
{
    /*! cpi instance handle */
    cpi_t *p_cpi;
    /*! output XML string */
    char *out_xml_str;
    /*! current parser state */
    enum parser_state
    {
        PARSER_STATE_CPI_BEG,       /*! <cpi> */
        PARSER_STATE_CPI_END,       /*! </cpi> */
        PARSER_STATE_QUERY_LIST,    /*! <query_list> */
        PARSER_STATE_QUERY_BEG,     /*! <query> */
        PARSER_STATE_QUERY_END,     /*! </query> */
        PARSER_STATE_FAIL,          /*! Parse failure */
        PARSER_STATE_SUCCESS        /*! Parse success */
    }
    cur_state;
    /*! flag specifying if we are validating */
    int validate_state;
}
parser_context;

/*! expat element start handler */
static void expat_handler_element_start(void *usr_data, const XML_Char *name, const XML_Char **attr);
/*! expat element end handler */
static void expat_handler_element_end(void *usr_data, const XML_Char *name);
/*! query execution function */
static int execute_query(parser_context *p_context, const char **attr);
/*! output error condition */
static void output_condition_failure(parser_context *p_context, const char *type, int ret);
/*! output success condition (begin) */
static void output_condition_success_beg(parser_context *p_context, const char *type);
/*! output success condition (end) */
static void output_condition_success_end(parser_context *p_context);
/*! retrieve attribute with the specified name - string type */
static int get_attr_str(const char **attr, const char *attr_str, char const **p_attr_val);
/*! retrieve attribute with the specified name - uint16_t type */
static int get_attr_uint16(const char **attr, const char *attr_str, uint16_t *p_attr_val);

/*! character encoding */
static const char *expat_char_encoding = "US-ASCII";

/*! max size of output string */
static int max_size = CPI_MAX_RESULT_SIZE;

int cpi_process_xml(struct _cpi_t *p_cpi, char *inp_xml_str, char *out_xml_str)
{
    int success = 0;

    enum XML_Status status;

    /*! create parser instance */
    XML_Parser xml_parser = XML_ParserCreate(expat_char_encoding);

    /*! validate XML */
    {
        /*! initialize parser context, in validate mode */
        parser_context context = { p_cpi, out_xml_str, PARSER_STATE_CPI_BEG, 1 };

        /*! set user data */
        XML_SetUserData(xml_parser, &context);

        /*! set start and end handlers for parsing */
        XML_SetElementHandler(xml_parser, expat_handler_element_start, expat_handler_element_end);

        /*! parse document, validating syntax */
        status = XML_Parse(xml_parser, inp_xml_str, strlen(inp_xml_str), 1);

        /*! update success state, if appropriate */
        success = (context.cur_state == PARSER_STATE_SUCCESS);
    }

    /*! if validation went okay, parse again while executing queries */
    if( (status == XML_STATUS_OK) && success && (XML_ParserReset(xml_parser, expat_char_encoding) == XML_TRUE) )
    {
        /*! initialize parser context, in execution mode */
        parser_context context = { p_cpi, out_xml_str, PARSER_STATE_CPI_BEG, 0 };

        /*! write XML header */
        strcpy(out_xml_str, "<?xml version='1.0'?>\n");
        
        /*! begin basic XML template */
        strncat(out_xml_str, "<cpi version='1.0'>\n", max_size);
        strncat(out_xml_str, "  <response_list>\n", max_size);

        /*! set user data */
        XML_SetUserData(xml_parser, &context);

        /*! set start and end handlers for parsing */
        XML_SetElementHandler(xml_parser, expat_handler_element_start, expat_handler_element_end);

        /*! parse document, executing queries */
        status = XML_Parse(xml_parser, inp_xml_str, strlen(inp_xml_str), 1);

        /*! finish basic XML template */
        strncat(out_xml_str, "  </response_list>\n", max_size);
        strncat(out_xml_str, "</cpi>\n", max_size);

        /*! update success state, if appropriate */
        success = (context.cur_state == PARSER_STATE_SUCCESS);
    }

    /*! free parser instance */
    XML_ParserFree(xml_parser);

    /*! check XML parsing return code */
    if( (status != XML_STATUS_OK) || !success ) { return CPI_FAIL; }

    return CPI_OK;
}

static void expat_handler_element_start(void *usr_data, const XML_Char *name, const XML_Char **attr)
{
    parser_context *p_context = (parser_context*)usr_data;

    switch(p_context->cur_state)
    {
        case PARSER_STATE_CPI_BEG:
        {
            if(strncmp(name, "cpi", strlen("cpi")) != 0) { p_context->cur_state = PARSER_STATE_FAIL; break; }

            p_context->cur_state = PARSER_STATE_QUERY_LIST;
        }
        break;

        case PARSER_STATE_QUERY_LIST:
        {
            if(strncmp(name, "query_list", strlen("query_list")) != 0) { p_context->cur_state = PARSER_STATE_FAIL; break; }

            p_context->cur_state = PARSER_STATE_QUERY_BEG;
        }
        break;

        case PARSER_STATE_QUERY_BEG:
        {
            if(strncmp(name, "query", strlen("query")) != 0) { p_context->cur_state = PARSER_STATE_FAIL; break; }

            /*! optionally, execute query */
            if(!p_context->validate_state) 
            { 
                /*! @note we currently ignore query elements with invalid parameters */
                execute_query(p_context, (const char**)attr); 
            }

            p_context->cur_state = PARSER_STATE_QUERY_END;
        }
        break;

        default:
            break;
    }

    return;
}

static void expat_handler_element_end(void *usr_data, const XML_Char *name)
{
    parser_context *p_context = (parser_context*)usr_data;

    switch(p_context->cur_state)
    {
        case PARSER_STATE_CPI_END:
        {
            if(strncmp(name, "cpi", strlen("cpi")) != 0) { p_context->cur_state = PARSER_STATE_FAIL; break; }

            p_context->cur_state = PARSER_STATE_SUCCESS;
        }
        break;

        case PARSER_STATE_QUERY_BEG:
        {
            if(strncmp(name, "query_list", strlen("query_list")) != 0) { p_context->cur_state = PARSER_STATE_FAIL; break; }

            p_context->cur_state = PARSER_STATE_CPI_END;
        }
        break;

        case PARSER_STATE_QUERY_END:
        {
            if(strncmp(name, "query", strlen("query")) != 0) { p_context->cur_state = PARSER_STATE_FAIL; break; }

            p_context->cur_state = PARSER_STATE_QUERY_BEG;
        }
        break;

        default:
            p_context->cur_state = PARSER_STATE_FAIL;
            break;
    }

    return;
}

static int execute_query(parser_context *p_context, const char **attr)
{
    const char *type = 0;

    int ret = get_attr_str(attr, "type", &type);

    if(CPI_FAILED(ret)) { return ret; }

    /*! handle PIDX query */
    if(strncmp(type, "pidx", strlen("pidx")) == 0)
    {
        uint16_t key_id = 0;

        /*! retrieve key_id */
        {
            ret = get_attr_uint16(attr, "key_id", &key_id);

            if(CPI_FAILED(ret)) { return ret; }
        }

        ret = cpi_get_putative_id(p_context->p_cpi, key_id, p_context->p_cpi->tmp_buff_xml);

        if(CPI_FAILED(ret))
        {
            output_condition_failure(p_context, type, ret);
        }
        else
        {
            output_condition_success_beg(p_context, type);
            strncat(p_context->out_xml_str, p_context->p_cpi->tmp_buff_xml, max_size);
            output_condition_success_end(p_context);
        }
    }
    /*! handle PKEY query */
    else if(strncmp(type, "pkey", strlen("pkey")) == 0)
    {
        uint16_t key_id = 0;

        /*! retrieve key_id */
        {
            ret = get_attr_uint16(attr, "key_id", &key_id);

            if(CPI_FAILED(ret)) { return ret; }
        }

        ret = cpi_get_public_key(p_context->p_cpi, key_id, p_context->p_cpi->tmp_buff_xml);

        if(CPI_FAILED(ret))
        {
            output_condition_failure(p_context, type, ret);
        }
        else
        {
            output_condition_success_beg(p_context, type);
            strncat(p_context->out_xml_str, "\n", max_size);
            strncat(p_context->out_xml_str, p_context->p_cpi->tmp_buff_xml, max_size);
            strncat(p_context->out_xml_str, "    ", max_size);
            output_condition_success_end(p_context);
        }
    }
    /*! handle VERS query */
    else if(strncmp(type, "vers", strlen("vers")) == 0)
    {
        uint16_t major = 0, minor = 0, fix = 0;

        ret = cpi_get_version_number(p_context->p_cpi, &major, &minor, &fix);

        if(CPI_FAILED(ret))
        {
            output_condition_failure(p_context, type, ret);
        }
        else
        {
            /*! generate version string major.minor.fix (e.g. 4.2.0) */
            sprintf(p_context->p_cpi->tmp_buff_xml, "%d.%d.%d", major, minor, fix);

            output_condition_success_beg(p_context, type);
            strncat(p_context->out_xml_str, p_context->p_cpi->tmp_buff_xml, max_size);
            output_condition_success_end(p_context);
        }
    }
    /*! handle TIME query */
    else if(strncmp(type, "time", strlen("time")) == 0)
    {
        uint32_t cur_time = 0;

        ret = cpi_get_current_time(p_context->p_cpi, &cur_time);

        if(CPI_FAILED(ret))
        {
            output_condition_failure(p_context, type, ret);
        }
        else
        {
            /*! convert time to string format */
            sprintf(p_context->p_cpi->tmp_buff_xml, "%u", cur_time);

            output_condition_success_beg(p_context, type);
            strncat(p_context->out_xml_str, p_context->p_cpi->tmp_buff_xml, max_size);
            output_condition_success_end(p_context);
        }
    }
    /*! handle CKEY query */
    else if(strncmp(type, "ckey", strlen("ckey")) == 0)
    {
        uint32_t cur_oki = 0;

        ret = cpi_get_owner_key_index(p_context->p_cpi, &cur_oki);

        if(CPI_FAILED(ret))
        {
            output_condition_failure(p_context, type, ret);
        }
        else
        {
            /*! convert owner key to string format */
            sprintf(p_context->p_cpi->tmp_buff_xml, "%u", cur_oki);

            output_condition_success_beg(p_context, type);
            strncat(p_context->out_xml_str, p_context->p_cpi->tmp_buff_xml, max_size);
            output_condition_success_end(p_context);
        }
    }
    /*! handle SNUM query */
    else if(strncmp(type, "snum", strlen("snum")) == 0)
    {
        uint8_t serial_number[CPI_SERIAL_NUMBER_SIZE] = { 0 };

        ret = cpi_get_serial_number(p_context->p_cpi, serial_number);

        if(CPI_FAILED(ret))
        {
            output_condition_failure(p_context, type, ret);
        }
        else
        {
            /*! blank out temporary buffer */
            strcpy(p_context->p_cpi->tmp_buff_xml, "");

            /*! convert serial number to string format */
            {
                char buff[8];

                int v;
                for(v=0;v<CPI_SERIAL_NUMBER_SIZE;v++)
                {
                    sprintf(buff, "%.02X", (uint32_t)serial_number[v]);
                    strncat(p_context->p_cpi->tmp_buff_xml, buff, max_size);
                }
            }

            output_condition_success_beg(p_context, type);
            strncat(p_context->out_xml_str, p_context->p_cpi->tmp_buff_xml, max_size);
            output_condition_success_end(p_context);
        }
    }
    /*! handle HWVR query */
    else if(strncmp(type, "hwvr", strlen("hwvr")) == 0)
    {
        uint8_t hardware_version[CPI_HARDWARE_VERSION_SIZE] = { 0 };

        ret = cpi_get_hardware_version_data(p_context->p_cpi, hardware_version);

        if(CPI_FAILED(ret))
        {
            output_condition_failure(p_context, type, ret);
        }
        else
        {
            /*! blank out temporary buffer */
            strcpy(p_context->p_cpi->tmp_buff_xml, "");

            /*! convert serial number to string format */
            {
                char buff[8];

                int v;
                for(v=0;v<CPI_HARDWARE_VERSION_SIZE;v++)
                {
                    sprintf(buff, "%.02X", (uint32_t)hardware_version[v]);
                    strncat(p_context->p_cpi->tmp_buff_xml, buff, max_size);
                }
            }

            output_condition_success_beg(p_context, type);
            strncat(p_context->out_xml_str, p_context->p_cpi->tmp_buff_xml, max_size);
            output_condition_success_end(p_context);
        }
    }
    /*! handle CHAL query */
    else if(strncmp(type, "chal", strlen("chal")) == 0)
    {
        uint16_t key_id = 0;
        uint8_t rand_data[CPI_RNDX_SIZE] = { 0 };

        /*! retrieve key_id */
        {
            ret = get_attr_uint16(attr, "key_id", &key_id);

            if(CPI_FAILED(ret)) { return ret; }
        }

        /*! parse random data input */
        {
            const char *rand_data_str = 0;

            ret = get_attr_str(attr, "rand_data", (char const **)&rand_data_str);

            if(CPI_FAILED(ret)) { return ret; }

            /*! handle invalid # of characters */
            if(strlen(rand_data_str) < (CPI_RNDX_SIZE*2)) { return CPI_FAIL; }

            int v;
            for(v=0;v<CPI_RNDX_SIZE;v++)
            {
                uint32_t cur_val = 0;

                ret = sscanf(&rand_data_str[v*2], "%02X", &cur_val);

                if(ret != 1) { return CPI_FAIL; }

                rand_data[v] = (uint8_t)cur_val;
            }
        }

        uint8_t *result1 = (uint8_t*)&p_context->p_cpi->tmp_buff_xml[0];
        uint8_t *result2 = (uint8_t*)&p_context->p_cpi->tmp_buff_xml[CPI_RESULT1_SIZE];
        uint8_t *result3 = (uint8_t*)&p_context->p_cpi->tmp_buff_xml[CPI_RESULT1_SIZE + CPI_RESULT2_SIZE];

        /*! clear result buffers */
        memset(result1, 0, CPI_RESULT1_SIZE);
        memset(result2, 0, CPI_RESULT2_SIZE);
        memset(result3, 0, CPI_RESULT3_SIZE);

        ret = cpi_issue_challenge(p_context->p_cpi, key_id, rand_data, result1, result2, result3);

        if(CPI_FAILED(ret))
        {
            output_condition_failure(p_context, type, ret);
        }
        else
        {
            output_condition_success_beg(p_context, type);

            strncat(p_context->out_xml_str, "\n", max_size);
            strncat(p_context->out_xml_str, "      <enc_owner_key>", max_size);

            /*! convert result1 to string format */
            {
                char buff[8];

                int v;
                for(v=0;v<CPI_RESULT1_SIZE;v++)
                {
                    sprintf(buff, "%.02X", (uint32_t)result1[v]);
                    strncat(p_context->out_xml_str, buff, max_size);

                    if(v == (CPI_RESULT1_ENC_OK_SIZE-1)) 
                    { 
                        strncat(p_context->out_xml_str, "</enc_owner_key>\n", max_size);
                        strncat(p_context->out_xml_str, "      <rand_data>", max_size);
                    }
                    else if(v == (CPI_RESULT1_ENC_OK_SIZE+CPI_RESULT1_RAND_SIZE-1))
                    {
                        strncat(p_context->out_xml_str, "</rand_data>\n", max_size);
                        strncat(p_context->out_xml_str, "      <vers>", max_size);
                    }
                    else if(v == (CPI_RESULT1_SIZE-1))
                    {
                        strncat(p_context->out_xml_str, "</vers>\n", max_size);
                    }
                }
            }

            strncat(p_context->out_xml_str, "      <signature>", max_size);

            /*! convert result2 to string format */
            {
                char buff[8];

                int v;
                for(v=0;v<CPI_RESULT2_SIZE;v++)
                {
                    sprintf(buff, "%.02X", (uint32_t)result2[v]);
                    strncat(p_context->out_xml_str, buff, max_size);
                }
            }
#if defined(CNPLATFORM_falconwing)
	    {
                char buff[8];

                int v;
                for(v=0;v<CPI_RESULT3_SIZE;v++)
                {
                    sprintf(buff, "%.02X", (uint32_t)result3[v]);
                    strncat(p_context->out_xml_str, buff, max_size);
                }
	    }
#endif

            strncat(p_context->out_xml_str, "</signature>\n", max_size);
            strncat(p_context->out_xml_str, "    ", max_size);

            output_condition_success_end(p_context);
        }
    }
    /*! handle unknown query */
    else
    {
        strncat(p_context->out_xml_str, "    <response result=\"failure\">Unknown \"type\" value</response>\n", max_size);
    }

    return CPI_OK;
}

static void output_condition_failure(parser_context *p_context, const char *type, int ret)
{
    strncat(p_context->out_xml_str, "    <response type=\"", max_size);
    strncat(p_context->out_xml_str, type, max_size);
    strncat(p_context->out_xml_str, "\"", max_size);
    strncat(p_context->out_xml_str, " result=\"failure\">", max_size);
    strncat(p_context->out_xml_str, CPI_RETURN_CODE_LOOKUP[ret], max_size);
    strncat(p_context->out_xml_str, "</response>\n", max_size);
    return;
}

static void output_condition_success_beg(parser_context *p_context, const char *type)
{
    strncat(p_context->out_xml_str, "    <response type=\"", max_size);
    strncat(p_context->out_xml_str, type, max_size);
    strncat(p_context->out_xml_str, "\"", max_size);
    strncat(p_context->out_xml_str, " result=\"success\">", max_size);
    return;
}

static void output_condition_success_end(parser_context *p_context)
{
    strncat(p_context->out_xml_str, "</response>\n", max_size);
    return;
}

static int get_attr_str(const char **attr, const char *attr_str, char const **p_attr_val)
{
    int v;

    for(v=0; attr[v] != 0; v+=2)
    {
        const char *cur_attr_str = attr[v+0];
        const char *cur_attr_val = attr[v+1];

        if(strncmp(cur_attr_str, attr_str, strlen(attr_str)) == 0)
        {
            *p_attr_val = cur_attr_val;
            return CPI_OK;
        }
    }

    return CPI_FAIL;
}

static int get_attr_uint16(const char **attr, const char *attr_str, uint16_t *p_attr_val)
{
    const char *attr_val = 0;

    int ret = get_attr_str(attr, attr_str, &attr_val);

    if(CPI_FAILED(ret)) { return ret; }

    if(sscanf(attr_val, "%hu", p_attr_val) != 1) { return CPI_FAIL; }

    return CPI_OK;
}

