/*
 * main.c
 *
 * Aaron "Caustik" Robinson
 * (c) Copyright Chumby Industries, 2007
 * All rights reserved
 *
 * This module defines the entry point for the cpi test application.
 */

#include "main.h"
#include "cp_interface.h"

#include <stdio.h>
#include <malloc.h>
#include <memory.h>
#include <tomcrypt.h>
#include <tommath.h>

/*! serial port device path */
#define SERIAL_DEVICE_PATH "/dev/ttyS2"
/*! key file path */
#define KEYFILE_PATH "keyfile"

/*! helper utility to display hash */
static void display_hash(const char *hash_name, uint8_t *hash);
/*! helper utility to display binary data */
static void display_binary(const char *prefix, const char *name, uint8_t *data, int size);
/*! helper utility to display big number */
static void display_bignum(const char *prefix, const char *name, void *bn);

/*! test program entry point */
int main(int argc, char **argv)
{
    /*! default at failure */
    int main_ret = 1;

    /*! CPI instance */
    cpi_t *p_cpi = 0;

    /*! raw buffer, used to parse results */
    uint8_t *tmp_buffer1 = (uint8_t*)malloc(CPI_MAX_RESULT_SIZE);
    uint8_t *tmp_buffer2 = (uint8_t*)malloc(CPI_MAX_RESULT_SIZE);
    uint8_t *tmp_buffer3 = (uint8_t*)malloc(CPI_MAX_RESULT_SIZE);

    /*! key file */
    FILE *key_file = 0;

    /*! key ID */
    uint16_t key_id = 0;

    /*! success flag */
    int success = 0;

    /*! create CPI instance */
    {
        cpi_info_t cpi_info = { 0 };

        int ret = cpi_create(&cpi_info, &p_cpi);

        if(CPI_FAILED(ret)) 
        { 
            fprintf(stderr, "Error: cpi_create failed (%s)\n", CPI_RETURN_CODE_LOOKUP[ret]);
            goto cleanup;
        }
    }

    /*! initialize CPI instance */
    {
        int ret = cpi_init(p_cpi, SERIAL_DEVICE_PATH);

        if(CPI_FAILED(ret))
        {
            fprintf(stderr, "Error: cpi_init failed (%s)\n", CPI_RETURN_CODE_LOOKUP[ret]);
            goto cleanup;
        }
    }

    /*! open key_file for tests */
    {
        key_file = fopen(KEYFILE_PATH, "rb");

        if(key_file == 0)
        {
            fprintf(stderr, "Error: key_file was not found in \"%s\"\n", KEYFILE_PATH);
            goto cleanup;
        }
    }

    /*! initialize libtomcrypt */
    {
        /*! register prng/hash */
        if(register_prng(&sprng_desc) == -1)
        {
            fprintf(stderr, "Error: register_prng failed!\n");
            goto cleanup;
        }

        /*! register math library @note we need to register libtommath instead of fastmath since
         *  we need mp_exteuclid which is not supported by fastmath, and the big number formats do
         *  not appear to be compatible. in the future, we could convert between the two formats
         *  and then perform the extended euclidean algorithm - but this should suffice for testing
         *  purposes */
        ltc_mp = ltm_desc;

        /*! register sha-1 hash function */
        if(register_hash(&sha1_desc) == -1)
        {
            fprintf(stderr, "Error: register_hash failed!\n");
            goto cleanup;            
        }
    }

    /*! display PIDX for debugging purposes */
    {
        int ret = cpi_get_putative_id(p_cpi, key_id, (char*)tmp_buffer1);

        if(CPI_FAILED(ret))
        {
            fprintf(stderr, "Error: cpi_get_putative_id failed (%s)\n", CPI_RETURN_CODE_LOOKUP[ret]);
            goto cleanup;
        }

        printf("PIDX: %s\n\n", tmp_buffer1);
    }

    /*! issue challenge */
    {
        /*! input random data */
        uint8_t rand_data_inp[CPI_RNDX_SIZE] = { 0 };
        /*! current key data */
        key_entry cur_key;

        /*! generate random input data */
        {
            prng_state prng;

            int ret = yarrow_start(&prng);

            if(ret != CRYPT_OK)
            {
                fprintf(stderr, "Error: yarrow_start failed (%d)\n", ret);
                goto cleanup;
            }

            ret = yarrow_add_entropy((uint8_t*)"cpi_test", 8, &prng);

            if(ret != CRYPT_OK)
            {
                fprintf(stderr, "Error: yarrow_add_entropy failed (%d)\n", ret);
                goto cleanup;
            }

            ret = yarrow_ready(&prng);

            if(ret != CRYPT_OK)
            {
                fprintf(stderr, "Error: yarrow_ready failed (%d)\n", ret);
                goto cleanup;
            }

            ret = yarrow_read(rand_data_inp, CPI_RNDX_SIZE, &prng);

            if(ret != CPI_RNDX_SIZE)
            {
                fprintf(stderr, "Error: yarrow_read failed (%d)\n", ret);
                goto cleanup;
            }

            display_binary("", "rndx", rand_data_inp, CPI_RNDX_SIZE);
        }

        int ret = cpi_issue_challenge(p_cpi, key_id, rand_data_inp, tmp_buffer1, tmp_buffer2, tmp_buffer3);

        if(CPI_FAILED(ret))
        {
            fprintf(stderr, "Error: cpi_issue_challenge failed (%s)\n", CPI_RETURN_CODE_LOOKUP[ret]);
            goto cleanup;
        }
       
        /*! read current keyfile entry */
        {
            ret = fread(&cur_key, sizeof(cur_key), 1, key_file);

            if(ret != 1)
            {
                fprintf(stderr, "Error: fread failed (%d)\n", ret);
                goto cleanup;
            }
        }

        /*! calculate hash of local values with result */
        {
            uint8_t pidx_hash[20];
            uint8_t full_hash[20];

            uint8_t *enc_owner_key = &tmp_buffer1[0];
            uint8_t *rand_data_out = &tmp_buffer1[CPI_RESULT1_ENC_OK_SIZE];
            uint8_t *vers_raw = &tmp_buffer1[CPI_RESULT1_ENC_OK_SIZE+CPI_RESULT1_RAND_SIZE];
            uint8_t *sig_raw = &tmp_buffer2[0];

            rsa_key key;

            /*! blinded signature data */
            void *bn_sig_mpn = 0;

            /*! blinded random output data */
            void *bn_rm_mpn = 0;

            /*! blinding factor inverse */
            void *bn_binv = 0;

            /*! temporary big number */
            void *bn_tmp = 0;

            /*! validate that returned version is equal to expected version */
            {
                /*! expected VERS result */
                uint8_t vers_exp[] = { 0x04, 0x02, 0x00, 0x00 };

                /*! validate vers_raw == vers_exp */
                if(memcmp(vers_exp, vers_raw, CPI_RESULT1_VERS_SIZE) != 0)
                {
                    fprintf(stderr, "Error: cpi_issue_challenge returned incorrect version result ");
                    fprintf(stderr, "raw:{0x%.02X,0x%.02X,0x%.02X,0x%.02X} exp:{0x%.02X,0x%.02X,0x%.02X,0x%.02X}\n",
                            (int)vers_raw[0], (int)vers_raw[1], (int)vers_raw[2], (int)vers_raw[3],
                            (int)vers_exp[0], (int)vers_exp[1], (int)vers_exp[2], (int)vers_exp[3]);
                    goto cleanup;
                }
            }

            /*! calculate hash of PIDx */
            {
                hash_state cur_hash_state;

                sha1_desc.init(&cur_hash_state);
                sha1_desc.process(&cur_hash_state, cur_key.i, 0x10);
                sha1_desc.done(&cur_hash_state, pidx_hash);

                display_binary("", "keyfile id", cur_key.i, 0x10);
            }

            display_hash("PIDX Hash", pidx_hash);

            /*! calculate full hash (x, H(PIDx), rn) with (rm, Paqs(OK), vers) */
            {
                hash_state cur_hash_state;

                int offs = 0;

                sha1_desc.init(&cur_hash_state);

                /*! write hash components */
                {
                    /*! helper macro for appending data to array */
                    #define HELPER_APPEND_DATA(x, s) memcpy(&tmp_buffer3[offs], x, s); offs += s;
                    /*! helper macro for appending byte to array */
                    #define HELPER_APPEND_BYTE(b) { uint8_t cur_byte = b; HELPER_APPEND_DATA(&cur_byte, 1); }

                    HELPER_APPEND_DATA(enc_owner_key, CPI_RESULT1_ENC_OK_SIZE);
                    HELPER_APPEND_DATA(rand_data_inp, CPI_RNDX_SIZE);
                    HELPER_APPEND_DATA(rand_data_out, CPI_RESULT1_RAND_SIZE);
                    HELPER_APPEND_BYTE(0);
                    HELPER_APPEND_BYTE(0);
                    HELPER_APPEND_BYTE((key_id >> 8) & 0xFF);
                    HELPER_APPEND_BYTE((key_id >> 0) & 0xFF);
                    HELPER_APPEND_DATA(pidx_hash, 20);
                    HELPER_APPEND_DATA(vers_raw, CPI_RESULT1_VERS_SIZE);
                }
                
                sha1_desc.process(&cur_hash_state, tmp_buffer3, offs);
                sha1_desc.done(&cur_hash_state, full_hash);
            }

            display_hash("Full Hash", full_hash);

            /*! initialize pair big numbers */
            {
                ret = ltc_init_multi(&key.e, &key.d, &key.N, &key.dQ, &key.dP, &key.qP, &key.p, &key.q, &bn_sig_mpn, &bn_rm_mpn, &bn_binv, &bn_tmp, NULL);

                if(ret != CRYPT_OK)
                {
                    fprintf(stderr, "Error: mp_init_multi failed (%s)\n", error_to_string(ret));
                    goto cleanup;
                }
            }

            /*! initialize keypair */
            {
                key.type = PK_PRIVATE;

                ltc_mp.unsigned_read(key.e, cur_key.e, 4);
                ltc_mp.unsigned_read(key.dP, cur_key.dp, 64);
                ltc_mp.unsigned_read(key.dQ, cur_key.dq, 64);
                ltc_mp.unsigned_read(key.qP, cur_key.qi, 64);

                ltc_mp.unsigned_read(key.N, cur_key.n, 128);
                ltc_mp.unsigned_read(key.p, cur_key.p, 64);
                ltc_mp.unsigned_read(key.q, cur_key.q, 64);
            }

            /*! perform extended euclidean algorithm to undo blinding value */
            {
                ltc_mp.unsigned_read(bn_sig_mpn, sig_raw, CPI_RESULT2_SIZE);
                ltc_mp.unsigned_read(bn_rm_mpn, rand_data_out, CPI_RESULT1_RAND_SIZE);

                /*! display debug information */
                {
                    display_bignum("", "bn_rm_mpn", bn_rm_mpn);
                    display_bignum("", "bn_sig_mpn", bn_sig_mpn);
                    display_bignum("", "key.N", key.N);
                }

                /*! perform inverse mod operation */
                {
                    ret = ltc_mp.invmod(bn_rm_mpn, key.N, bn_binv);

                    //if(ret != CRYPT_OK) { printf("invmod error : %s\n", error_to_string(ret)); }
                    if(ret != MP_OKAY) { printf("mp_exteuclid error : %d\n", ret); }

                    display_bignum("", "bn_binv", bn_binv);
                }

                /*! validate rm_inv * rm mod N */
                {
                    ret = ltc_mp.mulmod(bn_binv, bn_rm_mpn, key.N, bn_tmp);

                    if(ret != CRYPT_OK) 
                    {
                        fprintf(stderr, "Error: ltc_mp.mulmod failed (%s)\n", error_to_string(ret));
                        goto cleanup;
                    }

                    ret = ltc_mp.compare_d(bn_tmp, 1);

                    if(ret != LTC_MP_EQ)
                    {
                        display_bignum("x ", "bn_tmp", bn_tmp);
                        fprintf(stderr, "Error: ltc_mp.compare_d did not return LTC_MP_EQ\n");
                        goto cleanup;
                    }
                }
            }

            /*! unblind message */
            {
                ret = ltc_mp.mulmod(bn_binv, bn_sig_mpn, key.N, bn_tmp);

                if(ret != CRYPT_OK) 
                {
                    fprintf(stderr, "Error: ltc_mp.mulmod failed (%s)\n", error_to_string(ret));
                    goto cleanup;
                }
            }

            /*! perform RSA Pubkey Op on signature, using appropriate public key */
            {
                unsigned long size = 128;

                uint8_t tmpbuf[128] = { 0 };

                memset(tmp_buffer3, 0, CPI_MAX_RESULT_SIZE);

                ltc_mp.unsigned_write(bn_tmp, tmp_buffer3);

                ret = ltc_mp.rsa_me(tmp_buffer3, 128, tmpbuf, &size, PK_PUBLIC, &key);
               
                if(ret != CRYPT_OK) 
                {
                    fprintf(stderr, "Error: ltc_mp.rsa_me failed (%s)\n", error_to_string(ret));
                    goto cleanup;
                }

                /*! validate full hash */
                if(memcmp(full_hash, &tmpbuf[128-20], 20) != 0)
                {
                    fprintf(stderr, "Error: Full hash was not found in decrypted hash\n"); 
                    goto cleanup;
                }

                display_binary("", "decrypted hash", tmpbuf, size);
            }

            /*! perform RSA Privkey Op on owner key, using appropriate private key */
            {
                /*! @todo where do these values come from? (pulled from cpTest.c) */
                static const char *rsa_d = "38EC2EBB24F7880C9A400AFECE0A242BD0CA0F899BB962F2BD3D9CC06874DEE3735D43472F3D1250BDD247D8F2E0C763781B3FE3E3FC160D0CBB71E997AEE5F24E0D2DA83F592DBEB6429F1231A09F299F57C7A63DB088BA40A5F8BCD0348ECB2B8DBAD6645F98D0C0605F7E89E5FA3109997EA1F5D8A01BC30A241774AD6D6F44058C0A75A1D60DCA570DD633BD0D03DB00AA34364712B1178A4F8FB3EE2007AFB59D08577AE68CFFF552B7CA313FAB15ED7B6429104E7AFADE1DDD03EFD784E48F363C86043DD8AE29FF7803D820ABB782C491C7680DEA9A17BF38786BE62DD235D8C803D4D1CFF5F6F5FFC4417F2661A24018659ED2F7B6AA3B079754571";
                static const char *rsa_pn = "BD9F9545D325639D2EA557D404C4FBB1F5EDEA28CEC1919F0668722DC25EECE5B1E8481EBBC371D02B8AE5BDE91665035B4DF9A25C462975126A06ABC14B6E0260CF19B2130779FCE8C121E7CEEBDF02A79C6AAE971A7AAC7428E49B6262487B35E35666FE5E751100DAA483EE92E9735B2DBAA52160088FAE869507BCAE87C2C8924C48A9461044B212951436F2B9E59FF4B266D555505CD9FE21787886B71E002F2CD927ACC8A924D399BE075635FB8092ED80F664A776CE5F64BC6BA49D3AB81E44B520E7629B58361E53F6C909C6460DB276294CB0FA0440B7775A28E13612C92A001BAF5E0345E39F7A1E5C2AF38ADF830C45C4D151F7C0B24C3ED82035";

                ltc_mp.read_radix(key.e, rsa_d, 16);
                ltc_mp.read_radix(key.N, rsa_pn, 16);

                unsigned long size = CPI_RESULT1_ENC_OK_SIZE;

                memset(tmp_buffer3, 0, CPI_MAX_RESULT_SIZE);

                /*! @note we are tricking tomcrypt into doing a privkey op even though we're telling him public. ..sucker */
                ret = ltc_mp.rsa_me(enc_owner_key, CPI_RESULT1_ENC_OK_SIZE, tmp_buffer3, &size, PK_PUBLIC, &key);

                if(ret != CRYPT_OK) 
                {
                    fprintf(stderr, "Error: ltc_mp.rsa_me failed (%s)\n", error_to_string(ret));
                    goto cleanup;
                }

                /*! validate start bytes */
                if( (tmp_buffer3[0] != 0x00) || (tmp_buffer3[1] != 0x02) )
                {
                    fprintf(stderr, "Error: Decrypted owner key first two bytes were incorrect (%.02X %.02X instead of %.02X %.02X)\n", 
                            tmp_buffer3[0], tmp_buffer3[1], 0x00, 0x02);
                    goto cleanup;
                }

                /*! validate 256-16 byte */
                if(tmp_buffer3[256-16-1] != 0x00)
                {
                    fprintf(stderr, "Error: Decrypted owner key 256-16-1 byte was incorrect (%.02X instead of %.02X)\n", 
                            tmp_buffer3[256-16-1], 0x00);
                    goto cleanup;
                }

                display_binary("", "decrypted owner key", tmp_buffer3, size);
            }

            /*! cleanup big numbers */
            ltc_deinit_multi(key.e, key.d, key.N, key.dQ, key.dP, key.qP, key.p, key.q, bn_sig_mpn, bn_rm_mpn, bn_binv, bn_tmp, NULL);
        }
    }

    success = 1;

cleanup:

    printf("Result : %s!\n", success ? "Success" : "Failed!");

    if(key_file != 0)
    {
        fclose(key_file);
        key_file = 0;
    }

    if(tmp_buffer3 != 0)
    {
        free(tmp_buffer3);
        tmp_buffer3 = 0;
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
        int ret = cpi_close(p_cpi);

        if(CPI_FAILED(ret))
        {
            fprintf(stderr, "Warning: cpi_close failed (0x%.08X)\n", ret);
            return 1;
        }
    }

    return main_ret;
}

static void display_hash(const char *hash_name, uint8_t *hash)
{
    display_binary("", hash_name, hash, 20);

    return;
}

static void display_binary(const char *prefix, const char *name, uint8_t *data, int size)
{
    printf("%s%s[%d] : \n%s", prefix, name, size, prefix);

    int v;
    for(v=0;v<size;v++)
    {
        if(v%32 == 0) { printf("\n%s  ", prefix); }
        printf("%02X", (int)data[v]);
    }

    printf("\n%s\n", prefix);

    return;
}

static void display_bignum(const char *prefix, const char *name, void *bn)
{
    int size = ltc_mp.unsigned_size(bn);

    uint8_t *buff = (uint8_t*)malloc(size*2);

    ltc_mp.unsigned_write(bn, buff);

    display_binary(prefix, name, buff, size);

    free(buff);

    return;
}

