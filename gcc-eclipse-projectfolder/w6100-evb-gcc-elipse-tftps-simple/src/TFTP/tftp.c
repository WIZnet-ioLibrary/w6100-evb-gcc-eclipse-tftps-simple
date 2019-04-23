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
static uint32_t g_tftp_filesize = 512*10;
static uint32_t g_tftp_block_num = 0;

static TFTP_OPTION default_tftp_opt = {
    .name = (uint8_t *)"tsize",
    .value = (uint8_t *)"0"
};

static uint16_t current_opcode;
static uint16_t current_block_num;
static uint16_t current_filesize;

static uint8_t g_file_buf[512*10];
static uint8_t g_is_last_block = 0;

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

    getsockopt(sn, SO_STATUS,&status);
    switch(status)
    {
    case SOCK_UDP:
        switch(g_tftp_state)
        {
        case STATE_NONE:

            ret = send_rrq(buf, filename, sn, server_ip, ip_mode);

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
                    ret = proc_data(buf, sn, ip_mode, destip, destport);

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
                    ret = proc_oack(buf, ret, sn, ip_mode, destip, destport);
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
        socketcreate(sn, TFTP_TEMP_PORT, ip_mode);
        printf("curr state: STATE_NONE\r\n");
        printf("%d:Opened, UDP loopback, port [%d] as %s\r\n", sn, TFTP_TEMP_PORT, get_mode_message(ip_mode));
    }
}

void tftps(uint8_t sn, uint8_t ip_mode)
{
    uint8_t* mode_msg;
    int8_t status;
    datasize_t ret, received_size, remained_bytes;
    uint8_t buf[MAX_MTU_SIZE];// = "hello";
    datasize_t buf_size, i;
    static uint8_t destip[16] = {0,};
    static uint16_t destport;
    uint8_t addr_len;


    getsockopt(sn, SO_STATUS,&status);
    switch(status)
    {
    case SOCK_UDP:
        switch(g_tftp_state)
        {
        case STATE_NONE:
            // Check if there is received data
            getsockopt(sn, SO_RECVBUF, &received_size);
            if(received_size > 0)
            {
                printf("received_size: %d\r\n", received_size);

                if(received_size > MAX_MTU_SIZE) received_size = MAX_MTU_SIZE;
                memset(buf, 0, MAX_MTU_SIZE);
                ret = recvfrom(sn, buf, received_size, (uint8_t*)destip, (uint16_t*)&destport, &addr_len);

                if(ret < 0)
                    return;

                current_opcode = ntohs(*(uint16_t *)buf);
                remained_bytes = ret - 2;

                if(current_opcode == TFTP_RRQ)
                {
                    printf("RRQ received\r\n");
                    proc_rrq(buf, remained_bytes, sn, ip_mode, destip, destport);
                    g_tftp_state = STATE_RRQ;
                }
            }
            break;
        case STATE_RRQ:
            send_oack(buf, sn, ip_mode, destip, destport);

            g_tftp_state = STATE_OACK;

            break;
        case STATE_OACK:
            getsockopt(sn, SO_RECVBUF, &received_size);
            if(received_size > 0)
            {
                printf("received_size: %d\r\n", received_size);

                if(received_size > MAX_MTU_SIZE) received_size = MAX_MTU_SIZE;
                memset(buf, 0, MAX_MTU_SIZE);
                ret = recvfrom(sn, buf, received_size, (uint8_t*)destip, (uint16_t*)&destport, &addr_len);

                if(ret < 0)
                    return;
                current_opcode = ntohs(*(uint16_t *)buf);
                remained_bytes = ret - 2;

                if(current_opcode == TFTP_ACK)
                {
                    send_data(buf, sn, ip_mode, destip, destport);
                }
            }
            break;
        case STATE_DONE:
            g_tftp_state = STATE_NONE;
            g_tftp_block_num = 0;
            g_is_last_block = 0;
            break;
        }
        break;
    case SOCK_CLOSED:
        socketcreate(sn, TFTP_SERVER_PORT, ip_mode);
        printf("%d:Opened, TFTP Simple Server, port [%d] as %s\r\n", sn, TFTP_SERVER_PORT, get_mode_message(ip_mode));
        break;
    default:
        break;
    }
}

uint8_t digittostr(uint32_t intval, uint8_t* strbuf)
{
    uint32_t tmp;
    uint8_t depth;


    if((tmp = (intval / 10)) > 0)
    {
        depth = digittostr(tmp, strbuf);
        depth++;
        *(strbuf + depth) = (intval % 10) + '0';
//        printf("[1]%c\r\n", (intval % 10) + '0');
        return depth;
    }else
    {
        *strbuf = (intval % 10) + '0';
//        printf("[2]%c\r\n", (intval % 10) + '0');
        return 0;
    }
}

void initfilebuf(void)
{
    uint16_t i;

    memset(g_file_buf, 0, g_tftp_filesize);
    for(i=0; i<g_tftp_filesize; i++)
        g_file_buf[i] = (i % 26) + 'a';

}

uint16_t send_rrq(uint8_t * buf, uint8_t* filename, uint8_t sn, uint8_t* server_ip, uint8_t ip_mode)
{
    uint16_t buf_size=0, ret;
    memset(buf, 0, MAX_MTU_SIZE);

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

    ret = packetsend(sn, buf, buf_size, server_ip, TFTP_SERVER_PORT, ip_mode);

    return ret;
}

uint16_t send_oack(uint8_t * buf, uint8_t sn, uint8_t ip_mode, uint8_t* destip, uint16_t destport)
{
    uint16_t buf_size, ret;

    memset(buf, 0, MAX_MTU_SIZE);
    buf_size=0;
    *(uint16_t *)(buf + buf_size) = htons(TFTP_OACK);
    buf_size+= 2;
    strcpy((char *)(buf+buf_size), (const char *)"tsize");
    buf_size += strlen("tsize") + 1;

    digittostr(g_tftp_filesize, buf + buf_size);
    buf_size += strlen((char *)(buf + buf_size)) + 1;

    ret = packetsend(sn,buf,buf_size,destip,destport,ip_mode);

    return ret;
}

uint16_t send_data(uint8_t * buf, uint8_t sn, uint8_t ip_mode, uint8_t* destip, uint16_t destport)
{
    uint16_t buf_size, ret;
    uint16_t tmp_block_num;

    tmp_block_num = ntohs(*(uint16_t *)(buf + 2));
    printf("ACK with block num %d received\r\n", tmp_block_num);

    if(tmp_block_num != 0)
        if(g_is_last_block)
        {
            g_tftp_state = STATE_DONE;
            printf("Data Transmission finished\r\n");

            return 0;
        }
        else
            current_filesize -= 512;

    if(tmp_block_num == g_tftp_block_num)
    {
        memset(buf, 0, MAX_MTU_SIZE);
        buf_size=0;
        *(uint16_t *)(buf + buf_size) = htons(TFTP_DATA);
        buf_size+= 2;
        *(uint16_t *)(buf+buf_size) = htons(++g_tftp_block_num);
        buf_size += 2;

        if(current_filesize >= 512)
        {
            memcpy(buf+buf_size, g_file_buf+((g_tftp_block_num-1)*512), 512);
            buf_size += 512;
        }else
        {
            memcpy(buf+buf_size, g_file_buf+((g_tftp_block_num-1)*512), current_filesize);
            buf_size += current_filesize;
            g_is_last_block = 1;
        }

        ret = packetsend(sn,buf,buf_size,destip,destport,ip_mode);
    }
    return ret;
}

uint16_t proc_rrq(uint8_t * buf, uint16_t buf_size, uint8_t sn, uint8_t ip_mode, uint8_t* destip, uint16_t destport)
{
    current_filesize = g_tftp_filesize;
    //file name
    uint8_t* filename;
    uint8_t* file_type;
    uint8_t* option_start;
    uint16_t index;
    datasize_t nextpos;
    nextpos = 2;
    filename = (uint8_t *)(buf + nextpos);
//                    printf("buf addr: %08X\r\n", buf);
//                    printf("index: %d, filename addr: %08X, filename : %s\r\n", index, filename, (char *)filename);
    nextpos = strlen(filename) + 1;
    buf_size -= nextpos;
    //type
    file_type = filename + nextpos;
//                    printf("index: %d, filetype addr: %08X, filetype: %s\r\n", index, file_type, (char *)file_type);
    nextpos = strlen(file_type) + 1;
    buf_size -= nextpos;
    //option
    option_start = file_type + nextpos;
    nextpos = 0;
    while(buf_size > 0)
    {
        TFTP_OPTION option;
        option.name = option_start;
        printf("remained_bytes: %d, option.name addr: %08X, option.name: %s\r\n", buf_size, option.name, option.name);
        nextpos = (strlen(option.name) + 1);
        buf_size -= nextpos;
        option.value = option.name + nextpos;
        printf("remained_bytes: %d, option.value addr: %08X, option.value: %s\r\n", buf_size, option.value, option.value);
        nextpos = (strlen(option.value) + 1);
        buf_size -= nextpos;
        option_start = option.value + nextpos;
        if(option.name == "tsize")
        {
            ;
        }
    }
}

uint16_t proc_data(uint8_t * buf, uint8_t sn, uint8_t ip_mode, uint8_t* destip, uint16_t destport)
{
    TFTP_DATA_T *data = (TFTP_DATA_T *)buf;
    uint16_t buf_size = 0;

    uint16_t ret;

    current_block_num = ntohs(data->block_num);

    printf("%s", data->data);

    memset(buf, 0, MAX_MTU_SIZE);

    *(uint16_t *)(buf + buf_size) = htons(TFTP_ACK);
    buf_size+= 2;
    *(uint16_t *)(buf + buf_size) = htons(current_block_num);
    buf_size+= 2;

    return packetsend(sn,buf,buf_size,destip,destport,ip_mode);

}

uint16_t proc_oack(uint8_t * buf, uint16_t buf_len, uint8_t sn, uint8_t ip_mode, uint8_t* destip, uint16_t destport)
{
    uint16_t ret, buf_size;

    current_block_num = 0;
    uint8_t* option_startaddr = (uint8_t*)(buf + 2);
    while(option_startaddr < buf + buf_len )
    {
        printf("optoin_startaddr : %0x, buf + ret : %x\r\n", (unsigned int)option_startaddr, (unsigned int)(buf + buf_len));
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

    return packetsend(sn,buf,buf_size,destip,destport,ip_mode);

}

uint16_t packetsend(uint8_t sn, uint8_t* buf, uint16_t buf_size, uint8_t* destip, uint16_t destport, uint8_t ip_mode)
{
    if(ip_mode == AS_IPV4)
        return sendto(sn, buf, buf_size, destip, destport, 4);
    else if(ip_mode == AS_IPV6)
        return sendto(sn, buf, buf_size, destip, destport, 16);
}

uint16_t socketcreate(uint8_t sn, uint16_t src_port, uint8_t ip_mode)
{
    switch(ip_mode)
    {
    case AS_IPV4:
        return socket(sn,Sn_MR_UDP4, TFTP_SERVER_PORT, 0x00);
        break;
    case AS_IPV6:
        return socket(sn,Sn_MR_UDP6, TFTP_SERVER_PORT,0x00);
        break;
    case AS_IPDUAL:
        return socket(sn,Sn_MR_UDPD, TFTP_SERVER_PORT, 0x00);
        break;
    }

    return -1;
}
