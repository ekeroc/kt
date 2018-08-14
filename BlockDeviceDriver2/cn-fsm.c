/*
 * cn-fsm.c
 *
 *  Created on: 2014/5/7
 *      Author: bennett
 */

#include <linux/init.h>
#include <linux/module.h>

#include <linux/kthread.h>

#include <linux/jiffies.h>

#include <linux/socket.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <net/sock.h>

//#include "sock_client.h"

static const char *HOST = "192.168.95.1";
static const uint16_t PORT = 44555;
static const int HEADER_LEN = 32;


typedef enum {
    ACCEPT_ACK,
    REGISTER_REQ,
    HEARTBEAT_REQ,
    MAX
} op_code_t;

typedef enum {
    NOT_CONNECT,
    REGISTER,
    NORMAL,
    IDLE_TEST
} fsm_state_t;

typedef struct header_packet {
    uint8_t magic[8];
    uint16_t opcode;
    uint8_t seq_id;
    uint32_t content_size;
    uint32_t checksum;
    uint8_t reserved[13];
} hdr_t;

static const int SIZE_HDR_T = sizeof(hdr_t);
static hdr_t wr_hdr;
static hdr_t rd_hdr;

static struct task_struct *fsm_task_st;
static int32_t fsm_data;

static void init_hdr (void)
{
    memset(&wr_hdr, 0, SIZE_HDR_T);
    memset(&rd_hdr, 0, SIZE_HDR_T);
    strncpy(&wr_hdr.magic[1], "ccmablk", 7);
    strncpy(&rd_hdr.magic[1], "ccmablk", 7);
}

static void sleep_sec (uint32_t s)
{
    signed long timeout;
    do {
        set_current_state(TASK_INTERRUPTIBLE);
        timeout = schedule_timeout((signed long)s * HZ);
        // if timeout > 0, this may get some interrupt signal. 
        // and we continue to sleep.
    } while(timeout > 0);
    return;
}


static int receive (struct socket *sock, const char *rd_buf,
                                                         int rd_size)
{
    struct msghdr msg;
    struct kvec vec;
    void *iov_base;
    size_t iov_len;
    unsigned long rd_timeout;
    int r;

    memset(&vec, 0, sizeof(vec));
    memset(&msg, 0, sizeof(msg));
    sock->sk->sk_allocation = GFP_NOIO;
    msg.msg_flags = MSG_NOSIGNAL;

    iov_base = (void *)(&rd_buf[0]);
    iov_len = (size_t)rd_size;

    r = 0;
    rd_timeout = jiffies + HZ * 60; // 60 sec.
    while(r == 0) {
        vec.iov_base = iov_base;
        vec.iov_len = iov_len;
        r = kernel_recvmsg(sock, &msg, &vec, 1, iov_len,
                                                       MSG_DONTWAIT);
        if(r > 0) {
            iov_base = iov_base + r;
            iov_len = iov_len - r;
            r = 0;
            // reset timeout.
            rd_timeout = jiffies + HZ * 60; // 60 sec.
            if(iov_len == 0) {
                break; //// normal exit.
            } else if(iov_len < 0) {
                r = -4; //// need debug.
                break;
            }
        } else if(r == 0) {
            //// could loss connection.
            printk("could loss connection\n");
            r = -1; // loss connection.
        } else {
            switch(r) {
            case -EAGAIN: // -11
                r = 0; // do it again.
                sleep_sec(1);
                break;
            case -ECONNRESET: // -104
                printk("ECONNRESET: Connection reset by peer.\n");
                r = -2;
                break;
            default:
                printk("r = %d, unknown return code.\n", r);
                r = -2; // unknown error.
                break;
            }
        }

        if(r == 0) {
            if(time_before(jiffies, rd_timeout)) {

            } else {
                r = -3; // timeout.
                printk("read timeout...\n");
            }
        }
    }
    return r;
}
static int send (struct socket *sock, const char *wr_buf,
                                                         int wr_size)
{
    int result;
    struct msghdr msg;
    struct kvec iov;
    sigset_t blocked, oldset;
    void *iov_base;
    size_t iov_len;

    iov_base = (void *)(&wr_buf[0]);
    iov_len = (size_t)wr_size;

    /* Allow interception of SIGKILL only
     * Don't allow other signals to interrupt the transmission */
    siginitsetinv(&blocked, sigmask(SIGKILL));
    sigprocmask(SIG_SETMASK, &blocked, &oldset);

    memset(&iov, 0, sizeof(iov));
    memset(&msg, 0, sizeof(msg));
    sock->sk->sk_allocation = GFP_NOIO;
    msg.msg_flags = MSG_NOSIGNAL;

    do {
        iov.iov_base = iov_base;
        iov.iov_len = iov_len;


        /*struct timer_list ti;
        if (lo->xmit_timeout) {
					init_timer(&ti);
					ti.function = nbd_xmit_timeout;
					ti.data = (unsigned long)current;
					ti.expires = jiffies + lo->xmit_timeout;
					add_timer(&ti);
		}*/

        result = kernel_sendmsg(sock, &msg, &iov, 1, iov_len);

				/*if (lo->xmit_timeout)
					del_timer_sync(&ti);*/


/*			if (signal_pending(current)) {
				siginfo_t info;
				printk(KERN_WARNING "nbd (pid %d: %s) got signal %d\n",
					task_pid_nr(current), current->comm,
					dequeue_signal_lock(current, &current->blocked, &info));
				result = -EINTR;
				sock_shutdown(lo, !send);
				break;
			}*/

        if(result <= 0) {
            if(result == 0) {
                result = -EPIPE; /* short read */
            }
            break;
        }
        iov_len -= result;
        iov_base += result;
    }while(iov_len > 0);

    sigprocmask(SIG_SETMASK, &oldset, NULL);

    return result; // <0, if fail; >0, if correct; =0, impossible.
}

static int decode_and_update (char *rbuf)
{
    int r, i;
    uint8_t magic[8];
    uint16_t opcode;
    uint8_t seqid;
    uint32_t content_size;
    uint32_t checksum;
    uint8_t reserved[13];

    memcpy(&magic[0], &rbuf[0], 8);
    memcpy(&opcode, &(rbuf[8]), 2);
    opcode  = ntohs(opcode);
    memcpy(&seqid, &(rbuf[10]), 1);
    memcpy(&content_size, &(rbuf[11]), 4);
    content_size = ntohl(content_size);
    memcpy(&checksum, &(rbuf[15]), 4);
    memcpy(&reserved[0], &(rbuf[19]), 13);

    r = memcmp(&magic[0], &rd_hdr.magic[0], 8);
    if(r != 0) {
        printk("memcmp ret = r= %d\n", r);
        for(i=0; i<8; i++) {
            printk("%d: %d, %d\n", i, magic[i], rd_hdr.magic[i]);
        }
        return -1;
    }

    if(opcode != rd_hdr.opcode) { // expected opcode
        printk("opcode: %d, %d\n", opcode, rd_hdr.opcode);
        return -2;
    }

    if(seqid != rd_hdr.seq_id) { // expected seq_id
        printk("seqid: %d, %d\n", seqid, rd_hdr.seq_id);
        return -3;
    }

    //// update current seq_id
    if(rd_hdr.seq_id == 255) {
        rd_hdr.seq_id = 0;
    } else {
        rd_hdr.seq_id++;
    }

    //// update size, checksum.
    rd_hdr.content_size = content_size;
    rd_hdr.checksum = checksum;

    memcpy(&rd_hdr.reserved[0], &reserved[0], 13);

    return 0;
}

static int do_connect (struct socket **sock_ptr)
{
    int r, i;
    struct socket *client_sock = NULL;
    struct sockaddr_in s_addr;
    char rbuf[HEADER_LEN];
    
    //// create a socket
    r = sock_create_kern(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                                                       &client_sock);
    if(r < 0) {
        printk("client:socket create error! r=%d\n", r);
        r = -1;
        goto CONNECT_ERR;
    }
    printk("client: socket create ok!\n");

    memset(&s_addr, 0, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(PORT);
    s_addr.sin_addr.s_addr = in_aton(HOST);

    r = kernel_connect(client_sock, (struct sockaddr *)(&s_addr),
                                                  sizeof(s_addr), 0);
    if(r != 0) {
        if(r == -ECONNREFUSED) {
            printk("Connection refused.\n");
        } else {
            printk("client:connect error! ==> r=%d\n", r);
        }

        r = -2;
        goto CONNECT_ERR;
    }
    printk("client:connect ok!\n");

    /* every time, connect successfully, we reset seq_id */
    rd_hdr.seq_id = wr_hdr.seq_id = 0;

    //// read header from server ...
    memset(&rbuf[0], 0, sizeof(rbuf));

    r = receive(client_sock, &rbuf[0], HEADER_LEN);
    if(r < 0) {
        printk("receive error.\n");
        r = -3;
        goto CONNECT_ERR;
    }



    printk("rbuf= ");
    for(i=0; i<HEADER_LEN; i++){
        printk("%d ", rbuf[i]);
    }
    printk("\n");
    /* recvbuf= 0 99 99 109 97 98 108 107 0 1 0 0 0
     * 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 */

    rd_hdr.opcode = ACCEPT_ACK;
    r = decode_and_update(rbuf);
    if(r < 0) {
        r = -4;
        goto CONNECT_ERR;
    }
    if(rd_hdr.opcode != ACCEPT_ACK) {
        r = -4;
        goto CONNECT_ERR;
    }



    *sock_ptr = client_sock;
    return 0;


CONNECT_ERR:
    switch(r) {
    case -4:
    case -3:
    case -2:
    case -1:
        break;
    default:
        printk("why r=%d\n", r);
        break;
    }

    sleep_sec(1);
    if(client_sock != NULL) {
        printk("release sock...\n");
        sock_release(client_sock);
        client_sock = NULL;
    }

    *sock_ptr = NULL;
    return r;
}



static int do_register (struct socket *sock)
{
    int r;
    char rbuf[HEADER_LEN];
    uint32_t content_size;
    ////char *content;

    wr_hdr.opcode = REGISTER_REQ;
    wr_hdr.opcode = htons(wr_hdr.opcode);
    wr_hdr.seq_id = rd_hdr.seq_id;
    /* currently, we dont have any content to register. */
    wr_hdr.content_size = 0;
    wr_hdr.checksum = 0;

    r = send(sock, (char *)(&wr_hdr), HEADER_LEN);
    if(r <= 0) {
        printk("send return= %d\n", r);
        return -1;
    }

    /* dont have content to send NOW. */

    r = receive(sock, &rbuf[0], HEADER_LEN);
    if(r < 0) {
        printk("receive error.\n");
        return -2;
    }

    rd_hdr.opcode = REGISTER_REQ;
    r = decode_and_update(&rbuf[0]);
    if(r < 0) {
        printk("decode fail.\n");
        return -3;
    }

    content_size = rd_hdr.content_size;
    if(content_size > HEADER_LEN) {
        printk("not implemented now...\n");
    }
    r = receive(sock, &rbuf[0], content_size);
    if(r < 0) {
        printk("receive error.\n");
        return -4;
    }

    /* content only 1 bytes, means rr on/off */
    if(rbuf[0] == 1) {
        // turn rr on.
        // TODO::::
        printk("rr = on\n");
    } else if(rbuf[0] == 0) {
        // turn rr off.
        // TODO::::
        printk("rr = off\n");
    } else if(rbuf[0] == 2) {
        // register again.
        printk("rr = again.\n");
        return EAGAIN;
    }

    return 0;
}
static int do_heartbeat (struct socket *sock)
{
    int r;
    char rbuf[HEADER_LEN];

    printk("do_heartbeat...\n");

    wr_hdr.opcode = HEARTBEAT_REQ;
    wr_hdr.opcode = htons(wr_hdr.opcode);
    wr_hdr.seq_id = rd_hdr.seq_id;
    /* currently, we dont have any content to register. */
    wr_hdr.content_size = 0;
    wr_hdr.checksum = 0;

    r = send(sock, (char *)(&wr_hdr), HEADER_LEN);
    if(r <= 0) {
        printk("send return= %d\n", r);
        return -1;
    }

    /* dont have content to send NOW. */

    r = receive(sock, &rbuf[0], HEADER_LEN);
    if(r < 0) {
        printk("receive error.\n");
        return -2;
    }

    rd_hdr.opcode = HEARTBEAT_REQ;
    r = decode_and_update(&rbuf[0]);
    if(r < 0) {
        printk("decode fail.\n");
        return -3;
    }


    return 0;
}

static void close_sock (struct socket *sock)
{
    if(sock != NULL) {
        //// kernel_sock_shutdown? TODO:
        sock_release(sock);
        sock = NULL; // useless.
    }
}

static int fsm_thread (void *arg)
{
    int32_t *input = (int32_t *)arg;
    fsm_state_t state = NOT_CONNECT, next_state = NOT_CONNECT;
    struct socket *sock = NULL;
    int r;
    uint64_t loop_count = 0;

    printk("fsm_thread start. input= %d\n", *input);

    while(!kthread_should_stop()) {

        switch(state) {
        case NOT_CONNECT:
            r = do_connect(&sock);
            if(r == 0) {
                next_state = REGISTER;
            } else {
                printk("do_connect(), r=%d\n", r);
                next_state = NOT_CONNECT;
            }
            break;
        case REGISTER:
            r = do_register(sock);
            if(r == 0) {
                next_state = NORMAL;
            } else if(r < 0) {
                printk("do_register() r=%d\n", r);
                next_state = NOT_CONNECT;
                sleep_sec(1);
                close_sock(sock);
                sock = NULL;
            } else {
                printk("do_register() r=%d\n", r);
                if(r == EAGAIN) {
                    //// do it again.
                    printk("try register again...\n");
                    sleep_sec(3);
                } else {
                    // impossible here.
                }
            }
            break;
        case NORMAL:
            r = do_heartbeat(sock);
            if(r == 0) {
                next_state = NORMAL;
            } else {
                printk("do_heartbeat, r=%d\n", r);
                next_state = NOT_CONNECT;
                sleep_sec(1);
                close_sock(sock);
                sock = NULL;
            }

            break;
        case IDLE_TEST:
            break;
        default:
            printk("state= %d\n", state);
            break;
        }

        state = next_state;

        sleep_sec(9);

        loop_count++;
        printk("loop_count=%llu\n", loop_count);
    }


    close_sock(sock);
    sock = NULL;


    printk("fsm_thread stop.\n");
    return 0;
}


static int ccmablk_fsm_init (void)
{
    int r;

    init_hdr();

    fsm_task_st = kthread_create(fsm_thread, &fsm_data, "fsm_thread");
    if(IS_ERR(fsm_task_st)) {
        r = PTR_ERR(fsm_task_st);
        fsm_task_st = NULL;
        return r;
    }
    wake_up_process(fsm_task_st);

    return 0;
}
static void ccmablk_fsm_exit (void)
{
    kthread_stop(fsm_task_st);
}

static int __init mod_init (void)
{
    int r;

    printk("mod_init() start.\n");

    r = ccmablk_fsm_init();
    if(r != 0) {
        goto ERR;
    }

    printk("mod_init() stop.\n");
    return 0;

ERR:
    printk("mod_init() stop. err= %d\n", r);
    return -1;
}

static void __exit mod_exit (void)
{
    printk("mod_exit() start.\n");

    ccmablk_fsm_exit();

    printk("mod_exit() stop.\n");
}


module_init(mod_init);
module_exit(mod_exit);


MODULE_LICENSE("GPL");





