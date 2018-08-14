/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * client.c
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define htonll(x) \
((((x) & 0xff00000000000000LL) >> 56) | \
(((x) & 0x00ff000000000000LL) >> 40) | \
(((x) & 0x0000ff0000000000LL) >> 24) | \
(((x) & 0x000000ff00000000LL) >> 8) | \
(((x) & 0x00000000ff000000LL) << 8) | \
(((x) & 0x0000000000ff0000LL) << 24) | \
(((x) & 0x000000000000ff00LL) << 40) | \
(((x) & 0x00000000000000ffLL) << 56))

#define ntohll(x)   htonll(x)

#define MAX_CONN_RETRY  10
#define RETRY_WAIT_TIME 1

#define REQ_CMD_RESET_OVW   11
#define REQ_CMD_RESET_METADATA_CACHE    12
#define REQ_CMD_BLOCK_IO    13
#define REQ_CMD_UNBLOCK_IO  14
#define REQ_CMD_SHAREVOLUME 15
#define DMS_CMD_STOP_VOL 16
#define DMS_CMD_RESTART_VOL                     17
#define DMS_CMD_GET_CN_INFO         18

#define DMS_CMD_INVALIDATE_VOLUME_FREE_CHUNK    19
#define DMS_CMD_CLEANUP_VOLUME_FREE_CHUNK       20
#define DMS_CMD_FLUSH_VOLUME_FREE_CHUNK         21
#define DMS_CMD_PREALLOC                        22

typedef struct cli_prealloc_command {
    uint16_t opcode;
    uint16_t subopcode;
    uint32_t requestID;
    uint64_t volumeID;
    uint64_t chunkID;
} cli_prealloc_cmd_t;

#define REQ_CMD_CN_TEST     101
#define REQ_CMD_COMPARE_DATA    102
#define REQ_CMD_WRITE_DATA  103
#define REQ_CMD_CREATE_VOL  1001

void Compare_Data( char *file1_n, unsigned long long f1_sector_start,
                   char *file2_n, unsigned long long f2_sector_start, unsigned int sect_len )
{
    FILE *f1 = fopen( file1_n, "r" );
    FILE *f2 = fopen( file2_n, "r" );
    unsigned long long bytes1_start = f1_sector_start * 512L;
    unsigned long long bytes2_start = f2_sector_start * 512L;
    char buffer1[512], buffer2[512];
    int i, compare_result = 1;

    if( f1 == NULL || f2 == NULL ) {
        printf( "Cannot open file %s or %s\n", file1_n, file2_n );
        return;
    }

    fseek( f1, bytes1_start, SEEK_SET );
    fseek( f2, bytes2_start, SEEK_SET );

    for( i = 0; i < sect_len; i++ ) {
        fread( buffer1, sizeof( char ), 512, f1 );
        fread( buffer2, sizeof( char ), 512, f2 );

        if( memcmp( buffer1, buffer2, sizeof( char ) * 512 ) != 0 ) {
            printf( "Fail to compare at sector %llu\n", f1_sector_start + i );
            compare_result = 0;
            break;
        } else {
            printf("Compare sector %llu OK\n", (unsigned long long)(f1_sector_start + i));
        }
    }

    if( !compare_result ) {
        printf( "Fail to compare at sector %llu\n", f1_sector_start + sect_len );
    } else {
        printf( "All sectors are the same\n" );
    }

    fclose( f1 );
    fclose( f2 );
}

int Write_Data( char *file1_n, unsigned long long f1_sector_start,
                char *file2_n, unsigned long long f2_sector_start, unsigned int sect_len )
{
    int f1 = -1, f2 = -1;
    unsigned long long bytes1_start = f1_sector_start * 512L;
    unsigned long long bytes2_start = f2_sector_start * 512L;
    char buffer1[512];
    int i, compare_result = 1;
    int num_of_write = 0;

    //f1 = open(file1_n, O_RDWR | O_DIRECT);
    f1 = open( file1_n, O_RDWR );
    //f2 = open(file2_n, O_RDWR | O_DIRECT);
    f2 = open( file2_n, O_RDWR );

    if( f1 == -1 || f2 == -1 ) {
        printf( "Cannot open file %s or %s\n", file1_n, file2_n );
        return 1;
    }

    printf( "leek to position\n" );
    lseek( f1, bytes1_start, SEEK_SET );
    lseek( f2, bytes2_start, SEEK_SET );


    printf( "ready to read\n" );

    for( i = 0; i < sect_len; i++ ) {
        printf( "iteration %d: ", i );
        read( f1, buffer1, 512 );
        num_of_write = write( f2, buffer1, 512 );

        if( num_of_write <= 0 ) {
            if( num_of_write < 0 && errno == EINTR ) {
                printf( "write again\n" );
                num_of_write = write( f2, buffer1, 512 );
            } else {
                printf( "errno %d\n", errno );
            }
        }

        printf( "num write: %d\n", num_of_write );
    }

    printf( "DONE\n" );
    close( f1 );
    close( f2 );
    return 0;
}

static void do_get_cn_info (int sockfd, int argc, char **argv)
{
    short subopcode, num, retcode;
    unsigned long long volid;
    char buffer[1024], *ptr;
    int pktlen, numbytes, i, ip, port, WlatR, WlatAll, RlatR, RlatAll;

    subopcode = (short)atoi(argv[2]);

    if (subopcode > 3 || subopcode < 0) {
        printf("Wrong subopcode %d\n", subopcode);
        return;
    }

    ptr = buffer;
    ptr += sizeof(short);

    *((unsigned short *)ptr) = htons(DMS_CMD_GET_CN_INFO);
    ptr += sizeof(short);
    *((unsigned short *)ptr) = htons(subopcode);
    ptr += sizeof(short);

    pktlen = 4; //opcode, subopcode

    if (subopcode == 2) {

        if (argc < 4) {
            printf("No volume id\n");
            return;
        }
        volid = (unsigned long long)atoll(argv[3]);
        pktlen += 8;
        *((unsigned long long *)ptr) = htonll(volid);
    }

    ptr = buffer;
    *((unsigned short *)ptr) = htons(pktlen);

    printf("get cn info with subopcode %d and pktlen %d\n", subopcode, pktlen);
    write(sockfd, buffer, pktlen + 2);

    numbytes = recv(sockfd, buffer, 8, 0);

    ptr = buffer;
    pktlen = ntohs(*((unsigned short *)ptr));
    printf("RESP pktlen: %d\n", pktlen);
    ptr += sizeof(short);
    printf("RESP opcode: %d\n", ntohs(*((unsigned short *)ptr)));
    ptr += sizeof(short);
    printf("RESP subopcode: %d\n", ntohs(*((unsigned short *)ptr)));
    ptr += sizeof(short);

    retcode = ntohs(*((unsigned short *)ptr));
    printf("RESP ret: %d\n", retcode);
    ptr += sizeof(short);

    if (retcode != 0) {
        return;
    }

    numbytes = recv(sockfd, buffer, pktlen - 6, 0);

    ptr = buffer;
    if (subopcode == 1) {
        printf("RESP: CN DN q len %u\n", ntohl(*((int *)ptr)));
        return;
    }

    num = ntohs(*((short *)ptr));
    ptr += sizeof(short);
    printf("RESP: num of records %d\n", num);

    for (i = 0; i < num; i++) {
        ip = ntohl(*((int *)ptr));
        ptr += sizeof(int);

        if (subopcode == 3) {
            printf("RESP: record[%d] %u\n", i, ip);
            continue;
        }

        port = ntohl(*((int *)ptr));
        ptr += sizeof(int);

        if (subopcode == 0) {
            WlatR = ntohl(*((int *)ptr));
            ptr += sizeof(int);

            WlatAll = ntohl(*((int *)ptr));
            ptr += sizeof(int);

            RlatR = ntohl(*((int *)ptr));
            ptr += sizeof(int);

            RlatAll = ntohl(*((int *)ptr));
            ptr += sizeof(int);
        }

        if (subopcode == 0) {
            printf("RESP: record[%d] %u %u %u %u %u %u\n",
                    i, ip, port, WlatR, WlatAll, RlatR, RlatAll);
        } else {
            printf("RESP: record[%d] %u %u\n", i, ip, port);
        }
    }
}

static void _show_prealloc_cmd (cli_prealloc_cmd_t *cmd)
{
    printf("discoC_adm: show preallocation command\n");
    printf("discoC_adm: "
            "opcode %u subopcode %u requestID %u volumeID %llu chunkID %llu\n",
            cmd->opcode, cmd->subopcode, cmd->requestID,
            (unsigned long long)cmd->volumeID, (unsigned long long)cmd->chunkID);
}

static int32_t _get_prealloc_cmd_pktlen (cli_prealloc_cmd_t *cmd)
{
    int32_t pktlen;

    pktlen = 0;

    switch (cmd->opcode) {
    case DMS_CMD_INVALIDATE_VOLUME_FREE_CHUNK:
    case DMS_CMD_CLEANUP_VOLUME_FREE_CHUNK:
    case DMS_CMD_FLUSH_VOLUME_FREE_CHUNK:
        pktlen = 2 + 4 + 8 + 8; //opcode, reqID, volID, chunkID
        break;
    case DMS_CMD_PREALLOC:
        pktlen = 2 + 2 + 4 + 8; //opcode, subopcode, reqID, volID
        break;
    default:
        break;
    }

    return pktlen;
}

static int32_t _pack_prealloc_cmd_pkt (cli_prealloc_cmd_t *cmd, char *buff)
{
    char *ptr;
    uint64_t volID, chunkID;
    uint32_t reqID;
    uint16_t pktlen, opcode, subopcode, bytes_to_send;

    bytes_to_send = _get_prealloc_cmd_pktlen(cmd);
    if (bytes_to_send == 0) {
        return -1;
    }

    ptr = buff;

    pktlen = htons(bytes_to_send);
    memcpy(ptr, (char *)(&pktlen), sizeof(uint16_t));
    ptr += sizeof(uint16_t);

    opcode = htons(cmd->opcode);
    memcpy(ptr, (char *)(&opcode), sizeof(uint16_t));
    ptr += sizeof(uint16_t);

    if (cmd->opcode == DMS_CMD_PREALLOC) {
        subopcode = htons(cmd->subopcode);
        memcpy(ptr, (char *)(&subopcode), sizeof(uint16_t));
        ptr += sizeof(uint16_t);
    }

    reqID = htonl(cmd->requestID);
    memcpy(ptr, (char *)(&reqID), sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    volID = htonll(cmd->volumeID);
    memcpy(ptr, (char *)(&volID), sizeof(uint64_t));
    ptr += sizeof(uint64_t);

    if (cmd->opcode != DMS_CMD_PREALLOC) {
        chunkID = htonll(cmd->chunkID);
        memcpy(ptr, (char *)(&chunkID), sizeof(uint64_t));
        ptr += sizeof(uint64_t);
    }

    return (bytes_to_send + sizeof(uint16_t));
}

static void _show_prealloc_pkt_content (char pkt_buff[255])
{
    uint64_t *lint;
    uint32_t *nint;
    uint16_t *sint;
    char *ptr;

    ptr = pkt_buff;
    sint = (uint16_t *)ptr;
    ptr += sizeof(uint16_t);

    sint = (uint16_t *)ptr;
    ptr += sizeof(uint16_t);

    nint = (uint32_t *)ptr;
    ptr += sizeof(uint32_t);

    lint = (uint64_t *)ptr;
    ptr += sizeof(uint64_t);

    lint = (uint64_t *)ptr;
    ptr += sizeof(uint64_t);
}

static int32_t _send_prealloc_cmd (int sockfd, cli_prealloc_cmd_t *cmd)
{
    char pkt_buff[255], *ptr;
    int32_t ret, numbytes;
    uint16_t pktlen;

    ret = 0;

    memset(pkt_buff, 0, 255);

    pktlen = _pack_prealloc_cmd_pkt(cmd, pkt_buff);
    if (pktlen <= 0) {
        return -1;
    }
    _show_prealloc_pkt_content(pkt_buff);

    //printf("discoC_adm: _send_prealloc_cmd: packet len %u\n", pktlen);
    numbytes = write(sockfd, pkt_buff, pktlen);
    //printf("discoC_adm: _send_prealloc_cmd: number bytes write %u\n", numbytes);
    if (numbytes != pktlen) {
        printf("discoC_adm: send_prealloc_cmd fail expected: %u sent: %u\n",
                pktlen, numbytes);
    }

    return ret;
}

static int32_t _get_prealloc_resp_pktlen (cli_prealloc_cmd_t *cmd)
{
    int32_t pktlen;

    pktlen = 0;

    switch (cmd->opcode) {
    case DMS_CMD_INVALIDATE_VOLUME_FREE_CHUNK:
        pktlen = 2 + 2 + 4 + 8; //pkt len, ackcode, request ID, Max unuse
        break;
    case DMS_CMD_CLEANUP_VOLUME_FREE_CHUNK:
    case DMS_CMD_FLUSH_VOLUME_FREE_CHUNK:
    case DMS_CMD_PREALLOC:
        pktlen = 2 + 2 + 4; //pkt len, ackcode, request ID
        break;
    default:
        break;
    }

    return pktlen;
}

static int32_t _rcv_prealloc_resp (int sockfd, cli_prealloc_cmd_t *cmd)
{
    char pkt_buff[255], *ptr;
    uint64_t max_hbid;
    uint32_t reqID;
    int32_t numbytes, ret, expected_bytes;
    uint16_t pktlen;
    int16_t ackcode;

    ret = 0;

    expected_bytes = _get_prealloc_resp_pktlen(cmd);
    //printf("discoC_adm: ready to rcv prealloc resp %u bytes\n", expected_bytes);
    numbytes = recv(sockfd, pkt_buff, expected_bytes, 0);

    if (numbytes != expected_bytes) {
        printf("discoC_adm: fail rcv prealloc resp\n");
        return -1;
    }

    ptr = pkt_buff;

    memcpy((char *)&pktlen, ptr, sizeof(uint16_t));
    ptr += sizeof(uint16_t);
    pktlen = ntohs(pktlen);
    //printf("discoC_adm: get preallock resp pkt_len %d\n", pktlen);

    memcpy((char *)&ackcode, ptr, sizeof(int16_t));
    ptr += sizeof(int16_t);
    ackcode = ntohs(ackcode);
    printf("discoC_adm: get preallock resp ackcode %d\n", ackcode);

    memcpy((char *)&reqID, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    reqID = ntohl(reqID);
    printf("discoC_adm: get preallock resp request ID %u\n", reqID);

    switch (cmd->opcode) {
    case DMS_CMD_INVALIDATE_VOLUME_FREE_CHUNK:
        memcpy((char *)&max_hbid, ptr, sizeof(uint64_t));
        ptr += sizeof(uint64_t);
        max_hbid = ntohll(max_hbid);
        printf("discoC_adm: get preallock resp max HBID %llu\n",
                (unsigned long long)max_hbid);
        break;
    case DMS_CMD_CLEANUP_VOLUME_FREE_CHUNK:
    case DMS_CMD_FLUSH_VOLUME_FREE_CHUNK:
    case DMS_CMD_PREALLOC:
        break;
    default:
        break;
    }

    return ret;
}

static void _process_prealloc_cmd (int sockfd, cli_prealloc_cmd_t *cmd)
{
    _show_prealloc_cmd(cmd);

    if (_send_prealloc_cmd(sockfd, cmd) < 0) {
        printf("discoC_adm: send prealloc cmd fail\n");
        return;
    }

    if (_rcv_prealloc_resp(sockfd, cmd)) {
        printf("discoC_adm: receive prealloc cmd fail\n");
    }
}

int main( int argc, char *argv[] )
{
    cli_prealloc_cmd_t prealloc_cmd;
    int sockfd, numbytes;
    struct sockaddr_in address;
    int i;
    short opcode = -1;
    int subopcode = -1;
    long volid = -1;
    short deviceID = -1;
    char *vmid;
    char *buf;
    unsigned long long cap = 0;
    int requestID = 0;
    int conn_retry = 0;
    short share_vol = 0;

    if (argc < 2) {
        printf( "Error format\n" );
        return 0;
    }

    memset((char *)(&prealloc_cmd), 0, sizeof(cli_prealloc_cmd_t));

    opcode = ( short )atoi( argv[1] );

    if( opcode <= 4 ) {
        volid = atol( argv[2] );
        vmid = ( char * )malloc( sizeof( char ) * ( strlen( argv[3] ) + 1 ) );
        memcpy( vmid, argv[3], strlen( argv[3] ) );
        vmid[strlen( argv[3] )] = '\0';
        printf("attach/detach: %d %ld %s\n", opcode, volid, vmid);
    } else if( opcode == REQ_CMD_RESET_OVW || opcode == REQ_CMD_BLOCK_IO ||
               opcode == REQ_CMD_UNBLOCK_IO ) {
        printf( "reset overwritten flag or block IO or unblock IO\n" );
        requestID = atoi( argv[2] );
        volid = atol( argv[3] );
        printf( "reset overwritten flag (opcode=%d) with request id %d and volumd id %ld\n",
                opcode, requestID,  volid );
    } else if( opcode == REQ_CMD_RESET_METADATA_CACHE ) {
        printf( "clear cache\n" );
        requestID = atoi( argv[2] );
        subopcode = atoi( argv[3] );
    } else if( opcode == REQ_CMD_CREATE_VOL ) {
        cap = atol( argv[2] );
        printf("create volume with cap: %llu\n", (unsigned long long)cap);
    } else if( opcode == REQ_CMD_CN_TEST ) {
        printf( "CN test start\n" );

    } else if( opcode == REQ_CMD_COMPARE_DATA ) {
        if( argc < 7 ) {
            printf( "ERROR format for compare data parameter list should be: [opcode=102] [file1] [file1 start] [file2] [file2 start] [compare len]\n" );
            exit( 1 );
        }

        printf( "Compare Data: src:%s dst:%s src sector start:%s dst sector start:%s compare sect len:%s\n",
                argv[2], argv[4], argv[3], argv[5], argv[6] );
        Compare_Data( argv[2], atoll( argv[3] ), argv[4], atoll( argv[5] ),
                      atoi( argv[6] ) );
        return 0;
    } else if( opcode == REQ_CMD_WRITE_DATA ) {
        if( argc < 7 ) {
            printf( "ERROR format for compare data parameter list should be: [opcode=103] [file1] [file1 start] [file2] [file2 start] [write len]\n" );
            exit( 1 );
        }

        printf( "Compare Data: src:%s dst:%s src sector start:%s dst sector start:%s write sect len:%s\n",
                argv[2], argv[4], argv[3], argv[5], argv[6] );
        Write_Data( argv[2], atoll( argv[3] ), argv[4], atoll( argv[5] ),
                    atoi( argv[6] ) );
        return 0;
    } else if( opcode == REQ_CMD_SHAREVOLUME ) {
        requestID = atoi( argv[2] );
        volid = atol( argv[3] );
        share_vol = ( unsigned short )atoi( argv[4] );
    } else if ( opcode == DMS_CMD_STOP_VOL || opcode == DMS_CMD_RESTART_VOL) {
        volid = atol( argv[2] );
    } else if (opcode == DMS_CMD_GET_CN_INFO) {
        //printf("Get CN info test\n");
    } else if (opcode == DMS_CMD_INVALIDATE_VOLUME_FREE_CHUNK) {
        prealloc_cmd.opcode = DMS_CMD_INVALIDATE_VOLUME_FREE_CHUNK;
        prealloc_cmd.requestID = atoi(argv[2]);
        prealloc_cmd.volumeID = atol(argv[3]);
        prealloc_cmd.chunkID = atol(argv[4]);
    } else if (opcode == DMS_CMD_CLEANUP_VOLUME_FREE_CHUNK) {
        prealloc_cmd.opcode = DMS_CMD_CLEANUP_VOLUME_FREE_CHUNK;
        prealloc_cmd.requestID = atoi(argv[2]);
        prealloc_cmd.volumeID = atol(argv[3]);
        prealloc_cmd.chunkID = atol(argv[4]);
    } else if (opcode == DMS_CMD_FLUSH_VOLUME_FREE_CHUNK) {
        prealloc_cmd.opcode = DMS_CMD_FLUSH_VOLUME_FREE_CHUNK;
        prealloc_cmd.requestID = atoi(argv[2]);
        prealloc_cmd.volumeID = atol(argv[3]);
        prealloc_cmd.chunkID = atol(argv[4]);
    } else if (opcode == DMS_CMD_PREALLOC) {
        prealloc_cmd.opcode = DMS_CMD_PREALLOC;
        prealloc_cmd.subopcode = atoi(argv[2]);
        prealloc_cmd.requestID = atoi(argv[3]);
        prealloc_cmd.volumeID = atol(argv[4]);
    } else {
        printf("Error format\n");
        return 1;
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    //Initial, connect to port 2323
    address.sin_family = AF_INET;

    if (opcode == REQ_CMD_CREATE_VOL) {
        address.sin_port = htons(1234);
    } else {
        address.sin_port = htons(9898);
    }

    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    bzero(&(address.sin_zero), 8);

    //Connect to server
    while (connect(sockfd, (struct sockaddr *)&address,
                    sizeof(struct sockaddr)) == -1) {
        sleep(RETRY_WAIT_TIME);
        conn_retry++;

        if (conn_retry == MAX_CONN_RETRY) {
            perror("connect");
            exit(1);
        }
    }

    if (opcode == REQ_CMD_CREATE_VOL) {
        unsigned long long magicNum = htonll(0xa5a5a5a5a5a5a5a5ll);
        short h_opcode = htons(opcode);
        unsigned long long h_cap = htonll( cap );
        write( sockfd, ( char * )( &magicNum ), 8 );
        write( sockfd, ( char * )( &h_opcode ), 2 );
        write( sockfd, ( char * )( &h_cap ), 8 );
    } else if ( opcode == REQ_CMD_RESET_OVW || opcode == REQ_CMD_BLOCK_IO ||
                opcode == REQ_CMD_UNBLOCK_IO ) {
        short num = 14;
        short hnum = htons( num );
        char *array = ( char * )( &hnum );
        buf = malloc( sizeof( char ) * 16 );
        char *pos = buf;
        pos[0] = array[0];
        pos[1] = array[1];
        pos = pos + 2;

        opcode = htons( opcode );
        array = ( char * )( &opcode );
        pos[0] = array[0];
        pos[1] = array[1];
        pos = pos + 2;

        requestID = htonl( requestID );
        memcpy( pos, ( char * )( &requestID ), 4 );
        pos = pos + 4;

        volid = htonll( volid );
        memcpy( pos, ( char * )( &volid ), 8 );
        pos = pos + 8;
        numbytes = write( sockfd, buf, num + 2 );
        printf( "Write %d bytes \n", numbytes );
        free( buf );
        buf = NULL;

        buf = ( char * )malloc( sizeof( char ) * 8 );
        numbytes = recv( sockfd, buf, 8, 0 );
        printf( "client: received %d bytes\n", numbytes );

        char retPktlen[2], retAckCode[2], retReqID[4];
        retPktlen[0] = buf[0];
        retPktlen[1] = buf[1];
        retAckCode[0] = buf[2];
        retAckCode[1] = buf[3];
        retReqID[0] = buf[4];
        retReqID[1] = buf[5];
        retReqID[2] = buf[6];
        retReqID[3] = buf[7];
        int retCode = *( ( int * )buf );
        printf( "pkt length: %d\n", *( ( short * )retPktlen ) );
        printf( "ack code: %d\n", *( ( short * )retAckCode ) );
        printf( "request id: %d\n", *( ( int * )retReqID ) );
        free( buf );
        buf = NULL;
    } else if ( opcode == REQ_CMD_RESET_METADATA_CACHE ) {
        if( subopcode == 0 ) {
            volid = atol( argv[4] );
            int numOfLBID = atol( argv[5] );
            short num = 2 + 4 + 4 + 8 + 4 + 8 *
                        numOfLBID; //opcode, sub opcode, req id, volid, # of LBID, 10*LBID
            short hnum = htons( num );
            unsigned long long arrLBID[100];

            if( argc - 6 != numOfLBID ) {
                printf( "num of LBID is not match the input parameters\n" );
                return 1;
            }

            for( i = 0; i < numOfLBID; i++ ) {
                arrLBID[i] = atol( argv[6 + i] );
                arrLBID[i] = htonll( arrLBID[i] );
            }

            char *array = ( char * )( &hnum );
            buf = malloc( sizeof( char ) * ( num + 2 ) );
            char *pos = buf;
            pos[0] = array[0];
            pos[1] = array[1];
            pos = pos + 2;

            opcode = htons( opcode );
            array = ( char * )( &opcode );
            pos[0] = array[0];
            pos[1] = array[1];
            pos = pos + 2;

            subopcode = htonl( subopcode );
            memcpy( pos, ( char * )( &subopcode ), 4 );
            pos += 4;

            requestID = htonl( requestID );
            memcpy( pos, ( char * )( &requestID ), 4 );
            pos = pos + 4;

            volid = htonll( volid );
            memcpy( pos, ( char * )( &volid ), 8 );
            pos = pos + 8;

            int hnumOfLBID = htonl( numOfLBID );
            memcpy( pos, ( char * )( &hnumOfLBID ), 4 );
            pos += 4;

            memcpy( pos, ( char * )arrLBID, 8 * numOfLBID );
            numbytes = write( sockfd, buf, num + 2 );
            printf( "Write %d bytes \n", numbytes );
            free( buf );
            buf = NULL;

            buf = ( char * )malloc( sizeof( char ) * 8 );
            numbytes = recv( sockfd, buf, 8, 0 );
            printf( "client: received %d bytes\n", numbytes );

            char retPktlen[2], retAckCode[2], retReqID[4];
            retPktlen[0] = buf[0];
            retPktlen[1] = buf[1];
            retAckCode[0] = buf[2];
            retAckCode[1] = buf[3];
            retReqID[0] = buf[4];
            retReqID[1] = buf[5];
            retReqID[2] = buf[6];
            retReqID[3] = buf[7];
            int retCode = *( ( int * )buf );
            printf( "pkt length: %d\n", *( ( short * )retPktlen ) );
            printf( "ack code: %d\n", *( ( short * )retAckCode ) );
            printf( "request id: %d\n", *( ( int * )retReqID ) );
            free( buf );
            buf = NULL;
        } else if( subopcode == 1 ) {
            volid = atol( argv[4] );
            short num = 18;
            short hnum = htons( num );
            printf( "client: pktlen=%d, opcode=%d, subopcode=%d, requestID=%d, volumeID=%ld\n",
                    num, opcode, subopcode, requestID, volid );

            char *array = ( char * )( &hnum );
            buf = malloc( sizeof( char ) * ( num + 2 ) );
            char *pos = buf;
            pos[0] = array[0];
            pos[1] = array[1];
            pos = pos + 2;

            opcode = htons( opcode );
            array = ( char * )( &opcode );
            pos[0] = array[0];
            pos[1] = array[1];
            pos = pos + 2;

            subopcode = htonl( subopcode );
            memcpy( pos, ( char * )( &subopcode ), 4 );
            pos += 4;

            requestID = htonl( requestID );
            memcpy( pos, ( char * )( &requestID ), 4 );
            pos = pos + 4;

            volid = htonll( volid );
            memcpy( pos, ( char * )( &volid ), 8 );
            pos = pos + 8;
            numbytes = write( sockfd, buf, num + 2 );
            printf( "Write %d bytes \n", numbytes );
            free( buf );
            buf = NULL;

            buf = ( char * )malloc( sizeof( char ) * 8 );
            numbytes = recv( sockfd, buf, 8, 0 );
            printf( "client: received %d bytes\n", numbytes );

            char retPktlen[2], retAckCode[2], retReqID[4];
            retPktlen[0] = buf[0];
            retPktlen[1] = buf[1];
            retAckCode[0] = buf[2];
            retAckCode[1] = buf[3];
            retReqID[0] = buf[4];
            retReqID[1] = buf[5];
            retReqID[2] = buf[6];
            retReqID[3] = buf[7];
            int retCode = *( ( int * )buf );
            printf( "pkt length: %d\n", *( ( short * )retPktlen ) );
            printf( "ack code: %d\n", *( ( short * )retAckCode ) );
            printf( "request id: %d\n", *( ( int * )retReqID ) );
            free( buf );
            buf = NULL;
        } else if( subopcode == 2 ) {
            char dnname[] = { "172.106.42.4" };
            short num = 2 + 4 + 4 + 4 + 4 + strlen( dnname ) + 1;
            short hnum = htons( num );
            char *array = ( char * )( &hnum );
            buf = malloc( sizeof( char ) * ( num + 2 ) );
            char *pos = buf;
            pos[0] = array[0];
            pos[1] = array[1];
            pos = pos + 2;

            opcode = htons( opcode );
            array = ( char * )( &opcode );
            pos[0] = array[0];
            pos[1] = array[1];
            pos = pos + 2;

            subopcode = htonl( subopcode );
            memcpy( pos, ( char * )( &subopcode ), 4 );
            pos += 4;

            requestID = htonl( requestID );
            memcpy( pos, ( char * )( &requestID ), 4 );
            pos = pos + 4;

            int port = 20009;
            port = htonl( port );
            memcpy( pos, ( char * )( &port ), 4 );
            pos = pos + 4;

            int namelen = strlen( dnname ) + 1;
            namelen = htonl( namelen );
            memcpy( pos, ( char * )( &namelen ), 4 );
            pos = pos + 4;

            memcpy( pos, dnname, strlen( dnname ) );
            char *nn = pos;
            pos = pos + strlen( dnname );
            pos = '\0';
            printf( "debug: %s\n", buf + 20 );
            numbytes = write( sockfd, buf, num + 2 );
            printf( "Write %d bytes \n", numbytes );
            free( buf );
            buf = NULL;

            buf = ( char * )malloc( sizeof( char ) * 8 );
            numbytes = recv( sockfd, buf, 8, 0 );
            printf( "client: received %d bytes\n", numbytes );

            char retPktlen[2], retAckCode[2], retReqID[4];
            retPktlen[0] = buf[0];
            retPktlen[1] = buf[1];
            retAckCode[0] = buf[2];
            retAckCode[1] = buf[3];
            retReqID[0] = buf[4];
            retReqID[1] = buf[5];
            retReqID[2] = buf[6];
            retReqID[3] = buf[7];
            int retCode = *( ( int * )buf );
            printf( "pkt length: %d\n", *( ( short * )retPktlen ) );
            printf( "ack code: %d\n", *( ( short * )retAckCode ) );
            printf( "request id: %d\n", *( ( int * )retReqID ) );
            free( buf );
            buf = NULL;
        } else if( subopcode == 3 ) {
            int numOfHBID = atol( argv[4] );
            short num = 2 + 4 + 4 + 4 + 8*numOfHBID; //opcode, sub opcode, req id, # of HBID, 10*LBID
            short hnum = htons(num);
            unsigned long long arrHBID[100];

            if (argc - 5 != numOfHBID) {
                printf("num of HBID is not match the input parameters argc %d # of hbid\n", argc);
                return 1;
            }

            printf("num of HBIDs %d:", numOfHBID);
            for( i = 0; i < numOfHBID; i++ ) {
                arrHBID[i] = atol(argv[5 + i]);
                printf(" %llu", arrHBID[i]);
                arrHBID[i] = htonll(arrHBID[i]);
            }
            printf("\n");

            printf("total pkt size %d\n", num);
            char *array = (char *)(&hnum);
            buf = malloc(sizeof(char)*(num + 2));
            char *pos = buf;
            pos[0] = array[0];
            pos[1] = array[1];
            pos = pos + 2;

            opcode = htons( opcode );
            array = ( char * )( &opcode );
            pos[0] = array[0];
            pos[1] = array[1];
            pos = pos + 2;

            subopcode = htonl( subopcode );
            memcpy( pos, ( char * )( &subopcode ), 4 );
            pos += 4;

            requestID = htonl( requestID );
            memcpy( pos, ( char * )( &requestID ), 4 );
            pos = pos + 4;

            int hnumOfHBID = htonl( numOfHBID );
            memcpy( pos, ( char * )( &hnumOfHBID ), 4 );
            pos += 4;

            memcpy(pos, (char *)arrHBID, 8 * numOfHBID);
            numbytes = write( sockfd, buf, num + 2 );
            printf( "Write %d bytes \n", numbytes );
            free( buf );
            buf = NULL;

            buf = ( char * )malloc( sizeof( char ) * 8 );
            numbytes = recv( sockfd, buf, 8, 0 );
            printf( "client: received %d bytes\n", numbytes );

            char retPktlen[2], retAckCode[2], retReqID[4];
            retPktlen[0] = buf[0];
            retPktlen[1] = buf[1];
            retAckCode[0] = buf[2];
            retAckCode[1] = buf[3];
            retReqID[0] = buf[4];
            retReqID[1] = buf[5];
            retReqID[2] = buf[6];
            retReqID[3] = buf[7];
            int retCode = *( ( int * )buf );
            printf( "pkt length: %d\n", *( ( short * )retPktlen ) );
            printf( "ack code: %d\n", *( ( short * )retAckCode ) );
            printf( "request id: %d\n", *( ( int * )retReqID ) );
            free( buf );
            buf = NULL;
        }
    } else if( opcode == REQ_CMD_SHAREVOLUME ) {
        short num = 16;
        short hnum = htons( num );
        char *array = ( char * )( &hnum );
        buf = malloc( sizeof( char ) * ( num + 2 ) );
        char *pos = buf;
        pos[0] = array[0];
        pos[1] = array[1];
        pos = pos + 2;

        opcode = htons( opcode );
        array = ( char * )( &opcode );
        pos[0] = array[0];
        pos[1] = array[1];
        pos = pos + 2;

        requestID = htonl( requestID );
        memcpy( pos, ( char * )( &requestID ), 4 );
        pos = pos + 4;

        volid = htonll( volid );
        memcpy( pos, ( char * )( &volid ), 8 );
        pos = pos + 8;

        share_vol = htons( share_vol );
        memcpy( pos, ( char * )( &share_vol ), 2 );
        pos = pos + 2;
        numbytes = write( sockfd, buf, num + 2 );
        free( buf );
        buf = NULL;
    } else if( opcode == DMS_CMD_STOP_VOL  || opcode == DMS_CMD_RESTART_VOL) {
        short num = 10;
        short hnum = htons( num );
        char *array = ( char * )( &hnum );
        buf = malloc( sizeof( char ) * ( num + 2 ) );
        char *pos = buf;
        pos[0] = array[0];
        pos[1] = array[1];
        pos = pos + 2;

        opcode = htons( opcode );
        array = ( char * )( &opcode );
        pos[0] = array[0];
        pos[1] = array[1];
        pos = pos + 2;

        volid = htonll( volid );
        memcpy( pos, ( char * )( &volid ), 8 );
        pos = pos + 8;

        numbytes = write( sockfd, buf, num + 2 );
        free( buf );
        buf = NULL;
    } else if (opcode == REQ_CMD_CN_TEST) {
        buf = malloc( sizeof( char ) * 64 );
        char *pos = buf;
        short num = 2 + 4 + 8 + 4 + 2 +
                    2; //opcode, volid, sect start, sect len, rw, data
        short num1 = htons( num );
        opcode = htons( opcode );

        char *array = ( char * )( &num1 ); //pkt length
        pos[0] = array[0];
        pos[1] = array[1];

        array = ( char * )( &opcode ); //opcode
        pos[2] = array[0];
        pos[3] = array[1];
        //write(sockfd, buf, num+2);

        unsigned int test_volid = atoi( argv[2] );
        unsigned long long test_sect_start = atol( argv[3] );
        unsigned long long test_sect_len = atoi( argv[4] );
        short test_rw = ( short )atoi( argv[5] );
        short test_data = ( short )atoi( argv[6] );

        pos = pos + 4;
        printf("Issue command : volid %d sect start %llu sect len %llu rw %d data 0x%02x\n",
                test_volid,
                (unsigned long long)test_sect_start,
                (unsigned long long)test_sect_len, test_rw, test_data);

        test_volid = htonl( test_volid );
        memcpy( pos, ( char * )( &test_volid ), 4 );
        pos = pos + 4;

        test_sect_start = htonll( test_sect_start );
        memcpy( pos, ( char * )( &test_sect_start ), 8 );
        pos = pos + 8;

        test_sect_len = htonl( test_sect_len );
        memcpy( pos, ( char * )( &test_sect_len ), 4 );
        pos = pos + 4;

        test_rw = htons( test_rw );
        memcpy( pos, ( char * )( &test_rw ), 2 );
        pos = pos + 2;

        test_data = htons( test_data );
        memcpy( pos, ( char * )( &test_data ), 2 );
        pos = pos + 2;
        numbytes = write( sockfd, buf, num + 2 );

        free( buf );

        buf = ( char * )malloc( sizeof( char ) * 64 );
        numbytes = recv( sockfd, buf, 64, 0 );
        printf( "client: received %d bytes\n", numbytes );
        int retCode = *( ( int * )buf );
        printf( "retCode is %d\n", ntohl( retCode ) );
        free( buf );
        buf = NULL;
    } else if (opcode == DMS_CMD_GET_CN_INFO) {
        printf("Ready to get cn info\n");
        do_get_cn_info(sockfd, argc, argv);
    } else if (opcode == DMS_CMD_INVALIDATE_VOLUME_FREE_CHUNK) {
        printf("discoC_adm: inval volume free chunk\n");
        _process_prealloc_cmd(sockfd, &prealloc_cmd);
    } else if (opcode == DMS_CMD_CLEANUP_VOLUME_FREE_CHUNK) {
        printf("discoC_adm: cleanup volume free chunk\n");
        _process_prealloc_cmd(sockfd, &prealloc_cmd);
    } else if (opcode == DMS_CMD_FLUSH_VOLUME_FREE_CHUNK) {
        printf("discoC_adm: flush volume free chunk\n");
        _process_prealloc_cmd(sockfd, &prealloc_cmd);
    } else if (opcode == DMS_CMD_PREALLOC) {
        printf("discoC_adm: prealloc command\n");
        _process_prealloc_cmd(sockfd, &prealloc_cmd);
    } else {
        buf = malloc( sizeof( char ) * 64 );
        char *pos = buf;
        short num = 2 + 8 + strlen( vmid ) + 1;
        short num1 = htons( num );
        char *array = ( char * )( &num1 );
        pos[0] = array[0];
        pos[1] = array[1];
        pos = pos + 2;

        opcode = htons( opcode );
        array = ( char * )( &opcode );
        pos[0] = array[0];
        pos[1] = array[1];
        pos = pos + 2;

        volid = htonll( volid );
        memcpy( pos, ( char * )( &volid ), 8 );
        pos = pos + 8;

        memcpy( pos, vmid, strlen( vmid ) );
        pos = pos + strlen( vmid );

        *pos = '\0';
        printf( "Pktlen : %d\n", num );
        numbytes = write( sockfd, buf, num + 2 );

        printf( "Write %d bytes \n", numbytes );
        free( buf );
        buf = NULL;

        buf = ( char * )malloc( sizeof( char ) * 64 );
        numbytes = recv( sockfd, buf, 64, 0 );
        printf( "client: received %d bytes\n", numbytes );
        int retCode = *( ( int * )buf );
        printf( "retCode is %d\n", ntohl( retCode ) );
        printf( "Device file : %s\n", ( buf + 4 ) );
        free( vmid );
        free( buf );
        buf = NULL;
    }

    close( sockfd );
    return 0;
}

