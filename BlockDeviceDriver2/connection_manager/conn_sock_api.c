/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * conn_sock_api.c
 *
 */
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/net.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../common/dms_client_mm.h"
#include "../common/thread_manager.h"
#include "conn_manager_export.h"
#include "conn_state_fsm.h"
#include "conn_manager.h"
#include "conn_sock_api.h"
#include "../config/dmsc_config.h"

/*
 * Description: convert ipv4 address string into int32_t
 * Return:     0 : convert ok
 *         non 0 : convert fail
 */
static int32_t _inet_pton (int8_t *cp, void *dst)
{
    int32_t value, digit;
    int8_t temp, bytes[4];
    int8_t *end = bytes;
    static const int32_t addr_class_max[4] = {0xffffffff, 0xffffff,
                                              0xffff, 0xff};

    memset(bytes, 0, sizeof(int8_t)*4);

    temp = *cp;
    while (true) {
        if (!isdigit(temp)) {
            return -EINVAL;
        }

        value = 0;
        digit = 0;

        for (;;) {
            if (isascii(temp) && isdigit(temp)) {
                value = (value * 10) + temp - '0';
                temp = *++cp;
                digit = 1;
            } else {
                break;
            }
        }

        if (temp == '.') {
            if ((end > bytes + 2) || (value > 255)) {
                return -EINVAL;
            }

            *end++ = value;
            temp = *++cp;
        } else if (temp == ':') {
            // cFYI(1,("IPv6 addresses not supported for CIFS mounts yet"));
            return -EINVAL;
        } else {
            break;
        }
    }

    /* check for last characters */
    if (temp != '\0' && (!isascii(temp) || !isspace(temp))) {
        if (temp != '\\') {
            if (temp != '/') {
                return 0;
            } else {
                (*cp = '\\');    /* switch the slash the expected way */
            }
        }
    }

    if (value > addr_class_max[end - bytes]) {
        return -EINVAL;
    }

    *((__be32 *) dst) = *((__be32 *)bytes) | htonl(value);

    return 0; /* success */
}

/*
 * NOTE: caller make sure servaddr not NULL
 */
static int32_t set_sockaddr (struct sockaddr_in *servaddr, uint32_t ipaddr,
        int32_t port)
{
    int8_t addr[IPV4ADDR_NM_LEN];
    int32_t retcode;

    retcode = 0;

    memset(servaddr, 0, sizeof(struct sockaddr_in));

    servaddr->sin_family = AF_INET;
    servaddr->sin_port = htons(port);
    ccma_inet_aton(ipaddr, addr, IPV4ADDR_NM_LEN);

    //translate string to inet_addr
    retcode = _inet_pton(addr, &(servaddr->sin_addr));
    if (retcode) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN ip address translate Fail!: %s:%d\n", addr, port);
    }

    return retcode;
}

/* enable alive */
static void set_tcp_alive_timer (struct socket *s)
{
    int32_t retval, flag;

    flag = 1;
    retval = sock_setsockopt(s, IPPROTO_TCP, SO_KEEPALIVE, (char __user *)&flag,
                              sizeof(flag));
    if (retval < 0) {
        dms_printk(LOG_LVL_WARN,
                "DMS WARN Couldn't setsockopt(SO_KEEPALIVE), "
                "retval: %d flag=%ld\n",
                retval, s->sk->sk_flags);
    }

    flag = 5; // start send probing packet after idle for 5 seconds
    retval = s->ops->setsockopt(s, IPPROTO_TCP, TCP_KEEPIDLE,
                                 (char __user *)&flag, sizeof(flag));

    if (retval < 0) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN Couldn't setsockopt(TCP_KEEPIDLE), "
                "retval: %d val=%ds\n",
                retval, ((struct tcp_sock *)s->sk)->keepalive_time / HZ);
    }

    flag = 1; // probing per 1 second
    retval = s->ops->setsockopt(s, IPPROTO_TCP, TCP_KEEPINTVL,
                                 (char __user *)&flag, sizeof(flag));

    if (retval < 0) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN Couldn't setsockopt(TCP_KEEPINTVL), "
                "retval: %d val=%ds\n",
                retval, ((struct tcp_sock *)s->sk)->keepalive_intvl / HZ);
    }

    flag = 5; // if sequentiall probing faii for 5 times... then close connection
    retval = s->ops->setsockopt(s, IPPROTO_TCP, TCP_KEEPCNT,
                                 (char __user *)&flag, sizeof(flag));

    if (retval < 0) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN Couldn't setsockopt(TCP_KEEPCNT), "
                "retval: %d cnt=%d\n",
                retval, ((struct tcp_sock *)s->sk)->keepalive_probes);
    }
}

static int32_t optimize_tcp_conn (struct socket *s)
{
    struct timeval tv_timeo;
    mm_segment_t oldfs = {0};
    int32_t retval, flag;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37)
    int32_t usr_timeout;
#endif

    if (unlikely(IS_ERR_OR_NULL(s))) {
        dms_printk(LOG_LVL_WARN, ""
                "DMSC WARN wrong para when set linux sock opt\n");
        DMS_WARN_ON(true);
        return -1;
    }

    retval = -1;
    flag = 0;

    oldfs = get_fs();
    set_fs(KERNEL_DS);

    /* Disable the Nagle (TCP No Delay) algorithm */
    flag = 1;
    retval = s->ops->setsockopt(s, IPPROTO_TCP/*SOL_TCP*/, TCP_NODELAY,
                                 (char __user *)&flag, sizeof(flag));
    if (retval < 0) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WANR Couldn't setsockopt(TCP_NODELAY), "
                "retval: %d\n", retval);
    }

    /* set cork to 0*/
    flag = 0;
    retval = s->ops->setsockopt(s, IPPROTO_TCP/*SOL_TCP*/, TCP_CORK,
                                 (char __user *)&flag, sizeof(flag));

    if (retval < 0) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN Couldn't setsockopt(TCP_CORK), "
                "retval: %d\n", retval);
    }

    tv_timeo.tv_sec = SOCK_TIME_SO_SNDTIMEO;
    tv_timeo.tv_usec = 0;
    sock_setsockopt(s, IPPROTO_TCP, SO_SNDTIMEO, (char __user *)&tv_timeo,
                     sizeof(struct timeval));

    tv_timeo.tv_sec = SOCK_TIME_SO_RCVTIMEO;
    tv_timeo.tv_usec = 0;
    sock_setsockopt(s, IPPROTO_TCP, SO_RCVTIMEO, (char __user *)&tv_timeo,
                     sizeof(struct timeval));

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37)
    usr_timeout = SOCK_TIME_USER_TIMEOUT;
    sock_setsockopt(s, IPPROTO_TCP, TCP_USER_TIMEOUT, (char __user *)&usr_timeout,
            sizeof(usr_timeout));
#endif

    set_tcp_alive_timer(s);

    set_fs(oldfs);

    return retval;
}

cs_wrapper_t *alloc_sock_wrapper (uint32_t ipaddr, int32_t port)
{
    int8_t hname[IPV4ADDR_NM_LEN];
    cs_wrapper_t *sk_wapper;
    int32_t retval = 0;

    sk_wapper = discoC_mem_alloc(sizeof(cs_wrapper_t), GFP_NOWAIT | GFP_ATOMIC);
    if (unlikely(IS_ERR_OR_NULL(sk_wapper))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN alloc mem fail when create linux sock\n");
        DMS_WARN_ON(true);
        return NULL;
    }

    atomic_set(&sk_wapper->sock_ref_cnt, 0);
    retval = sock_create(AF_INET, SOCK_STREAM, IPPROTO_TCP, &(sk_wapper->sock));
    if (retval < 0) {
        ccma_inet_aton(ipaddr, hname, IPV4ADDR_NM_LEN);
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN create linux sock err: %d %s:%d\n", retval, hname, port);
        discoC_mem_free(sk_wapper);
        sk_wapper = NULL;
    }

    return sk_wapper;
}

static void sock_error_report (struct sock *sk)
{
    dms_printk(LOG_LVL_INFO, "DMSC INFO linux sock error report state %d\n",
            sk->sk_err);
    read_lock(&sk->sk_callback_lock);
    //NOTE: add your handling code here
    read_unlock(&sk->sk_callback_lock);
}

/*
 * enum {
        TCP_ESTABLISHED = 1,
        TCP_SYN_SENT,
        TCP_SYN_RECV,
        TCP_FIN_WAIT1,
        TCP_FIN_WAIT2,
        TCP_TIME_WAIT,
        TCP_CLOSE,
        TCP_CLOSE_WAIT,
        TCP_LAST_ACK,
        TCP_LISTEN,
        TCP_CLOSING,    // Now a valid state

        TCP_MAX_STATES  // Leave at the end!
 * */
static void sock_state_change (struct sock *sk)
{
    discoC_conn_t *goal;

    dms_printk(LOG_LVL_INFO,
            "DMSC INFO linux sock error report state change "
            "state:%d thread:%s in_interrupt: %lu\n",
            sk->sk_state, current->comm, in_interrupt());

    read_lock(&sk->sk_callback_lock);

    goal = (discoC_conn_t *)sk->sk_user_data;
    if (IS_ERR_OR_NULL(goal)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN sock state change sk_usr_data is NULL\n");
        DMS_WARN_ON(true);
        goto out;
    }

    dms_printk(LOG_LVL_INFO, "DMSC INFO sk state change %s:%d\n",
            goal->ipv4addr_str, goal->port);

    switch (sk->sk_state) {
        case TCP_SYN_SENT: // ===> 2
            dms_printk(LOG_LVL_INFO, "DMSC INFO TCP_SYN_SEND\n");
            break;

        case TCP_SYN_RECV: // ===> 3
            dms_printk(LOG_LVL_INFO, "DMSC TCP_SYN_RECV\n");
            break;

        case TCP_ESTABLISHED: // ===> 1
            dms_printk(LOG_LVL_INFO, "DMSC established.\n");
            break;

        default:
            dms_printk(LOG_LVL_INFO, "DMSC %s:%d %d SHUTDOWN\n",
                    goal->ipv4addr_str, goal->port, sk->sk_state);

            if (!IS_ERR_OR_NULL(goal->rx_error_handler)) {
                goal->rx_error_handler(goal);
            }
            break;
    }

out:
    read_unlock(&sk->sk_callback_lock);
}

void free_sock_wrapper (cs_wrapper_t *sk_wrapper)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25)
    kernel_sock_shutdown(sk_wrapper->sock, SHUT_RDWR);
#endif
    sock_release(sk_wrapper->sock);
    sk_wrapper->sock = NULL;
    discoC_mem_free(sk_wrapper);
    sk_wrapper = NULL;
}

int32_t sock_connect_target (uint32_t ipaddr, int32_t port,
        struct socket *sock)
{
    int8_t hname[IPV4ADDR_NM_LEN];
    struct sockaddr_in servaddr;
    int32_t retcode;

    retcode = 0;
    if (unlikely(IS_ERR_OR_NULL(sock))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN wrong para for conn to sk server\n");
        DMS_WARN_ON(true);
        return -EINVAL;
    }

    if (set_sockaddr(&servaddr, ipaddr, port)) {
        return -EINVAL;
    }

    optimize_tcp_conn(sock);

    retcode = sock->ops->connect(sock, (struct sockaddr *)&servaddr,
                                      sizeof (servaddr), 0);
    if (retcode == 0) {
        //success
        sock->sk->sk_error_report = sock_error_report;
        sock->sk->sk_state_change = sock_state_change;
    }
    /*
     * NOTE: if we using the same sock to connect, we should take care
     *       return code of connect
     *       retcode == -EISCONN
     *       retcode == -EALREADY
     *
     *       Original design is alloc socket wrapper at add_retry_task
     *       And using work to connect every 5 seconds without free socket wrapper
     */

    //-113: -EHOSTUNREACH, No route to host (physical network cable disconnect)
    ccma_inet_aton(ipaddr, hname, IPV4ADDR_NM_LEN);
    dms_printk(LOG_LVL_DEBUG,
            "DMSC DEBUG conn retcode = %d %s:%d\n", retcode, hname, port);

    return retcode;
}

/*
 * Description:
 *     allocate socket wrapper and try connect to target
 *     if ok, return socket wrapper
 *     if fail, return NULL
 */
cs_wrapper_t *connect_target (uint32_t ipv4addr, int32_t target_port)
{
    int8_t hname[IPV4ADDR_NM_LEN];
    cs_wrapper_t *sk_wapper;
    struct socket *sock;
    int32_t retval, retry_counter;

    sk_wapper = alloc_sock_wrapper(ipv4addr, target_port);
    ccma_inet_aton(ipv4addr, hname, IPV4ADDR_NM_LEN);
    if (unlikely(IS_ERR_OR_NULL(sk_wapper))) {
        dms_printk(LOG_LVL_WARN, "DMSC fail to alloc socket wapper for %s:%d\n",
                hname, target_port);
        return NULL;
    }

    if (unlikely((discoC_MODE_SelfTest ==
            dms_client_config->clientnode_running_mode) ||
            (discoC_MODE_SelfTest_NN_Only ==
            dms_client_config->clientnode_running_mode))) {
        return sk_wapper;
    }

    retry_counter = 0;
    dms_printk(LOG_LVL_INFO, "DMSC INFO try to connect to target %s %d\n",
            hname, target_port);

    sock = sk_wapper->sock;

retry:
    retval = sock_connect_target(ipv4addr, target_port, sock);
    if (retval) {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO conn retry build fail %s:%d again...\n",
                hname, target_port);

        retry_counter++;

        if (retry_counter < SOCK_CONN_CONNECT_MAX_RETRY) {
            msleep(SOCK_CONN_CONNECT_TIMEOUT);
            goto retry;
        }

        dms_printk(LOG_LVL_INFO,
                "DMSC INFO conn retry build fail %s:%d end...\n",
                hname, target_port);
        free_sock_wrapper(sk_wapper);
        sock = NULL;
        sk_wapper = NULL;
    }

    return sk_wapper;
}

/*
 * decode common network errno values into more useful strings.
 * strerror would be nice right about now.
 */
static int8_t *sock_errormsg (int32_t errno)
{
    switch (errno) {
        case 0:
            return "connection close";

        case -EIO:
            return "I/O error";

        case -EINTR:
            return "Interrupted system call";

        case -ENXIO:
            return "No such device or address";

        case -EFAULT:
            return "Bad address";

        case -EBUSY:
            return "Device or resource busy";

        case -EINVAL:
            return "Invalid argument";

        case -EPIPE:
            return "Broken pipe";

        case -ENONET:
            return "Machine is not on the network";

        case -ECOMM:
            return "Communication error on send";

        case -EPROTO:
            return "Protocol error";

        case -ENOTUNIQ:
            return "Name not unique on network";

        case -ENOTSOCK:
            return "Socket operation on non-socket";

        case -ENETDOWN:
            return "Network is down";

        case -ENETUNREACH:
            return "Network is unreachable";

        case -ENETRESET:
            return "Network dropped connection because of reset";

        case -ECONNABORTED:
            return "Software caused connection abort";

        case -ECONNRESET:
            return "Connection reset by peer";

        case -EISCONN:
            return "Transport endpoint is already connected";

        case -ESHUTDOWN:
            return "Cannot send after shutdown";

        case -ETIMEDOUT:
            return "Connection timed out";

        case -ECONNREFUSED:
            return "Connection refused";

        case -EHOSTDOWN:
            return "Host is down";

        case -EHOSTUNREACH:
            return "No route to host";

        case -ECP_INVAL:
            return "The state of CPE is invalid";

        case -ECP_SNDTIMEO:
            return  "Socket send time out!";

        case -ECP_RECVTIMEO:
            return "Socket receive time out!";

        default:
            return "Unknow error code";
    }
}

//NOTE: caller guarantee buf is not NULL
int32_t sock_tx_send (struct socket *sock, void *buf, size_t len)
{
    struct msghdr msg;
    struct iovec iov;
    unsigned long ret_msleep;
    void *buffer_ptr;
    mm_segment_t oldfs;
    int32_t done, cnt_retry, total_done;

    if (unlikely(IS_ERR_OR_NULL(sock))) {
        dms_printk(LOG_LVL_WARN, "DMSC WANR null para ptr sock tx send\n");
        return -EINVAL;
    }

    done = 0;
    cnt_retry = 0;
    total_done = 0;
    buffer_ptr = buf;
    ret_msleep = 0;

sock_xmit_retry:
    memset(&msg, 0, sizeof(struct msghdr));
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_name = 0;
    msg.msg_namelen = 0;
    msg.msg_flags = MSG_WAITALL;

    oldfs = get_fs();
    set_fs(KERNEL_DS);
    iov.iov_len = len;
    iov.iov_base = buffer_ptr;

    DMSC_iov_iter_init(msg, WRITE, &iov, 1, len);

    sock->sk->sk_allocation = GFP_NOIO;
    done = DMSC_sock_sendmsg(sock, &msg, len);

    set_fs(oldfs);

    if (done > 0) {
        total_done = total_done + done;

        if (done != len) {
            cnt_retry++;
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN sock_sendmsg size mismatch, "
                    "expect = %d actual = %d retry again "
                    "(retry cnt=%d MAX_RETRY=%d)\n",
                    (uint32_t)len, done, cnt_retry, SOCK_TX_TIMEOUT_MAX_RETRY);

            if (cnt_retry <= SOCK_TX_TIMEOUT_MAX_RETRY) {
                ret_msleep =
                        msleep_interruptible(SOCK_TX_TIMEOUT_WAIT_TIME);

                if (ret_msleep != 0) {
                    msleep(ret_msleep);
                    ret_msleep = 0;
                }

                buffer_ptr = buffer_ptr + done;
                len = len - done;
                goto sock_xmit_retry;
            }

            done = -ECP_SNDTIMEO;
        }
    }

    if (done <= 0) {
        //translate send timeout (EAGAIN) to ECP_SNDTIMEO
        if (done == -EAGAIN) {
            //TODO: Maybe the best way to do is retry again
            done = -ECP_SNDTIMEO;
        }

        total_done = done;
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN socket send ERROR! retval = %d, %s\n",
                done, sock_errormsg(done));
    }

    return total_done;
}

int32_t sock_rx_rcv (struct socket *sock, void *buf, int32_t len)
{
    struct msghdr msg;
    struct iovec iov;
    mm_segment_t oldfs;
    int32_t size, r;

    if (len <= 0) {
        dms_printk(LOG_LVL_WARN, "DMS WARN wrong length for sock rcv %d\n", len);
        DMS_WARN_ON(true);
        return -EINVAL;
    }

    if (IS_ERR_OR_NULL(sock) || IS_ERR_OR_NULL(buf)) {
        dms_printk(LOG_LVL_WARN, "DMSC WANR null para ptr sock rcv\n");
        return -EINVAL;
    }

    size = 0;
    do {
        msg.msg_control = NULL;
        msg.msg_name = NULL;
        msg.msg_namelen = 0;
        msg.msg_flags = MSG_WAITALL;

        oldfs = get_fs();
        set_fs(KERNEL_DS);
        iov.iov_base = (void *)(buf + size);
        iov.iov_len = (__kernel_size_t)(len - size);
        sock->sk->sk_allocation = GFP_ATOMIC;

        DMSC_iov_iter_init(msg, READ, &iov, 1, (__kernel_size_t)(len - size));

        do {
            r = DMSC_sock_recvmsg(sock, &msg, len - size, 0);

            //TODO: We should use kernel_sock_shutdown to close socket
            //      so that got error number here and avoid using kthread_should_stop
            if (kthread_should_stop()) {
                dms_printk(LOG_LVL_INFO,
                        "DMSC INFO someone stop the thread "
                        "give up this connection\n");
                r = -EPIPE;
                //NOTE: if we didn't close conn and stop threads, we got -EAGAIN
                break;
            }

            //NOTE: r = 0 connection close, < 0 error, > 0 how many byte rcv
            //      when conn reset, sock_recvmsg return 0
        } while ((r == -EAGAIN) || (r == -ETIMEDOUT));

        set_fs(oldfs);

        if (r <= 0) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN sock_recvmsg error: %d, %s\n", r, sock_errormsg(r));
            return -EPIPE;
        }

        size += r;
    } while (size != len);

    return size;
}
