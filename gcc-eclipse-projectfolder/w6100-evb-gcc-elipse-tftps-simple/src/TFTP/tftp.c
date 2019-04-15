/**
 * @file tftp.c
 * @brief TFTP Source File.
 * @version 0.1.0
 * @author Sang-sik Kim
 */

/* Includes -----------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include "tftp.h"
#include "socket.h"
#include "netutil.h"

/* define -------------------------------------------------------*/

/* typedef ------------------------------------------------------*/

/* Extern Variable ----------------------------------------------*/

/* Extern Functions ---------------------------------------------*/
#ifdef F_STORAGE
extern void save_data(uint8_t *data, uint32_t data_len, uint16_t block_number);
#endif

/* Global Variable ----------------------------------------------*/
//static uint16_t g_local_port = 0;

static uint32_t g_tftp_state = STATE_NONE;
//static uint16_t g_block_num = 0;


static TFTP_OPTION default_tftp_opt = {
    .name = (uint8_t *)"tsize",
    .value = (uint8_t *)"0"
};

static uint16_t current_opcode;
static uint16_t current_block_num;

uint8_t g_progress_state = TFTP_PROGRESS;

#ifdef __TFTP_DEBUG__
int dbg_level = (INFO_DBG | ERROR_DBG | IPC_DBG);
#endif


/* static function define ---------------------------------------*/

void tftpc(uint8_t sn, uint8_t *server_ip, uint8_t *filename, uint8_t ip_mode)
{
    uint8_t* mode_msg;
    int8_t status;
    datasize_t ret, received_size;
    uint8_t buf[MAX_MTU_SIZE];// = "hello";
    datasize_t buf_size, i;
    static uint8_t destip[16] = {0,};
    static uint16_t destport;
    uint8_t addr_len;

    if(ip_mode == AS_IPV4)
    {
        mode_msg = (uint8_t *)mode_v4;
    }else if(ip_mode == AS_IPV6)
    {
        mode_msg = (uint8_t *)mode_v6;
    }else
    {
        mode_msg = (uint8_t *)mode_dual;
    }

    getsockopt(sn, SO_STATUS,&status);
    switch(status)
    {
    case SOCK_UDP:
        switch(g_tftp_state)
        {
        case STATE_NONE:
            buf_size = strlen(buf);
            memset(buf, 0, MAX_MTU_SIZE);
            buf_size=0;
            *(uint16_t *)(buf + buf_size) = htons(TFTP_RRQ);
            buf_size+= 2;
            strcpy((char *)(buf+buf_size), (const char *)filename);
            buf_size += strlen((char *)filename) + 1;
            strcpy((char *)(buf+buf_size), (const char *)TRANS_BINARY);
            buf_size += strlen((char *)TRANS_BINARY) + 1;
            strcpy((char *)(buf+buf_size), (const char *)default_tftp_opt.name);
            buf_size += strlen((char *)default_tftp_opt.name) + 1;
            strcpy((char *)(buf+buf_size), (const char *)default_tftp_opt.value);
            buf_size += strlen((char *)default_tftp_opt.value) + 1;
            printf("curr state: STATE_NONE\r\n");
            if(ip_mode == AS_IPV4)
                ret = sendto(sn, buf, buf_size, server_ip, TFTP_SERVER_PORT, 4);
            else if(ip_mode == AS_IPV6)
                ret = sendto(sn, buf, buf_size, server_ip, TFTP_SERVER_PORT, 16);

            if(ret > 0){
                printf("curr state: STATE_RRQ\r\n");
                g_tftp_state = STATE_RRQ;
            }

            break;
        case STATE_RRQ:
            getsockopt(sn, SO_RECVBUF, &received_size);
            if(received_size > 0)
            {
                if(received_size > MAX_MTU_SIZE) received_size = MAX_MTU_SIZE;
                memset(buf, 0, MAX_MTU_SIZE);
                ret = recvfrom(sn, buf, received_size, (uint8_t*)destip, (uint16_t*)&destport, &addr_len);

                if(ret < 0)
                    return;

                current_opcode = ntohs(*(uint16_t *)buf);

                printf("opcode : %d\r\n", current_opcode);

                if(current_opcode == TFTP_DATA)
                {

                    TFTP_DATA_T *data = (TFTP_DATA_T *)buf;

                    current_block_num = ntohs(data->block_num);

                    printf("%s", data->data);

                    memset(buf, 0, MAX_MTU_SIZE);
                    buf_size=0;
                    *(uint16_t *)(buf + buf_size) = htons(TFTP_ACK);
                    buf_size+= 2;
                    *(uint16_t *)(buf + buf_size) = htons(current_block_num);
                    buf_size+= 2;
                    if(ip_mode == AS_IPV4)
                        ret = sendto(sn, buf, buf_size, destip, destport, 4);
                    else if(ip_mode == AS_IPV6)
                        ret = sendto(sn, buf, buf_size, destip, destport, 16);

                    if(ret > 0)
                    {
                        if(received_size - 4 < TFTP_BLK_SIZE){
                            g_tftp_state = STATE_DONE;
                            printf("curr state: STATE_DONE\r\n");
                        }
                    }

                }else if(current_opcode == TFTP_ERROR)
                {
                    TFTP_ERROR_T *data = (TFTP_ERROR_T *)buf;
                    printf("Error occurred with %s\r\n", data->error_msg);
                    g_tftp_state = STATE_DONE;
                    printf("curr state: STATE_DONE\r\n");
                }else if(current_opcode == TFTP_OACK)
                {
                    current_block_num = 0;
                    uint8_t* option_startaddr = (uint8_t*)(buf + 2);
                    while(option_startaddr < buf + ret )
                    {
                        printf("optoin_startaddr : %0x, buf + ret : %x\r\n", (unsigned int)option_startaddr, (unsigned int)(buf + ret));
                        TFTP_OPTION option;
                        option.name = option_startaddr;
                        option_startaddr += (strlen(option.name) + 1);
                        option.value = option_startaddr;
                        option_startaddr += (strlen(option.value) + 1);
                        printf("option.name: %s, option.value: %s\r\n", option.name, option.value);
					}

                    memset(buf, 0, MAX_MTU_SIZE);
                    buf_size=0;
                    *(uint16_t *)(buf + buf_size) = htons(TFTP_ACK);
                    buf_size+= 2;
                    *(uint16_t *)(buf + buf_size) = htons(current_block_num);
                    buf_size+= 2;

                    if(ip_mode == AS_IPV4)
                        ret = sendto(sn, buf, buf_size, destip, destport, 4);
                    else if(ip_mode == AS_IPV6)
                        ret = sendto(sn, buf, buf_size, destip, destport, 16);
                }
            }
            break;
        case STATE_DONE:
            break;
        default:
            ;
        }
        break;
    case SOCK_CLOSED:
        switch(ip_mode)
        {
        case AS_IPV4:
            socket(sn,Sn_MR_UDP4, TFTP_TEMP_PORT, 0x00);
            break;
        case AS_IPV6:
            socket(sn,Sn_MR_UDP6, TFTP_TEMP_PORT,0x00);
            break;
        case AS_IPDUAL:
            socket(sn,Sn_MR_UDPD, TFTP_TEMP_PORT, 0x00);
            break;
        }
        printf("%d:Opened, UDP loopback, port [%d] as %s\r\n", sn, TFTP_TEMP_PORT, mode_msg);
    }
}

void tftps(uint8_t sn, uint8_t ip_mode)
{

}
