/*
 * Copyright (C) 2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * ipcsocket.c
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <net/if.h>
#include <string.h>
#include <pthread.h>
#include <syslog.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include "../common/discoC_sys_def.h"
#include "../common/common.h"
#include "ipcsocket_private.h"
#include "ipcsocket.h"
#include "mclient.h"
#include "user_command.h"

void *malloc_wrap(size_t size);

static int32_t _chk_nn_host (struct hostent *host)
{
    int32_t retry;

    syslog(LOG_ERR, "DMSC INFO conn to namodenode: %s:%d\n",
            g_namenode_address, g_nn_VO_port);
    if (host == NULL) {
        host = (struct hostent *)gethostbyname(g_namenode_address);

        while (host == NULL){
            retry++;
            if (retry > MAX_GETHOSTNAME_RETRY_COUNT) {
                syslog(LOG_ERR,
                        "DMSC ERROR gethostbyname %s fail exceed max retry time (5)\n",
                        g_namenode_address);
                return INVALID_SOCK_FD;
            }
            host = gethostbyname(g_namenode_address);
            sleep(1);
        }

        memcpy(&gdef_nnSrvIPAddr, g_nn_host->h_addr, g_nn_host->h_length);
    }

    return 0;
}

static int create_nn_connection (uint32_t nnIPAddr)
{
    int ns, i;
    struct sockaddr_in sin;
    int retry, ret;
    struct timeval timeout = {60, 0};

    for (i = 0; i < MAX_GETHOSTNAME_RETRY_COUNT; i++) {
        ns = socket(AF_INET, SOCK_STREAM, 0);
        if(ns > INVALID_SOCK_FD) {
            break;
        }
	}

    if (ns == INVALID_SOCK_FD) {
        return INVALID_SOCK_FD;
    }

    retry = 0;
    syslog(LOG_ERR, "DMSC INFO conn to namodenode: %s:%d\n",
            g_namenode_address, g_nn_VO_port);

    ret = setsockopt(ns, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    if (ret < 0) {
        syslog(LOG_ERR, "DMSC¡@setsockopt nn conn fail\n");
    } else {
        memcpy(&sin.sin_addr, &nnIPAddr, sizeof(uint32_t));
        sin.sin_family = AF_INET;
        sin.sin_port = htons(g_nn_VO_port);
        memset(&sin.sin_zero, 0, sizeof(sin.sin_zero));

        syslog(LOG_ERR, "DMSC INFO connectting namenode %s:%d\n",
                g_namenode_address, g_nn_VO_port);
        if (connect(ns, (struct sockaddr *)&sin,
                sizeof(struct sockaddr_in)) < 0) {
            //2016-05-03 Lego fix coverity issue CID: 37651
            shutdown(ns, SHUT_RDWR);
            close(ns);
            return INVALID_SOCK_FD;
        }
    }

	return ns;
}

void check_namenode_connection (struct hostent *host)
{
    int error;
    int len;
    int ns;
    unsigned int cnt;

    if (_chk_nn_host(host)) {
        return;
    }

    ns = create_nn_connection(gdef_nnSrvIPAddr);

    cnt = 0;
    while (ns == -1) {
        cnt++;
        if (cnt % 60 == 0) {
            syslog(LOG_ERR, "DMSC INFO C Daemon cannot create sock for 60 times, "
                    "try again\n");
        }
        sleep(1);
        ns = create_nn_connection(gdef_nnSrvIPAddr);
    }

    len = 0;
    error = 0;
    syslog(LOG_ERR, "DMSC INFO C Daemon is checking connection with namenode\n");
    while (getsockopt(ns, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
        if (ns != -1) {
            //2016-05-03 Lego fix coverity issue CID: 31902
            shutdown(ns, SHUT_RDWR);
            close(ns);
        }
        ns = create_nn_connection(gdef_nnSrvIPAddr);
        syslog(LOG_ERR,"DMSC INFO try to re-connect.....\n");
        sleep(1);
    }

    //2016-05-03 Lego fix coverity issue CID: 31902
    shutdown(ns, SHUT_RDWR);
    close(ns);
    syslog(LOG_ERR, "DMSC INFO C Daemon connect to namenode ok!\n");
}

static int socket_rcv (int sk_fd, char *rx_buf, int rx_len)
{
    char *recv_ptr;
    int local_rx_len, res;

    recv_ptr = rx_buf;
    local_rx_len = 0;
    res = 0;

    while (local_rx_len < rx_len) {
        res = recv(sk_fd, recv_ptr, rx_len - local_rx_len, MSG_WAITALL);
        if (res < 0) {
            if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
                syslog(LOG_ERR, "DMSC INFO recv data ERROR: errno %d\n", errno);
                break;
            }
        } else if (res == 0) {
            syslog(LOG_ERR, "socket_recvfrom receive 0 bytes and errno %d\n",
                    errno);
            return 0;
        }

        local_rx_len = local_rx_len + res;
        recv_ptr = recv_ptr + res;
    }

    return ((rx_len == local_rx_len) ? 0 : -1);
}

static int socket_send (int sk_fd, char *tx_buf, int tx_len)
{
    char *send_ptr;
    int local_tx_len, res, ret;

    send_ptr = tx_buf;
    local_tx_len = 0;
    res = 0;
    ret = 0;
    while (local_tx_len < tx_len) {
        res = write(sk_fd, send_ptr, tx_len - local_tx_len);

        if (res < 0) {
            if(errno != EINTR) {
                syslog(LOG_ERR, "DMSC INFO socket send ERROR: errno %d\n", errno);
                break;
            }
        }

        local_tx_len += res;
        send_ptr = send_ptr + res;
    }

    return (local_tx_len == tx_len ? 0 : -1);
}

static int send_response (int sockfd, int retCode)
{
    int response_for_client;
    int ret;

    response_for_client = htonl(retCode);
    return socket_send(sockfd, (char*)(&response_for_client), 4);
}

static int conn_to_nn (int32_t nnIPAddr)
{
    int i, sock_fd, retCode, error, len;

    for (i = 0; i < MAX_CONNECTION_RETRY_COUNT; i++) {
        sock_fd = create_nn_connection(nnIPAddr);
        if (sock_fd > INVALID_SOCK_FD) {
            break;
        }
    }

    if (sock_fd == INVALID_SOCK_FD) {
        syslog(LOG_ERR, "DMSC WARN create nn conn fail\n");
        return INVALID_SOCK_FD;
    }

#if 0 //comment this code when using Ubuntu
    retCode = getsockopt(sock_fd, SOL_SOCKET, SO_ERROR, &error, &len);
    if (retCode < 0) {
        close(sock_fd);
        shutdown(sock_fd, SHUT_RDWR);
        syslog(LOG_ERR, "DMSC WARN set socket opt fail error %d\n", error);
        return INVALID_SOCK_FD;
    }
#endif

    return sock_fd;
}

static void gen_nn_req_packet(int vol_id, short opcode, char *buf)
{
    long long magic_number;
    int volns;
    char *ptr;

    magic_number = htonll(MAGIC_NUM);
    opcode = htons(opcode);
    volns = htonl(vol_id);

    memset(buf, 0, sizeof(char)*NN_REQ_PACKET_SIZE);

    ptr = &(buf[0]);
    memcpy(ptr, &(magic_number), sizeof(long long));
    ptr = ptr + sizeof(long long);
    memcpy(ptr, &(opcode), sizeof(short));
    ptr = ptr + sizeof(short);
    memcpy(ptr, &(volns), sizeof(int));
}

/*
 * return code: -1 : fail to establish connection with namenode
 * return code: -2 : fail to send request to namenode
 * return code:  0  : OK
 *
 * TODO: should contains nn ip
 */
static int32_t send_detach_to_nn (uint32_t nnIPAddr, uint32_t vol_id)
{
    int32_t error, retCode = 0, i, sock_fd, len;
    int8_t buf[NN_REQ_PACKET_SIZE];
    int8_t *ptr = NULL;

    if (g_nnSimulator_on) {
        return 0;
    }

    if((sock_fd = conn_to_nn(nnIPAddr)) == INVALID_SOCK_FD) {
        return -1;
    }

    gen_nn_req_packet(vol_id, REQ_DETCH_VOLUME, buf);

    if(socket_send(sock_fd, buf, NN_REQ_PACKET_SIZE)) {
        syslog(LOG_ERR, "DMSC WARN send detach to nn fail\n");
        retCode = -2;
    } else {
        retCode = 0;
    }

    //2016-05-03 Lego fix coverity issue CID: 37658
    shutdown(sock_fd, SHUT_RDWR);
    close(sock_fd);
    return retCode;
}

static void parse_vol_info_resp (struct dms_volume_info *v_info, char *content)
{
    char *ptr;
    long long capacity;

    ptr = content;
    memcpy(&capacity, ptr, sizeof(long long));
    capacity = ntohll(capacity);
    if (capacity == -1) {
        v_info->capacity = 0;
    } else {
        v_info->capacity = capacity;
    }
    ptr = ptr + sizeof(long long);

    memcpy(&(v_info->replica_factor), ptr, sizeof(int));
    v_info->replica_factor = ntohl(v_info->replica_factor);
    ptr = ptr + sizeof(int);

    memcpy(&(v_info->share_volume), ptr, sizeof(short));
    v_info->share_volume = ntohs(v_info->share_volume);
}

static void get_vol_info (struct dms_volume_info *v_info)
{
    int sock_fd, retCode, i;
    char tx_buf[NN_REQ_PACKET_SIZE], rx_buf[255];

    do {
        sock_fd = INVALID_SOCK_FD;
        for (i = 0; i < 30; i++) {
            //TODO: v_info has nn ip
            if((sock_fd = conn_to_nn(v_info->nnSrv_IPV4addr)) == INVALID_SOCK_FD) {
                syslog(LOG_ERR, "DMSC WARN fail connect to nn ... retry %d\n", i);
                sleep(1);
                continue;
            }
            break;
        }

        if (sock_fd == INVALID_SOCK_FD) {
            syslog(LOG_ERR,
                    "DMSC WARN fail connect to nn after retry 30 secs\n");
            v_info->capacity = 0;
            break;
        }

	    syslog(LOG_ERR, "DMSC INFO get volume info volume id = %d "
	            "and socket fd = %d\n", v_info->volumeID, sock_fd);

	    gen_nn_req_packet(v_info->volumeID, REQ_REQUEST_FOR_VOLUME_INFO, tx_buf);
	    if(socket_send(sock_fd, tx_buf, NN_REQ_PACKET_SIZE)) {
	        syslog(LOG_ERR, "DMSC WARN send query info to nn fail vol id %d\n",
	            v_info->volumeID);
	        //2016-05-03 Lego fix coverity issue CID: 37653
	        shutdown(sock_fd, SHUT_RDWR);
	        close(sock_fd);
	        v_info->capacity = 0;
	        break;
	    }
  
	    memset(rx_buf, 0, sizeof(char)*255);
	    retCode = socket_rcv(sock_fd, rx_buf, NN_VOL_INFO_RESP_SIZE);

	    if (retCode == 0) {
	        parse_vol_info_resp(v_info, rx_buf);
	        syslog(LOG_ERR, "DMSC get volume info: volid=%d cap=%llu "
	                "replica_factor=%d share_volume=%d\n",
	                v_info->volumeID, (long long unsigned int)v_info->capacity,
	                v_info->replica_factor,
	                v_info->share_volume);
	    } else {
	        v_info->capacity = 0;
	    }

	    //2016-05-03 Lego fix coverity issue CID: 37653
	    shutdown(sock_fd, SHUT_RDWR);
        close(sock_fd);
    } while (0);
}

static void perform_volume_setting (int8_t *nnSrvNM, uint32_t vol_id)
{
	int8_t cmd[128];

	sprintf(cmd, "blockdev --setra %d %s%s%d", disk_read_ahead,
	        DEV_FULL_PATH_PREFIX, nnSrvNM, vol_id);
	system(cmd);
	syslog(LOG_ERR, "DMSC INFO set volume cmd: %s\n", cmd);

	sprintf(cmd, "echo noop > /sys/block/%s%d/queue/scheduler", nnSrvNM, vol_id);
	system(cmd);
	syslog(LOG_ERR, "DMSC INFO set volume cmd: %s\n", cmd);
}

static int is_invalid_vol_cap (long long cap)
{
    if (cap <= 0 || cap > (64L*1024L*1024L*1024L*1024L)) {
        return 1;
    }

    return 0;
}

static int is_dev_file_exist (int8_t *nnSrvNM, uint32_t vol_id)
{
    char dev_fn[128];
    FILE *dev_fp;
    int wait_dev_cnt;

    //check device file
    sprintf(dev_fn, "%s%s%d", DEV_FULL_PATH_PREFIX, nnSrvNM, vol_id);
    syslog(LOG_ERR, "DMSC INFO check device file: %s\n", dev_fn);
    wait_dev_cnt = 0;
    while (wait_dev_cnt < MAX_WAIT_TIME_DEV_FILE) {
        dev_fp = fopen(dev_fn, "r");
        if (dev_fp) {
            syslog(LOG_ERR, "DMSC INFO Device files %s exist, close file and "
                    "leave loop\n", dev_fn);
            fclose(dev_fp);
            return 1;
        } else {
            syslog(LOG_ERR,"DMSC INFO Device files %s not exist, wait %d\n",
                    dev_fn, wait_dev_cnt);
        }
        wait_dev_cnt++;
        sleep(1);
    }

    return 0;
}

/*
 * NOTE Will device appear when we commit error during read partition table?
 * Ans: case 1: if exist partition table on this volume
 *              device file won't appear
 *      case 2: if non-exist partition table on this volume
 *              device will appear
 */
static void force_detach_vol (int8_t *nnName, uint32_t nnIPAddr, int32_t vol_id)
{
    /*
     * step 1: set volume IOCTL_DISABLE_VOL_RW
     * step 2: wait volume device file appear
     *         The better way might be chk whether all io of this volume
     *         finish
     * step 3: issue detach to driver
     */
    struct dms_volume_info vinfo;
    int32_t ret, i, max_cnt;

    ret = ioctl(dms_chdev_fd, IOCTL_DISABLE_VOL_RW, (unsigned long)vol_id);

    max_cnt = 2;
    for (i = 0; i < max_cnt; i++) {
        if (is_dev_file_exist(nnName, vol_id)) {
            break;
        }
    }

    vinfo.nnSrv_IPV4addr = nnIPAddr;
    strcpy(vinfo.nnSrv_nm, nnName);
    vinfo.volumeID = vol_id;
    vinfo.share_volume = 0;
    vinfo.capacity = 0;
    ret = ioctl(dms_chdev_fd, IOCTL_DETATCH_VOL, &vinfo);
}

static attach_response_t attach_volume (int8_t *nnName, uint32_t nnIPAddr,
        int32_t vol_id, int8_t *instid)
{
    struct dms_volume_info vinfo;
    int ret, ret2, i, do_roll_back;

    //TODO: get default name and IP
    vinfo.nnSrv_IPV4addr = nnIPAddr;
    strcpy(vinfo.nnSrv_nm, nnName);
    vinfo.volumeID = vol_id;
    vinfo.share_volume = 0;
    do_roll_back = 0;

    if (clientnode_running_mode != discoC_MODE_SelfTest &&
            clientnode_running_mode != discoC_MODE_SelfTest_NN_Only &&
            g_nnSimulator_on == false) {
        get_vol_info(&vinfo);
    } else {
        vinfo.nnSrv_IPV4addr = gdef_nnSrvIPAddr;
        vinfo.volumeID = vol_id;
        vinfo.replica_factor = 2;
        vinfo.capacity = 64L*1024L*1024L*1024L*1024L;
    }

    //2016-05-03 Lego fix coverity issue CID: 37656
    ret = 0;
    ret2 = 0;
    syslog(LOG_ERR, "DMSC INFO get_vol_info result: ret=%d capacity=%llu\n",
            ret, (long long unsigned int)vinfo.capacity);

    if (is_invalid_vol_cap(vinfo.capacity)) {
        syslog(LOG_ERR, "DMSC WARN fail get volume capacity or wrong capacity "
                "(capicity= %llu) for attach volume id=%d\n",
                (long long unsigned int)vinfo.capacity, vol_id);
        ret = Attach_Vol_Fail;
        goto ATT_SEND_RETURN;
    }

    syslog(LOG_ERR,
            "DMSC INFO Issue IOCTL to driver to attach volume %d\n", vol_id);

    ret = ioctl(dms_chdev_fd, IOCTL_ATTACH_VOL, &vinfo);

    syslog(LOG_ERR,
            "DMSC INFO ioctl for attach volume %d with ret=%d\n", vol_id, ret);

    switch (ret) {
    case Attach_Vol_OK:
        break;
    case Attach_Vol_Exist_Attaching:
    case Attach_Vol_Exist:
    case Attach_Vol_Exist_Detaching:
        syslog(LOG_ERR, "DMSC WARN vol exist for volid %d and ret %d\n",
                vol_id, ret);
        goto ATT_SEND_RETURN;
        break;
    case Attach_Vol_Max_Volumes:
        syslog(LOG_ERR, "DMSC WARN reach max num volumes\n");
        goto ATT_SEND_DETACH_TO_NN;
        break;
    case DRIVER_ISNOT_WORKING:
        syslog(LOG_ERR, "DMSC WARN system is not ready or enter "
                "shutdown mode, reject any command: IOCTL_ATTACH_VOL\n");
        ret = Attach_Vol_Fail;
        goto ATT_SEND_DETACH_TO_NN;
        break;
    default:
        ret = Attach_Vol_Fail;
        goto ATT_SEND_DETACH_TO_NN;
        break;
    }

    if (is_dev_file_exist(nnName, vol_id)) {
        perform_volume_setting(nnName, vol_id);
        ret = Attach_Vol_OK;
    } else {
        ret = Attach_Vol_Fail;
        do_roll_back = 1;
        syslog(LOG_ERR, "DMSC WARN Attach fail due to device file "
                "%s%s%d not exist\n", DEV_FULL_PATH_PREFIX, nnName, vol_id);
    }

    //TODO: issue vol_id + mds id
    ret2 = ioctl(dms_chdev_fd, IOCTL_ATTACH_VOL_FINISH, &vinfo);

    if (Attach_Vol_OK == ret && 0 == ret2) {
        syslog(LOG_ERR, "DMSC INFO attach volume %d to inst %s success\n",
                vol_id, instid);
    } else if (Attach_Vol_OK == ret && DRIVER_ISNOT_WORKING == ret2) {
        syslog(LOG_ERR, "DMSC WARN system is not ready or enter shutdown "
                "mode, reject any command: IOCTL_ATTACH_VOL_FINISH\n");
        ret = Attach_Vol_Fail;
    } else if (Attach_Vol_Fail == ret && 0 == ret2) {
        ret = Attach_Vol_Fail;
        do_roll_back = 1;
    } else if (Attach_Vol_Fail == ret && DRIVER_ISNOT_WORKING == ret2) {
        ret = Attach_Vol_Fail;
    }

    if (do_roll_back) {
        force_detach_vol(nnName, nnIPAddr, vol_id);
    }

ATT_SEND_DETACH_TO_NN:
    if (Attach_Vol_OK != ret) {
        //TODO: If send fail, how to handle
        if (send_detach_to_nn(nnIPAddr, vol_id) == 0) {
            syslog(LOG_ERR, "DMSC INFO fail to send detach command to NN when "
                    "attach %d fail\n", vol_id);
        }
    }

ATT_SEND_RETURN:
    return ret;
}

detach_response_t detach_volume (int8_t *nnName, uint32_t nnIPAddr,
        int32_t vol_id, int8_t *instid)
{
    struct dms_volume_info vinfo;
    int ret;

    syslog(LOG_ERR,
            "DMSC INFO Issue IOCTL to driver to detach NN %s %u volume vol_id %d\n",
            nnName, nnIPAddr, vol_id);

    vinfo.nnSrv_IPV4addr = nnIPAddr;
    strcpy(vinfo.nnSrv_nm, nnName);
    vinfo.volumeID = vol_id;
    vinfo.share_volume = 0;
    vinfo.capacity = 0;
    ret = ioctl(dms_chdev_fd, IOCTL_DETATCH_VOL, &vinfo);

    syslog(LOG_ERR, "DMSC INFO detach volume %d result is %d\n", vol_id, ret);

	if (ret == DRIVER_ISNOT_WORKING) {
	    syslog(LOG_ERR, "DMS-CLIENT: system is not ready or enter "
	            "shutdown mode, reject any command: IOCTL_DETACH_VOL\n");
	    ret = Detach_Vol_Fail;
	}

	if (Detach_Vol_OK == ret) {
	    //TODO: Handle cannot send detach information to NN
        if (send_detach_to_nn(nnIPAddr, vol_id)) {
            syslog(LOG_ERR, "DMS-CLIENT: fail to send detach volume %d to NN "
                    "with result %d\n", vol_id, ret);
        }
	}

    syslog(LOG_ERR, "detach volume %d done result is %d\n", vol_id, ret);

    return ret;
}

//TODO : Current only implement cacnel attach, did we need this?
static int32_t cancel_volume_operation (int8_t *nnName, uint32_t nnIPAddr, int32_t vol_id,
        char *instid) {
    int32_t ret;
    ret = detach_volume(nnName, nnIPAddr, vol_id, instid);
    return ret;
}

static void reset_client_fd_with_lock (int * new_socket_fd)
{
    pthread_mutex_lock(&g_client_conn_mutex);
    *new_socket_fd = -1;
    pthread_mutex_unlock(&g_client_conn_mutex);
}

static short get_short_int (char *buf)
{
    short val;
    val = *((short *) buf);
    val = ntohs(val);

    //2016-05-03 Lego fix coverity issue CID: 37625
    return val;
}

static int get_int (char *buf)
{
    int val;
    val = *((int *) buf);
    val = ntohl(val);

    //2016-05-03 Lego fix coverity issue CID: 37626
    return val;
}

static unsigned long long get_int64 (char *buf)
{
    unsigned long long val;
    val = *((unsigned long long *) buf);
    val = ntohll(val);

    //2016-05-03 Lego fix coverity issue CID: 37623
    return val;
}

static void cmd_do_attach_volume (int skfd, char *cmd_buffer, int pktlen)
{
    unsigned long long volID;
    char *ptr, *instID;
    int ret, retSendRes;

    ptr = cmd_buffer;

    volID = get_int64(ptr);
    ptr += sizeof(unsigned long long);

    syslog(LOG_ERR, "DMSC INFO Attach Received volid is %llu\n", volID);

    //2016-05-03 Lego fix coverity issue CID: 37654
    instID = NULL;
    if (pktlen > 10) {
        instID = ptr;
        instID[pktlen - sizeof(short) - sizeof(unsigned long long)] = '\0';

        syslog(LOG_ERR, "DMSC INFO Commands: attach volume [id=%llu] to "
                "instance [id=%s]\n", volID, instID);

        ret = (int)attach_volume(gdef_nnSrvName, gdef_nnSrvIPAddr, volID, instID);

    } else {
        syslog(LOG_ERR, "attach commands format error!\n");
        ret = ATTACH_FAIL_FOR_USER;
    }

    retSendRes = send_response(skfd, ret);
    if (instID != NULL) {
        syslog(LOG_ERR, "DMSC INFO Process attach command %llu to inst: %s done, "
                "result = %d (0 means success) and set response result=%d\n",
                volID, instID, ret, retSendRes);
    } else {
        syslog(LOG_ERR, "DMSC INFO Process attach command %llu done, "
                "result = %d (0 means success) and set response result=%d\n",
                volID, ret, retSendRes);
    }
}

static void cmd_do_detach_volume (int skfd, char *cmd_buffer, int pktlen)
{
    unsigned long long volID;
    char *ptr, *instID;
    int32_t ret, retSendRes;

    ptr = cmd_buffer;

    volID = get_int64(ptr);
    ptr += sizeof(unsigned long long);

    syslog(LOG_ERR, "DMSC INFO Detach Received volid is %llu\n", volID);

    //2016-05-03 Lego fix coverity issue CID: 37657
    instID = NULL;
    if (pktlen > 10) {
        instID = ptr;
        instID[pktlen - sizeof(short) - sizeof(unsigned long long)] = '\0';

        syslog(LOG_ERR, "DMSC INFO Commands: detach volume [id=%llu] to "
                "instance [id=%s] without device file\n",
                volID, instID);

        ret = (int32_t)detach_volume(gdef_nnSrvName, gdef_nnSrvIPAddr, volID, instID);

    } else {
        syslog(LOG_ERR, "DMSC WARN detach commands format error!\n");
        ret = Detach_Vol_Fail;
    }

    retSendRes = send_response(skfd, ret);
    if (instID != NULL) {
        syslog(LOG_ERR, "DMSC INFO Process detach command %llu to inst %s: done, "
                "result = %d (0 means success) and set response result=%d\n",
                volID, instID, ret, retSendRes);
    } else {
        syslog(LOG_ERR, "DMSC INFO Process detach command %llu, "
                "result = %d (0 means success) and set response result=%d\n",
                volID, ret, retSendRes);
    }
}

static void cmd_do_cancel_op (int skfd, char *cmd_buffer, int pktlen)
{
    unsigned long long volID;
    char *ptr, *instID;
    int ret, retSendRes;

    ptr = cmd_buffer;

    volID = get_int64(ptr);
    ptr += sizeof(unsigned long long);

    //2016-05-03 Lego fix coverity issue CID: 37655
    instID = NULL;
    syslog(LOG_ERR, "DMSC INFO Cancel operation Received volid is %llu\n",
            volID);
    if (pktlen > 10) {
        instID = ptr;
        instID[pktlen - sizeof(short) - sizeof(unsigned long long)] = '\0';
        syslog(LOG_ERR,"DMSC INFO Commands: cancel operation [id=%llu] to "
                "instance [id=%s]\n", volID, instID);
        ret = cancel_volume_operation(gdef_nnSrvName, gdef_nnSrvIPAddr, volID, instID);
    }  else {
        syslog(LOG_ERR, "DMSC WARN cancel operation commands format error!\n");
        ret = -1;
    }

    retSendRes = send_response(skfd, ret);
    if (instID != NULL) {
        syslog(LOG_ERR, "DMSC INFO Process cancel operation command %llu to inst %s: done, "
            "result = %d (0 means success) and set response result=%d\n",
            volID, instID, ret, retSendRes);
    } else {
        syslog(LOG_ERR, "DMSC INFO Process cancel operation command %llu done, "
            "result = %d (0 means success) and set response result=%d\n",
            volID, ret, retSendRes);
    }
}

/*
 * TODO: support range clear
 */
static void cmd_do_reset_ovw (int skfd, char *cmd_buffer)
{
    char *ptr, buffer[MAX_CMD_PKTLEN];;
    int reqID;
    unsigned long long volID;
    short ackCode, retPktlen, total_len;

    ptr = cmd_buffer;

    reqID = get_int(ptr);
    ptr += sizeof(int);

    volID = get_int64(ptr);
    ptr += sizeof(unsigned long long);

    syslog(LOG_ERR, "DMSC INFO issue reset ovw to driver with request id = %d "
            "and volume id = %llu\n", reqID, volID);
    //TODO: need mds IP + vol IP
    ackCode = ioctl(dms_chdev_fd, IOCTL_RESETOVERWRITTEN, (unsigned long)volID);
    if(ackCode == DRIVER_ISNOT_WORKING) {
        syslog(LOG_ERR, "DMSC WARN system is not ready or enter shutdown mode, "
                "reject any command: IOCTL_RESETOVERWRITTEN\n");
        ackCode = Clear_OVW_Fail;
    }

    syslog(LOG_ERR, "DMSC INFO Ready to send ackCode to namenode with "
            "ackCode=%d request id = %d and volume id = %llu\n",
            ackCode, reqID, volID);

    retPktlen = sizeof(short) + sizeof(int);
    total_len = retPktlen + sizeof(short);
    retPktlen = htons(retPktlen);
    ackCode = htons(ackCode);
    reqID = htonl(reqID);
    memset(buffer, 0, sizeof(char)*MAX_CMD_PKTLEN);
    ptr = &buffer[0];

    memcpy(ptr, (char*) (&retPktlen), sizeof(short));
    ptr += sizeof(short);
    memcpy(ptr, (char*) (&ackCode), sizeof(short));
    ptr += sizeof(short);
    memcpy(ptr, (char*) (&reqID), sizeof(int));

    if (socket_send(skfd, buffer, total_len)) {
        syslog(LOG_ERR, "DMSC WARN Fail to send response to NN "
                "(DMS_CMD_RESET_OVERWRITTEN_FLAG)\n");
    }
}

/*
 * Check reset cache by LBID , maybe overflow due to pktlen including
 * LBIDs..
 */
static void cmd_do_reset_cache (int skfd, char *cmd_buffer)
{
    int subOpcode, reqID, i;
    char *ptr, buffer[MAX_CMD_PKTLEN];
    unsigned long long volID;
    struct ResetCacheInfo *resetInfo;
    unsigned int num_of_lb;
    short ackCode, retPktlen, total_len;
    unsigned int dnPort, dnNameLen;

    ptr = cmd_buffer;

    subOpcode = get_int(ptr);
    ptr += sizeof(int);

    reqID = get_int(ptr);
    ptr += sizeof(int);

    resetInfo = (struct ResetCacheInfo*) malloc(sizeof(struct ResetCacheInfo));

    if (resetInfo == NULL) {
        //TODO Send error to NN
        syslog(LOG_ERR, "DMSC WARN Cannot alloc mem when reset "
                "metadata cache\n");
        return;
    }

    syslog(LOG_ERR, "DMSC INFO Receive clear meta cache command with "
            "request id %d and subopcode %d\n", reqID, subOpcode);

    resetInfo->SubOpCode = subOpcode;
    switch (subOpcode) {
    case SUBOP_INVALIDATECACHE_BYLBIDS:
        volID = get_int64(ptr);
        ptr += sizeof(unsigned long long);

        num_of_lb = get_int(ptr);
        ptr += sizeof(unsigned int);
        resetInfo->u.LBIDs.VolumeID = volID;
        resetInfo->u.LBIDs.NumOfLBID = num_of_lb;
        resetInfo->u.LBIDs.LBID_List =
                (uint64_t *)malloc_wrap(sizeof(uint64_t)*num_of_lb);
        if(!resetInfo->u.LBIDs.LBID_List){
            syslog(LOG_ERR, "DMS WARN malloc() finall fail, size = %zu\n",
                    sizeof(uint64_t)*num_of_lb);
            ackCode = -1;
            break;
        }
        syslog(LOG_ERR, "num of LBIDs %d and LBIDs as following: \n",num_of_lb);
        
        socket_rcv(skfd, (char *)resetInfo->u.LBIDs.LBID_List, sizeof(uint64_t)*num_of_lb);
        for (i = 0; i < resetInfo->u.LBIDs.NumOfLBID; i++) {
            resetInfo->u.LBIDs.LBID_List[i] = ntohll(resetInfo->u.LBIDs.LBID_List[i]);
            syslog(LOG_DEBUG, " %llu", (long long unsigned int)resetInfo->u.LBIDs.LBID_List[i]);
        }

        //TODO: need mds IP + vol IP
        ackCode = ioctl(dms_chdev_fd, IOCTL_INVALIDCACHE, (unsigned long)resetInfo);
        syslog(LOG_ERR, "invalidate cache by LBIDs: ackCode: %d\n", ackCode);

        free(resetInfo->u.LBIDs.LBID_List);
        break;

    case SUBOP_INVALIDATECACHE_BYHBIDS:
        num_of_lb = get_int(ptr);
        ptr += sizeof(unsigned int);

        resetInfo->u.HBIDs.NumOfHBID = num_of_lb;
        resetInfo->u.HBIDs.HBID_List =
                        (uint64_t *)malloc(sizeof(uint64_t)*num_of_lb);
        syslog(LOG_ERR, "num of HBIDs %d %d and HBID as following: \n",
                num_of_lb, resetInfo->u.HBIDs.NumOfHBID);

        for (i = 0; i < resetInfo->u.HBIDs.NumOfHBID; i++) {
            resetInfo->u.HBIDs.HBID_List[i] = get_int64(ptr);
            ptr += sizeof(unsigned long long);
            syslog(LOG_ERR, " %llu", (long long unsigned int)resetInfo->u.HBIDs.HBID_List[i]);
        }

        ackCode = ioctl(dms_chdev_fd, IOCTL_INVALIDCACHE, (unsigned long)resetInfo);
        syslog(LOG_ERR, "invalidate cache by HBIDs: ackCode: %d\n", ackCode);

        free(resetInfo->u.HBIDs.HBID_List);

        break;

    case SUBOP_INVALIDATECACHE_BYVOLID:
        volID = get_int64(ptr);
        ptr += sizeof(unsigned long long);

        //TODO: need mds IP + vol IP
        resetInfo->u.VolID.VolumeID = volID;
        ackCode = ioctl(dms_chdev_fd, IOCTL_INVALIDCACHE,
                (unsigned long)resetInfo);
        syslog(LOG_ERR, "invalidate cache by volume id: ackCode: %d\n", ackCode);
        break;

    case SUBOP_INVALIDATECACHE_BYDNNAME:
        dnPort = get_int(ptr);
        ptr += sizeof(unsigned int);

        dnNameLen = get_int(ptr);
        ptr += sizeof(unsigned int);

        resetInfo->u.DNName.Port = dnPort;
        resetInfo->u.DNName.NameLen = dnNameLen;
        resetInfo->u.DNName.Name = (char*) malloc(sizeof(char) * dnNameLen);

        memcpy(resetInfo->u.DNName.Name, ptr, dnNameLen);
        ackCode = ioctl(dms_chdev_fd, IOCTL_INVALIDCACHE,
                (unsigned long)resetInfo);
        free(resetInfo->u.DNName.Name);
        break;

    default:
        syslog(LOG_ERR, "DMSC WARN Receive unknow subcode for reset cache %d\n",
                subOpcode);
        ackCode = -1;
        break;
    }

    if(ackCode == DRIVER_ISNOT_WORKING) {
        syslog(LOG_ERR, "DMSC WARN system is not ready or enter shutdown mode, "
                "reject any command: IOCTL_INVALIDCACHE\n");
    }

    retPktlen = sizeof(short) + sizeof(int);
    total_len = retPktlen + sizeof(short);
    retPktlen = htons(retPktlen);
    ackCode = htons(ackCode);
    reqID = htonl(reqID);
    memset(buffer, 0, sizeof(char)*MAX_CMD_PKTLEN);
    ptr = &buffer[0];

    memcpy(ptr, (char*) (&retPktlen), sizeof(short));
    ptr += sizeof(short);
    memcpy(ptr, (char*) (&ackCode), sizeof(short));
    ptr += sizeof(short);
    memcpy(ptr, (char*) (&reqID), sizeof(int));

    if (socket_send(skfd, buffer, total_len)) {
        syslog(LOG_ERR, "DMSC WARN Fail to send response to NN "
                "(IOCTL_INVALIDCACHE)\n");
    }

    free(resetInfo);
}

static void cmd_do_block_io (int skfd, char *cmd_buffer, short op)
{
    char *ptr, buffer[MAX_CMD_PKTLEN];
    int reqID;
    unsigned long long volID;
    short ackCode, retPktlen, total_len;

    ptr = cmd_buffer;

    reqID = get_int(ptr);
    ptr += sizeof(int);

    volID = get_int64(ptr);
    ptr += sizeof(unsigned long long);

    switch (op) {
    case DMS_CMD_BLOCK_VOLUME_IO:
        syslog(LOG_ERR, "DMSC INFO Req ID %d Set Volume %llu as Block IO\n",
                reqID, volID);
        //TODO: need mds IP + vol IP
        ackCode = ioctl(dms_chdev_fd, IOCTL_BLOCK_VOLUME_IO,
                (unsigned long)volID);
        break;
    case DMS_CMD_UNBLOCK_VOLUME_IO:
        syslog(LOG_ERR, "DMSC INFO Req ID %d Set Volume %llu as UnBlock IO\n",
                reqID, volID);
        //TODO: need mds IP + vol IP
        ackCode = ioctl(dms_chdev_fd, IOCTL_UNBLOCK_VOLUME_IO,
                (unsigned long)volID);
        break;
    default:
        syslog(LOG_ERR, "DMSC INFO Req ID %d Unknown Block IO OP %d for "
                "volume %llu\n", reqID, op, volID);
        return;
    }

    syslog(LOG_ERR, "DMSC INFO Ready to send ackCode to namenode with ackCode=%d "
            "request id = %d and volume id = %llu\n",
            ackCode, reqID, volID);

    if (ackCode == DRIVER_ISNOT_WORKING) {
        syslog(LOG_ERR, "DMSC WANR system is not ready or enter "
                "shutdown mode, reject any command: "
                "DMS_CMD_BLOCK_VOLUME_IO / DMS_CMD_UNBLOCK_VOLUME_IO\n");
        if (DMS_CMD_BLOCK_VOLUME_IO == op) {
            ackCode = Block_IO_Fail;
        } else {
            ackCode = UnBlock_IO_OK;
        }
    }


    retPktlen = sizeof(short) + sizeof(int);
    total_len = retPktlen + sizeof(short);
    retPktlen = htons(retPktlen);
    ackCode = htons(ackCode);
    reqID = htonl(reqID);
    memset(buffer, 0, sizeof(char)*MAX_CMD_PKTLEN);
    ptr = &buffer[0];

    memcpy(ptr, (char*) (&retPktlen), sizeof(short));
    ptr += sizeof(short);
    memcpy(ptr, (char*) (&ackCode), sizeof(short));
    ptr += sizeof(short);
    memcpy(ptr, (char*) (&reqID), sizeof(int));

    if (socket_send(skfd, buffer, total_len)) {
        syslog(LOG_ERR, "DMSC WARN Fail to send response to NN "
                "(DMS_CMD_BLOCK_VOLUME_IO/DMS_CMD_UNBLOCK_VOLUME_IO)\n");
        if (DMS_CMD_BLOCK_VOLUME_IO == op && Block_IO_OK == ackCode) {
            ackCode = ioctl(dms_chdev_fd, IOCTL_UNBLOCK_VOLUME_IO,
                (unsigned long)volID);
        }
    }
}

static void cmd_do_set_vol_share (int skfd, char *cmd_buffer)
{
    int32_t reqID;
    int8_t *ptr, buffer[MAX_CMD_PKTLEN];
    unsigned long long volID;
    uint16_t share_enable;
    struct dms_volume_info vinfo;
    int16_t ackCode, retPktlen, total_len;

    ptr = cmd_buffer;

    reqID = get_int(ptr);
    ptr += sizeof(int);

    volID = get_int64(ptr);
    ptr += sizeof(unsigned long long);

    share_enable = get_short_int(ptr);
    ptr += sizeof(int);

    //TODO: get default name and IP
    vinfo.volumeID = (int)volID;
    vinfo.share_volume = share_enable;

    //TODO: need mds IP + vol IP
    ackCode = ioctl(dms_chdev_fd, IOCTL_SET_VOLUME_SHARE_FEATURE, &vinfo);

    syslog(LOG_ERR, "DMSC INFO reqID %d set volume %llu as share %d "
            "result %d\n", reqID, volID, share_enable, ackCode);

    if (DRIVER_ISNOT_WORKING == ackCode) {
        syslog(LOG_ERR, "DMSC WARN system is not ready or enter shutdown mode, "
                "reject any command: IOCTL_SET_VOLUME_SHARE_FEATURE\n");
        ackCode = Set_Fea_Fail;
    }

    retPktlen = sizeof(short) + sizeof(int);
    total_len = retPktlen + sizeof(short);
    retPktlen = htons(retPktlen);
    ackCode = htons(ackCode);
    reqID = htonl(reqID);
    memset(buffer, 0, sizeof(char)*MAX_CMD_PKTLEN);
    ptr = &buffer[0];

    memcpy(ptr, (char*) (&retPktlen), sizeof(short));
    ptr += sizeof(short);
    memcpy(ptr, (char*) (&ackCode), sizeof(short));
    ptr += sizeof(short);
    memcpy(ptr, (char*) (&reqID), sizeof(int));

    if (socket_send(skfd, buffer, total_len)) {
        syslog(LOG_ERR, "DMSC WARN Fail to send response to NN "
                "(IOCTL_SET_VOLUME_SHARE_FEATURE)\n");
    }
}

static void cmd_do_stop_vol (int skfd, char *cmd_buffer)
{
    int8_t *ptr;
    uint64_t volID;
    int32_t ackCode;

    ptr = cmd_buffer;

    volID = get_int64(ptr);
    ptr += sizeof(unsigned long long);

    //TODO: need mds IP + vol IP
    syslog(LOG_ERR, "DMSC INFO stop vol %llu\n", (long long int)volID);
    ackCode = ioctl(dms_chdev_fd, IOCTL_STOP_VOL, (unsigned long)volID);

    syslog(LOG_ERR, "DMSC INFO ackCode of stop vol %llu is %d\n",
            (long long unsigned int)volID, ackCode);

    if(ackCode == DRIVER_ISNOT_WORKING) {
        syslog(LOG_ERR, "DMSC WANR system is not ready or enter "
                "shutdown mode, reject any command: "
                "DMS_CMD_STOP_VOL\n");
        ackCode = Stop_Vol_Fail;
    }

    if (send_response(skfd, (int)ackCode)) {
        syslog(LOG_ERR,
                "DMSC WARN Fail to send response to caller (DMS_CMD_STOP_VOL)\n");
    }
}

static void cmd_do_restart_vol (int skfd, char *cmd_buffer)
{
    int8_t *ptr;
    uint64_t volID;
    int32_t ackCode;

    ptr = cmd_buffer;

    volID = get_int64(ptr);
    ptr += sizeof(unsigned long long);

    //TODO: need mds IP + vol IP
    syslog(LOG_ERR, "DMSC INFO restart vol %llu\n", (long long int)volID);
    ackCode = ioctl(dms_chdev_fd, IOCTL_RESTART_VOL, (unsigned long)volID);

    syslog(LOG_ERR, "DMSC INFO ackCode of stop vol %llu is %d\n",
            (long long unsigned int)volID, ackCode);

    if (ackCode == DRIVER_ISNOT_WORKING) {
        syslog(LOG_ERR, "DMSC WANR system is not ready or enter "
                "shutdown mode, reject any command: "
                "DMS_CMD_STOP_VOL\n");
        ackCode = Restart_Vol_Fail;
    }

    if (send_response(skfd, (int)ackCode)) {
        syslog(LOG_ERR,
                "DMSC WARN Fail to send response to caller (DMS_CMD_RESTART_VOL)\n");
    }
}

static int32_t _gen_latency_resp (int8_t *src_buff, int8_t *dest_buff)
{
    int8_t *ptr_src, *ptr_dest;
    int32_t pktlen, i, nums;

    pktlen = 0;
    ptr_src = src_buff;
    ptr_dest = dest_buff;

    nums = *((int16_t *)ptr_src);
    *((int16_t *)ptr_dest) = htons(nums); // # of records
    pktlen += sizeof(int16_t);
    ptr_src += sizeof(int16_t);
    ptr_dest += sizeof(int16_t);

    for (i = 0; i < nums; i++) {
        *((int32_t *)ptr_dest) = htonl(*((int32_t *)ptr_src)); //IP
        pktlen += sizeof(int32_t);
        ptr_src += sizeof(int32_t);
        ptr_dest += sizeof(int32_t);

        *((int32_t *)ptr_dest) = htonl(*((int32_t *)ptr_src)); //Port
        pktlen += sizeof(int32_t);
        ptr_src += sizeof(int32_t);
        ptr_dest += sizeof(int32_t);

        *((int32_t *)ptr_dest) = htonl(*((int32_t *)ptr_src)); //100 wreq avg
        pktlen += sizeof(int32_t);
        ptr_src += sizeof(int32_t);
        ptr_dest += sizeof(int32_t);

        *((int32_t *)ptr_dest) = htonl(*((int32_t *)ptr_src)); //all wreq avg
        pktlen += sizeof(int32_t);
        ptr_src += sizeof(int32_t);
        ptr_dest += sizeof(int32_t);

        *((int32_t *)ptr_dest) = htonl(*((int32_t *)ptr_src)); //100 rreq avg
        pktlen += sizeof(int32_t);
        ptr_src += sizeof(int32_t);
        ptr_dest += sizeof(int32_t);

        *((int32_t *)ptr_dest) = htonl(*((int32_t *)ptr_src)); //all rreq avg
        pktlen += sizeof(int32_t);
        ptr_src += sizeof(int32_t);
        ptr_dest += sizeof(int32_t);
    }

    return pktlen;
}

static int32_t _gen_queue_len_resp (int8_t *src_buff, int8_t *dest_buff)
{
    int8_t *ptr_src, *ptr_dest;
    int32_t pktlen;

    pktlen = 0;
    ptr_src = src_buff;
    ptr_dest = dest_buff;

    *((int32_t *)ptr_dest) = htonl(*((int32_t *)ptr_src)); //IP
    pktlen += sizeof(int32_t);

    return pktlen;
}

static int32_t _gen_active_dn_resp (int8_t *src_buff, int8_t *dest_buff)
{
    int8_t *ptr_src, *ptr_dest;
    int32_t pktlen, i, nums;

    pktlen = 0;
    ptr_src = src_buff;
    ptr_dest = dest_buff;

    nums = *((int16_t *)ptr_src);
    *((int16_t *)ptr_dest) = htons(nums); // # of records
    pktlen += sizeof(int16_t);
    ptr_src += sizeof(int16_t);
    ptr_dest += sizeof(int16_t);

    for (i = 0; i < nums; i++) {
        *((int32_t *)ptr_dest) = htonl(*((int32_t *)ptr_src)); //IP
        pktlen += sizeof(int32_t);
        ptr_src += sizeof(int32_t);
        ptr_dest += sizeof(int32_t);

        *((int32_t *)ptr_dest) = htonl(*((int32_t *)ptr_src)); //Port
        pktlen += sizeof(int32_t);
        ptr_src += sizeof(int32_t);
        ptr_dest += sizeof(int32_t);
    }

    return pktlen;
}

static int32_t _get_getAllVols_resp (int8_t *src_buff, int8_t *dest_buff)
{
    int8_t *ptr_src, *ptr_dest;
    int32_t pktlen, i, nums;

    pktlen = 0;
    ptr_src = src_buff;
    ptr_dest = dest_buff;

    nums = *((int16_t *)ptr_src);
    *((int16_t *)ptr_dest) = htons(nums); // # of records
    pktlen += sizeof(int16_t);
    ptr_src += sizeof(int16_t);
    ptr_dest += sizeof(int16_t);

    for (i = 0; i < nums; i++) {
        *((int32_t *)ptr_dest) = htonl(*((int32_t *)ptr_src)); //volume id
        pktlen += sizeof(int32_t);
        ptr_src += sizeof(int32_t);
        ptr_dest += sizeof(int32_t);
    }

    return pktlen;
}

static void cmd_do_get_cn_info (int32_t skfd, int8_t *cmd_buffer)
{
    uint64_t volid;
    int8_t *ptr, *data_buffer, *dataptr, *resp_buffer, *resp_ptr;
    int16_t subopcode, pktlen;
    int8_t doIoctl;
    int32_t ackCode;

    ptr = cmd_buffer;
    subopcode = get_short_int(ptr);
    ptr += sizeof(int16_t);

    doIoctl = 1;

    data_buffer = malloc(sizeof(int8_t)*READ_DATA_PAGE_SIZE);
    dataptr = data_buffer;

    switch (subopcode) {
    case CN_INFO_SUBOP_GetAllCNDNLatencies:
        *((int16_t *)dataptr) = CN_INFO_SUBOP_GetAllCNDNLatencies;
        break;
    case CN_INFO_SUBOP_GetAllCNDNQLen:
        *((int16_t *)dataptr) = CN_INFO_SUBOP_GetAllCNDNQLen;
        break;
    case CN_INFO_SUBOP_GETVolActiveDN:
        *((int16_t *)dataptr) = subopcode;
        dataptr += sizeof(int16_t);

        volid = get_int64(ptr);
        *((int64_t *)dataptr) = volid;
        dataptr += sizeof(int64_t);
        break;
    case CN_INFO_SUBOP_GETAllVols:
        *((int16_t *)dataptr) = CN_INFO_SUBOP_GETAllVols;
        break;
    default:
        syslog(LOG_ERR,
                "DMSC WARN wrong subopcode %d of get cn info\n", subopcode);
        doIoctl = 0;
        break;
    }

    if (doIoctl) {
        ackCode = ioctl(dms_chdev_fd, IOCTL_GET_CN_INFO,
                (unsigned long)data_buffer);
    } else {
        ackCode = -1002;
    }

    resp_buffer = malloc(sizeof(int8_t)*READ_DATA_PAGE_SIZE);

    pktlen = 0;
    resp_ptr = resp_buffer;
    resp_ptr += sizeof(int16_t);

    *((int16_t *)resp_ptr) = htons(DMS_CMD_GET_CN_INFO);
    resp_ptr += sizeof(int16_t);
    pktlen += sizeof(int16_t);

    *((int16_t *)resp_ptr) = htons(subopcode);
    resp_ptr += sizeof(int16_t);
    pktlen += sizeof(int16_t);

    *((int16_t *)resp_ptr) = htons(-ackCode);
    resp_ptr += sizeof(int16_t);
    pktlen += sizeof(int16_t);

    if (ackCode != 0) {
        *((int16_t *)resp_buffer) = htons(pktlen);
    } else {
        switch (subopcode) {
        case CN_INFO_SUBOP_GetAllCNDNLatencies:
            pktlen += _gen_latency_resp(data_buffer, resp_ptr);
            break;
        case CN_INFO_SUBOP_GetAllCNDNQLen:
            pktlen += _gen_queue_len_resp(data_buffer, resp_ptr);
            break;
        case CN_INFO_SUBOP_GETVolActiveDN:
            pktlen += _gen_active_dn_resp(data_buffer, resp_ptr);
            break;
        case CN_INFO_SUBOP_GETAllVols:
            pktlen += _get_getAllVols_resp(data_buffer, resp_ptr);
            break;
        default:
            syslog(LOG_ERR,
                    "DMSC WARN wrong subopcode %d of get cn info\n", subopcode);
            break;
        }
        *((int16_t *)resp_buffer) = htons(pktlen);
    }

    socket_send(skfd, (char*)(resp_buffer), pktlen + 2);

    free(data_buffer);
    free(resp_buffer);
}

static void cmd_do_inval_vol_fs (int skfd, char *cmd_buffer)
{
    userD_prealloc_ioctl_cmd_t ioctlcmd;
    UserD_invalfs_resp_t inval_resp;
    char *buf_ptr;
    int16_t ackCode;

    buf_ptr = cmd_buffer;
    ioctlcmd.requestID = get_int(buf_ptr);
    //ioctlcmd.requestID = ntohl(ioctlcmd.requestID);
    buf_ptr += sizeof(uint32_t);

    ioctlcmd.volumeID = get_int64(buf_ptr);
    //ioctlcmd.volumeID = ntohll(ioctlcmd.volumeID);
    buf_ptr += sizeof(uint64_t);

    ioctlcmd.chunkID = get_int64(buf_ptr);
    //ioctlcmd.chunkID = ntohll(ioctlcmd.chunkID);
    buf_ptr += sizeof(uint64_t);

    ioctlcmd.opcode = DMS_CMD_INVALIDATE_VOLUME_FREE_CHUNK;
    ioctlcmd.subopcode = SUBOP_PREALLOC_Dummy;

    syslog(LOG_ERR, "DMSC INFO got invalid volume reqID %u volume %llu chunk %llu\n",
            ioctlcmd.requestID, (unsigned long long)ioctlcmd.volumeID,
            (unsigned long long)ioctlcmd.chunkID);

    ackCode = ioctl(dms_chdev_fd, IOCTL_INVAL_VOL_FS, (unsigned long)(&ioctlcmd));

    syslog(LOG_ERR, "DMSC INFO reqID %u invalid volume %llu chunk %llu "
            "result %d\n", ioctlcmd.requestID, (unsigned long long)ioctlcmd.volumeID,
            (unsigned long long)ioctlcmd.chunkID, ackCode);
    if (DRIVER_ISNOT_WORKING == ackCode) {
        syslog(LOG_ERR, "DMSC WARN system is not ready or enter shutdown mode, "
                "reject any command: IOCTL_INVAL_VOL_FS\n");
        ackCode = Inval_chunk_FAIL;
    }

    inval_resp.pkt_len = sizeof(UserD_invalfs_resp_t) - sizeof(uint16_t);
    inval_resp.pkt_len = htons(inval_resp.pkt_len);
    inval_resp.ack_code = htons(ackCode);
    inval_resp.requestID = htonl(ioctlcmd.requestID);
    inval_resp.maxUnuse_hbid = htonll(ioctlcmd.max_unuse_hbid);

    syslog(LOG_ERR, "DMSC INFO inval send packet len %u ack_code %u request ID %u maxhbid %llu\n",
            (uint32_t)(sizeof(UserD_invalfs_resp_t) - sizeof(uint16_t)),
            ackCode, ioctlcmd.requestID, (unsigned long long)ioctlcmd.max_unuse_hbid);

    if (socket_send(skfd, (char *)(&inval_resp), sizeof(UserD_invalfs_resp_t))) {
        syslog(LOG_ERR, "DMSC WARN Fail to send response to NN "
                "(IOCTL_INVAL_VOL_FS)\n");
    }
}

static void cmd_do_clean_vol_fs (int skfd, char *cmd_buffer)
{
    userD_prealloc_ioctl_cmd_t ioctlcmd;
    UserD_prealloc_resp_t clean_resp;
    char *buf_ptr;
    int16_t ackCode;

    buf_ptr = cmd_buffer;
    ioctlcmd.requestID = get_int(buf_ptr);
    //ioctlcmd.requestID = ntohl(ioctlcmd.requestID);
    buf_ptr += sizeof(int);

    ioctlcmd.volumeID = get_int64(buf_ptr);
    //ioctlcmd.volumeID = ntohll(ioctlcmd.volumeID);
    buf_ptr += sizeof(unsigned long long);

    ioctlcmd.chunkID = get_int64(buf_ptr);
    //ioctlcmd.chunkID = ntohll(ioctlcmd.chunkID);
    buf_ptr += sizeof(unsigned long long);

    ioctlcmd.opcode = DMS_CMD_CLEANUP_VOLUME_FREE_CHUNK;
    ioctlcmd.subopcode = SUBOP_PREALLOC_Dummy;

    syslog(LOG_ERR, "DMSC INFO got clean volume reqID %u volume %llu chunk %llu\n",
            ioctlcmd.requestID, (unsigned long long)ioctlcmd.volumeID,
            (unsigned long long)ioctlcmd.chunkID);

    ackCode = ioctl(dms_chdev_fd, IOCTL_CLEAN_VOL_FS, (unsigned long)(&ioctlcmd));

    syslog(LOG_ERR, "DMSC INFO reqID %d clean volume %llu chunk %llu "
            "result %d\n", ioctlcmd.requestID, (unsigned long long)ioctlcmd.volumeID,
            (unsigned long long)ioctlcmd.chunkID, ackCode);
    if (DRIVER_ISNOT_WORKING == ackCode) {
        syslog(LOG_ERR, "DMSC WARN system is not ready or enter shutdown mode, "
                "reject any command: IOCTL_FLUSH_VOL_FS\n");
        ackCode = Clean_chunk_FAIL;
    }

    clean_resp.pkt_len = sizeof(UserD_prealloc_resp_t) - sizeof(uint16_t);
    clean_resp.pkt_len = htons(clean_resp.pkt_len);
    clean_resp.ack_code = htons(ackCode);
    clean_resp.requestID = htonl(ioctlcmd.requestID);

    syslog(LOG_ERR, "DMSC INFO clean send packet len %u ack_code %u request ID %u\n",
            (uint32_t)(sizeof(UserD_prealloc_resp_t) - sizeof(uint16_t)),
            ackCode, ioctlcmd.requestID);

    if (socket_send(skfd, (char *)(&clean_resp), sizeof(UserD_prealloc_resp_t))) {
        syslog(LOG_ERR, "DMSC WARN Fail to send response to NN "
                "(IOCTL_CLEAN_VOL_FS)\n");
    }
}

static void cmd_do_flush_vol_fs (int skfd, char *cmd_buffer)
{
    userD_prealloc_ioctl_cmd_t ioctlcmd;
    UserD_prealloc_resp_t clean_resp;
    char *buf_ptr;
    int16_t ackCode;

    buf_ptr = cmd_buffer;
    ioctlcmd.requestID = get_int(buf_ptr);
    //ioctlcmd.requestID = ntohl(ioctlcmd.requestID);
    buf_ptr += sizeof(int);

    ioctlcmd.volumeID = get_int64(buf_ptr);
    //ioctlcmd.volumeID = ntohll(ioctlcmd.volumeID);
    buf_ptr += sizeof(unsigned long long);

    ioctlcmd.chunkID = get_int64(buf_ptr);
    //ioctlcmd.chunkID = ntohll(ioctlcmd.chunkID);
    buf_ptr += sizeof(unsigned long long);

    ioctlcmd.opcode = DMS_CMD_FLUSH_VOLUME_FREE_CHUNK;
    ioctlcmd.subopcode = SUBOP_PREALLOC_Dummy;

    syslog(LOG_ERR, "DMSC INFO got flush volume reqID %u volume %llu chunk %llu\n",
            ioctlcmd.requestID, (unsigned long long)ioctlcmd.volumeID,
            (unsigned long long)ioctlcmd.chunkID);

    ackCode = ioctl(dms_chdev_fd, IOCTL_FLUSH_VOL_FS, (unsigned long)(&ioctlcmd));

    syslog(LOG_ERR, "DMSC INFO reqID %d flush volume %llu chunk %llu "
            "result %d\n", ioctlcmd.requestID, (unsigned long long)ioctlcmd.volumeID,
            (unsigned long long)ioctlcmd.chunkID, ackCode);
    if (DRIVER_ISNOT_WORKING == ackCode) {
        syslog(LOG_ERR, "DMSC WARN system is not ready or enter shutdown mode, "
                "reject any command: IOCTL_FLUSH_VOL_FS\n");
        ackCode = Flush_chunk_FAIL;
    }

    clean_resp.pkt_len = sizeof(UserD_prealloc_resp_t) - sizeof(uint16_t);
    clean_resp.pkt_len = htons(clean_resp.pkt_len);
    clean_resp.ack_code = htons(ackCode);
    clean_resp.requestID = htonl(ioctlcmd.requestID);

    syslog(LOG_ERR, "DMSC INFO flush send packet len %u ack_code %u request ID %u\n",
            (uint32_t)(sizeof(UserD_prealloc_resp_t) - sizeof(uint16_t)),
            ackCode, ioctlcmd.requestID);

    if (socket_send(skfd, (char *)(&clean_resp), sizeof(UserD_prealloc_resp_t))) {
        syslog(LOG_ERR, "DMSC WARN Fail to send response to NN "
                "(IOCTL_FLUSH_VOL_FS)\n");
    }
}

static void cmd_do_prealloc_cmd (int skfd, char *cmd_buffer)
{
    userD_prealloc_ioctl_cmd_t ioctlcmd;
    UserD_prealloc_resp_t clean_resp;
    char *buf_ptr;
    int16_t ackCode;

    buf_ptr = cmd_buffer;

    ioctlcmd.subopcode = get_short_int(buf_ptr);
    buf_ptr += sizeof(short);

    ioctlcmd.requestID = get_int(buf_ptr);
    buf_ptr += sizeof(int);

    ioctlcmd.volumeID = get_int64(buf_ptr);
    buf_ptr += sizeof(unsigned long long);

    ioctlcmd.opcode = DMS_CMD_PREALLOC;

    syslog(LOG_ERR, "DMSC INFO got prealloc subopcode %u reqID %u volume %llu\n",
            ioctlcmd.subopcode, ioctlcmd.requestID, (unsigned long long)ioctlcmd.volumeID);

    ackCode = ioctl(dms_chdev_fd, IOCTL_PREALLOC_CMD, (unsigned long)(&ioctlcmd));

    syslog(LOG_ERR, "DMSC INFO reqID %d prealloc subopcode %d volume %llu "
            "result %d\n", ioctlcmd.requestID, ioctlcmd.subopcode,
            (unsigned long long)ioctlcmd.volumeID, ackCode);
    if (DRIVER_ISNOT_WORKING == ackCode) {
        syslog(LOG_ERR, "DMSC WARN system is not ready or enter shutdown mode, "
                "reject any command: IOCTL_PREALLOC_CMD\n");
        ackCode = Prealloc_cmd_FAIL;
    }

    clean_resp.pkt_len = sizeof(UserD_prealloc_resp_t) - sizeof(uint16_t);
    clean_resp.pkt_len = htons(clean_resp.pkt_len);
    clean_resp.ack_code = htons(ackCode);
    clean_resp.requestID = htonl(ioctlcmd.requestID);

    syslog(LOG_ERR, "DMSC INFO prealloc send packet len %u ack_code %u request ID %u\n",
            (uint32_t)(sizeof(UserD_prealloc_resp_t) - sizeof(uint16_t)),
            ackCode, ioctlcmd.requestID);

    if (socket_send(skfd, (char *)(&clean_resp), sizeof(UserD_prealloc_resp_t))) {
        syslog(LOG_ERR, "DMSC WARN Fail to send response to NN "
                "(IOCTL_PREALLOC_CMD)\n");
    }
}

void * malloc_wrap (size_t size)
{
    void *p;
    int32_t retry_cnt = 0;
    
    while(retry_cnt < 30)
    {
        p = malloc(size);
        if(!p){
            syslog(LOG_ERR, "DMS WARN malloc() fail, size = %zu\n", size);
            retry_cnt++;
            sleep(1);
            continue;
        }
        break;
    }
    return p;
}    
/*
 * cmd_process_thread : handle commands
 * 1. attach volume : packsize Opcode volumeID instanceID (Opcode = 1 and 3)
 * 2. detach volume : packsize  Opcode volumeID (Opcode = 2 and 4)
 * 3. reset overwritten flag : packsize Opcode (Opcode = 11)
 * 4. reset cache : packsize Opcode (Opcode = 12) volumeID 
 * 5. attach all volume : packsize Opcode (Opcode = 5)
 * 6. detach all volume : packsize Opcode (Opcode = 6)
 * 7. attach volume id on device id : packsize  Opcode volumeID InstanceID deviceID (Opcode = 7, long, int)
 * 8. change volume ack policy to DISK_ACK : packsize  Opcode volumeID  (Opcode = 8, long)
 * 9. change volume ack policy to MEMORY_ACK : packsize  Opcode volumeID  (Opcode = 9, long)
 * 
 * packsize : 2 bytes  
 * opcode : 2 bytes, volume id : 8 bytes, instanceID : 22 bytes
 * response code 0 : OK, 1 : fail, 2 : OK but not update to RS 
 */

void cmd_process_thread (void *data)
{
    int clientfd;
    char pkt_buf[MAX_CMD_PKTLEN], *buf_ptr, *dataBuf;
    unsigned short pktlen, opcode;
    int retCode;

    if (data == NULL) {
        syslog(LOG_ERR, "DMS WARN the NULL ptr of sock fd data\n");
        return;
    }

    clientfd = *((int *)data);

    while(1) {
        memset(pkt_buf, 0, sizeof(char)*MAX_CMD_PKTLEN);
        retCode = socket_rcv(clientfd, pkt_buf, 2);

        if (retCode < 0) {
            syslog(LOG_ERR, "DMSC WARN recv data error\n");
            break;
        }

        pktlen = get_short_int(pkt_buf);

        if (pktlen <= 0) {
            syslog(LOG_ERR, "DMSC WARN Receive 0 packet length %d\n", pktlen);
            break;
        }

        dataBuf = (char *)malloc_wrap((sizeof(char)*pktlen) + 1);
        if(!dataBuf){
            syslog(LOG_ERR, "DMS WARN retry malloc() fail! size = %zu\n",
                    (sizeof(int8_t)*pktlen) + 1);
            break;
        }    
        memset(dataBuf, 0, (sizeof(char)*pktlen) + 1);
        buf_ptr = dataBuf;
        socket_rcv(clientfd, buf_ptr, pktlen);
        opcode = get_short_int(buf_ptr);
        buf_ptr += sizeof(short);

        syslog(LOG_ERR, "Receive command with opcode %hu packet length %hu\n",
                opcode, pktlen);
        switch (opcode) {
        case DMS_CMD_RESET_OVERWRITTEN_FLAG:
            cmd_do_reset_ovw(clientfd, buf_ptr);
            break;
        case DMS_CMD_RESET_METADATA_CACHE:
            cmd_do_reset_cache(clientfd, buf_ptr);
            break;
        case DMS_CMD_ATTACH_VOLUME:
        case DMS_CMD_ATTACH_VOLUME_FOR_MIGRATION:
            cmd_do_attach_volume(clientfd, buf_ptr, pktlen);
            break;
        case DMS_CMD_DETACH_VOLUME:
        case DMS_CMD_DETACH_VOLUME_FOR_MIGRATION:
            cmd_do_detach_volume(clientfd, buf_ptr, pktlen);
            break;
        case DMS_CMD_CANCEL_ATTACH_OPERATION:
            cmd_do_cancel_op(clientfd, buf_ptr, pktlen);
            break;
        case DMS_CMD_BLOCK_VOLUME_IO:
        case DMS_CMD_UNBLOCK_VOLUME_IO:
            cmd_do_block_io(clientfd, buf_ptr, opcode);
            break;
        case DMS_CMD_SET_VOLUME_SHARE_FEATURE:
            cmd_do_set_vol_share(clientfd, buf_ptr);
            break;
        case DMS_CMD_STOP_VOL:
            cmd_do_stop_vol(clientfd, buf_ptr);
            break;
        case DMS_CMD_RESTART_VOL:
            cmd_do_restart_vol(clientfd, buf_ptr);
            break;
        case DMS_CMD_GET_CN_INFO:
            cmd_do_get_cn_info(clientfd, buf_ptr);
            break;
        case DMS_CMD_INVALIDATE_VOLUME_FREE_CHUNK:
            cmd_do_inval_vol_fs(clientfd, buf_ptr);
            break;
        case DMS_CMD_CLEANUP_VOLUME_FREE_CHUNK:
            cmd_do_clean_vol_fs(clientfd, buf_ptr);
            break;
        case DMS_CMD_FLUSH_VOLUME_FREE_CHUNK:
            cmd_do_flush_vol_fs(clientfd, buf_ptr);
            break;
        case DMS_CMD_PREALLOC:
            cmd_do_prealloc_cmd(clientfd, buf_ptr);
            break;
        default:
            syslog(LOG_ERR, "DMSC WARN Receive Unknown opcode %d\n", opcode);
            break;
        }
        free(dataBuf);
        continue;
    }

    close(clientfd);
    reset_client_fd_with_lock((int *)data);
}

static int initial_global_socket_fd(void)
{
    int i;

    for(i=0;i<MAX_NUM_OF_ACCEPT_SOCKET;i++) {
        global_client_fd[i] = -1;
    }

    return 0;
}

static int add_new_client_to_global_socket_fd (int new_socket_fd)
{
    int i, ret_index = -1;
    pthread_mutex_lock(&g_client_conn_mutex);
    for (i = 0; i < MAX_NUM_OF_ACCEPT_SOCKET; i++) {
        if(global_client_fd[i] == -1) {
            global_client_fd[i] = new_socket_fd;
            ret_index = i;
            break;
        }
    }
    pthread_mutex_unlock(&g_client_conn_mutex);

    return ret_index;
}

static int32_t _create_socket (void)
{
    struct sockaddr_in dest;
    int32_t ret, on, creat_ret;

    ret = 0;
    on = 0; //2016-05-03 Lego fix coverity issue CID: 37650

    /* create socket */
    dms_sk_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (dms_sk_server_fd < 0) {
        syslog(LOG_ERR,
                "DMSC ERROR: C Daemon fail create socket ret: %d\n",
                dms_sk_server_fd);
        return ret;
    }

    do {
        creat_ret = setsockopt(dms_sk_server_fd, SOL_SOCKET, SO_REUSEADDR,
                (int8_t *)&on, sizeof(on));

        if (creat_ret < 0) {
            syslog(LOG_ERR,
                    "DMSC ERROR: setsockopt return error : %d\n", ret);
        }

        /* initialize structure dest */
        bzero(&dest, sizeof(dest));
        dest.sin_family = AF_INET;
        dest.sin_port = htons(SERVER_PORT);

        /* this line is different from client */
        dest.sin_addr.s_addr = INADDR_ANY;

        /* assign a port number to socket */
        creat_ret = bind(dms_sk_server_fd, (struct sockaddr*)&dest, sizeof(dest));

        if (creat_ret < 0) {
            syslog(LOG_ERR,
                    "DMSC ERROR C Daemon fail bind port ret: %d\n", creat_ret);
            break;
        }

        initial_global_socket_fd();
        /* make it listen to socket with max 20 connections */
        creat_ret = listen(dms_sk_server_fd, MAX_NUM_OF_ACCEPT_SOCKET);

        if (creat_ret < 0) {
            syslog(LOG_ERR,
                    "DMSC ERROR C Daemon fail listen port return code: %d\n",
                    creat_ret);
            break;
        }

        ret = 1;
    } while (0);

    if (!ret) {
        close(dms_sk_server_fd);
        shutdown(dms_sk_server_fd, SHUT_RDWR);
        dms_sk_server_fd = -1;
    }

    return ret;
}

void create_socket_server (void)
{
    int32_t ret, max_cnt, retry_cnt;
    int32_t clientfd, index_of_global_fd, addrlen;
    struct sockaddr_in client_addr;
    pthread_attr_t attr;
    pthread_t id;

    max_cnt = 90;
    retry_cnt = 0;

    while (retry_cnt <= max_cnt) {
        if (_create_socket()) {
            break;
        }

        sleep(1);

        retry_cnt++;
    }
	
    if (retry_cnt >= max_cnt) {
        syslog(LOG_ERR, "DMSC ERROR fail to create socket server\n");
        return;
    }

    addrlen = sizeof(client_addr);
    while (1) {
        clientfd = 0;
        index_of_global_fd = -1;
        memset(&client_addr, 0, sizeof(struct sockaddr));
		
        /* Wait and accept connection */
        clientfd = accept(dms_sk_server_fd, (struct sockaddr*)&client_addr,
                &addrlen);

        if (clientfd < 0) {
            syslog(LOG_ERR, "DMSC WARN fail to accept new conn %d\n", clientfd);
            continue;
        }

        index_of_global_fd = add_new_client_to_global_socket_fd(clientfd);

        if (index_of_global_fd == -1) {
            syslog(LOG_ERR, "DMSC WARN fail find empty slot client, "
                    "close it and shutdown\n");
            if (clientfd >= 0) { //Lego 20130821 : Fix Corverity 31378
                close(clientfd);
            }

            continue;
        }

        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        ret = pthread_create(&id, &attr, (void*)cmd_process_thread,
                (void *)(&(global_client_fd[index_of_global_fd])));

        if (ret != 0) {
            syslog(LOG_ERR, "DMSC WARN Socket server create pthread error!\n");
            close(clientfd);
        }

        pthread_attr_destroy(&attr);
    }
}

void stop_socket_server (void)
{
    shutdown(dms_sk_server_fd, SHUT_RDWR);
    close(dms_sk_server_fd);
}
