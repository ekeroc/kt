/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * mclient.c
 *
 * Description: CDaemon run at computer node.
 * Respobsility as following:
 * 1. Load block driver automatically. Remove block driver when receive
 *    control-c signal
 * 2. Run as a socket server to accept commands from Namenode and VMM
 *    2.1 Command: reset overwritten flag from Namenode
 *    2.2 Command: attach volume
 *    2.3 Command: detach volume
 * 3. update attach / detach information to RS and load information at startup. 
 * 4. Pass attach / detach / reset overwritten flag commands to driver
 * 5. Automatically attach / detach at startup / shutdown
 * 6. maintain the mapping of VolumeID InstanceID DeviceFile:
 *    7.1 If user attach the same instance twice, we can return the file
 *        immedidatly
 *    7.2 Automatically attach when startup
 *    7.3 Automatically detach when shutdown  
 *
 * Design Note
 * 1. One major thread running:
 *    1.1 Main thread: accept commands
 * 2. work thread
 *    2.1 create at runtime
 *    2.2 update RS
 *    2.3 handle command
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <pthread.h>
#include "../common/discoC_sys_def.h"
#include "../config/dmsc_config.h"
#include "../common/common.h"
#include "mclient_private.h"
#include "mclient.h"
#include "ipcsocket.h"

//TODO: 201505 show which connection broken, if lose conn with mds, try again
void connection_broken ()
{
    syslog(LOG_ERR, "DMSC INFO connection broken\n");
}

static void release_globale_res (void)
{
    /*
     * NOTE: memory return by gethostbyname, don't need free
     * if (g_nn_host != NULL) {
        syslog(LOG_ERR, "DMSC INFO free global nn host resource\n");
        free(g_nn_host);
        g_nn_host = NULL;
    }*/

    pthread_mutex_lock(&rel_res_lock);
    if (g_namenode_address != NULL) {
        syslog(LOG_INFO, "DMSC INFO free global namenode address resource\n");
        free(g_namenode_address);
        g_namenode_address = NULL;
    }
    pthread_mutex_unlock(&rel_res_lock);
}

static void add_configuration (const int8_t *element)
{
    int32_t i;
    int8_t *el_s_name = "<name>";
    int8_t *el_e_name = "</name>";
    int8_t *el_s_value = "<value>";
    int8_t *el_e_value = "</value>";

    int8_t *name_el_start = NULL;
    int8_t *name_el_end = NULL;
    int8_t *value_el_start = NULL;
    int8_t *value_el_end = NULL;

    int32_t length = 0;
    int8_t el[64];

    name_el_start = strstr(element, el_s_name);
    name_el_end   = strstr(element, el_e_name);
    value_el_start = strstr(element, el_s_value);
    value_el_end   = strstr(element, el_e_value);

    if (name_el_start != NULL && name_el_end != NULL &&
            value_el_start != NULL && value_el_end != NULL) {
        name_el_start = name_el_start + strlen(el_s_name);
        name_el_end = name_el_end - 1;
        value_el_start = value_el_start + strlen(el_s_value);
        value_el_end = value_el_end - 1;
        strncpy(el, value_el_start, value_el_end - value_el_start + 1);
        el[value_el_end - value_el_start + 1] = '\0';

        set_config_val(name_el_start, name_el_end - name_el_start + 1,
                el, strlen(el));
    }
}

static void load_configuration (int8_t *fname)
{
#define ch_arr_size 1024
    int8_t *el_s_property = "<property>", *el_e_property = "</property>";
    int8_t ch_array[ch_arr_size], ch;
    int32_t index = -1;
    int32_t state = 0;
    FILE *fp;

    if (NULL == fname) {
        syslog(LOG_INFO, "DMSC INFO Cannot find config file\n");
        return;
    }

    syslog(LOG_INFO, "DMSC INFO loading config %s\n",
            fname);

    fp = fopen(fname, "r");

    if (NULL == fp) {
        syslog(LOG_INFO, "DMSC INFO fail open configuration file: %s\n",
                fname);
        return;
    }

    memset(ch_array, 0, sizeof(int8_t)*ch_arr_size);

    while ((ch = fgetc(fp)) != EOF) {
        if (0 == state && '<' == ch) {
            state = 1;
        }

        if (state > 0) {
            index++; //ERROR Handling
            ch_array[index] = ch;
        }

        if ('>' == ch) {
            ch_array[index+1] = '\0';

            if (1 == state && (strstr(ch_array, el_s_property) != NULL)) {
                state = 2;
            } else if (2 == state && (strstr(ch_array, el_e_property) != NULL)) {
                //Extract value
                add_configuration(ch_array);
                state = 0;
                index = -1;
                memset(ch_array, 0, sizeof(int8_t)*ch_arr_size);
            } else if (1 == state) {
                index = -1;
                state = 0;
                memset(ch_array, 0, sizeof(int8_t)*ch_arr_size);
            }
        }
    }

    fclose(fp);
}

uint32_t ccma_inet_ntoa (int8_t *ipaddr)
{
    int32_t a, b, c, d;
    int8_t addr[4];

    sscanf(ipaddr, "%d.%d.%d.%d", &a, &b, &c, &d);
    addr[0] = a;
    addr[1] = b;
    addr[2] = c;
    addr[3] = d;
    return *(uint32_t *)addr;
}

static int32_t check_driver (int32_t drv_run_mode)
{
    int8_t buf[200];
    int8_t cmd[128];
    FILE *fp;
    int32_t ret;

    ret = 0;
    if (drv_run_mode != discoC_MODE_Normal &&
            drv_run_mode != discoC_MODE_Test_NN_Only) {
        sprintf(cmd, "insmod ./ccmablk.ko running_mode=%d", drv_run_mode);
        system(cmd);
        return ret;
    }

    fp = popen("lsmod|grep ccma","r");
    memset(buf, 0, 200);
    if (fgets(buf, 200, fp) != EOF) {
        if (buf[0] == 0) {
            sprintf(cmd, "insmod ./ccmablk.ko running_mode=%d", drv_run_mode);
            if (!(ret = system(cmd))) {
                syslog(LOG_INFO, "DMSC INFO load driver cmd: %s result %d\n", cmd, ret);
                system("insmod ../iosched/ccmathrottle.ko");
                syslog(LOG_INFO, "DMSC INFO load throttle module\n");
            }
        }
    }

    pclose(fp);

    return ret;
}

static void auto_remove_driver (void)
{
    FILE *fp;
    int8_t buf[200];
    int32_t i;

    fp = popen("lsmod|grep ccma","r");
    if (fgets(buf, 200, fp) != EOF) {
        if (buf[0] == 0) {
            syslog(LOG_INFO, "driver already unloaded..\n");
        } else {
            printf("unload ccmathrottle driver\n");
            system("rmmod ccmathrottle");
            printf("ccmathrottle driver removed\n");
            printf("unload driver \n");
            system("rmmod ccmablk");
            printf("driver removed\n");
        }
    }
    pclose(fp);


    for (i = 0; i < TIME_WAIT_DRV_UNLOAD; i++) {
        memset(buf, 0, 200);
        fp = popen("lsmod|grep ccma","r");
        if (fgets(buf, 200, fp) != EOF) {
            if (buf[0] == 0) {
                printf("driver already unloaded..\n");
                pclose(fp);
                break;
            } else {
                printf("DMSC INFO wait drv unload after %d seconds\n", i);
                syslog(LOG_INFO,
                        "DMSC INFO wait drv unload after %d seconds\n", i);
            }
        }

        pclose(fp);
        sleep(1);
    }

    if (i == TIME_WAIT_DRV_UNLOAD) {
        printf("DMSC ERROR driver may unload fail check the status\n");
        syslog(LOG_INFO,
                "DMSC ERROR driver may unload fail check the status\n");
    }
}

//TODO: 201505 shutdown may stuck, how long we can wait??
void sighandler (int32_t sig)
{
    int32_t ret = 0;

    printf("DMSC INFO control-c pressed\n");
    ret = ioctl(dms_chdev_fd, IOCTL_RESET_DRIVER, NULL);
    printf("DMSC INFO shutdown ret = %d\n", ret);
    syslog(LOG_INFO, "DMSC INFO shutdown ret = %d\n", ret);

    if (RESET_DRIVER_OK == ret) {
        printf("DMSC INFO ready to shutdown clientnode\n");
        syslog(LOG_INFO, "DMSC INFO ready to shutdown clientnode\n");
        close(dms_chdev_fd);
        system("rm -rf /dev/chrdev_dmsc");

        auto_remove_driver();
        stop_socket_server();
        release_globale_res();
        exit(1);
    } else if (RESET_DRIVER_BUSY == ret) {
        printf("DMSC INFO reset driver fail: volume is in using\n");
        syslog(LOG_INFO, "DMSC INFO reset driver fail: volume is in using\n");
    } else if (DRIVER_ISNOT_WORKING == ret) {
        printf("DMSC INFO reset driver fail: driver shutting down\n");
        syslog(LOG_INFO,"DMSC INFO reset driver fail: driver shutting down\n");
    } else {
        printf("DMSC INFO reset driver fail: unknown reason\n");
        syslog(LOG_INFO, "DMSC INFO reset driver fail: unknown reason\n");
    }
}

static int32_t init_discoC_drv (int32_t chrdev_fd)
{
    int32_t ret;

    ret = ioctl(chrdev_fd, IOCTL_INIT_DRV, NULL);

    return ret;
}

static void register_sighandler (void)
{
    if (signal(SIGINT, &sighandler) == SIG_ERR) {
        syslog(LOG_INFO, "DMSC INFO fail catch SIGINT\n");
    }

    if (signal(SIGTERM, &sighandler) == SIG_ERR) {
        syslog(LOG_INFO, "DMSC INFO fail catch SIGTERM\n");
    }

    if (signal(SIGKILL, &sighandler) == SIG_ERR) {
        syslog(LOG_INFO, "DMSC INFO fail catch SIGKILL\n");
    }

    if (signal(SIGPIPE, &sighandler) == SIG_ERR) {
        syslog(LOG_INFO, "DMSC INFO fail catch SIGPIPE\n");
    }
    //signal(SIGTSTP, &tsphandler);
}

static int32_t install_driver (int32_t driver_mode, int32_t prev_shutdown)
{
    FILE *fp;
    struct sockaddr_in nn_Addr;
    uint32_t nn_ip;
    int32_t ret;

    syslog(LOG_INFO, "DMSC INFO User Daemon load kernel module\n");

    if ((ret = check_driver(driver_mode))) {
        syslog(LOG_INFO, "DMSC INFO fail insert kernel module ret %d\n", ret);
        return ret;
    }

    fp = fopen("/dev/chrdev_dmsc", "r");
    if (!fp) {
        //TODO : Using variable for chrdev_dmsc, bcz driver also need this name
        system("mknod /dev/chrdev_dmsc c 151 0");
        syslog(LOG_INFO, "DMSC INFO mknod /dev/chrdev_dmsc c 151 0\n");
    } else {
        fclose(fp);
        syslog(LOG_INFO, "DMSC INFO char interface exist\n");
    }

    dms_chdev_fd = open("/dev/chrdev_dmsc", O_RDWR);
    if (dms_chdev_fd == -1) {
        syslog(LOG_INFO, "DMSC ERROR char interface open fail /dev/chrdev_dmsc\n");
        auto_remove_driver();
        return -1;
    }

    if (g_nnSimulator_on == false) {
        memcpy(&(nn_Addr.sin_addr.s_addr), g_nn_host->h_addr, g_nn_host->h_length);
        nn_ip = (uint32_t)nn_Addr.sin_addr.s_addr;
    } else {
        nn_ip = ccma_inet_ntoa(g_nnSimulator_ipaddr);
        gdef_nnSrvIPAddr = nn_ip;
    }

    syslog(LOG_INFO, "DMSC INFO User Daemon config kernel module\n");
    config_discoC_drv(dms_chdev_fd, nn_ip);

    syslog(LOG_INFO, "DMSC INFO User Daemon init kernel module\n");
    ret = init_discoC_drv(dms_chdev_fd);
    if (ret) {
        syslog(LOG_INFO, "DMSC WARN init driver fail ret %d\n", ret);
        close(dms_chdev_fd);
        system("rm -rf /dev/chrdev_dmsc");
        auto_remove_driver();
    } else {
        syslog(LOG_INFO, "DMSC INFO User Daemon register MDS\n");
    }

    return ret;
}

static int8_t *getNamenodeAddress (void)
{
    FILE *configfn;
    int8_t buff[256];
    int8_t *startStr = "hdfs://";
    int8_t *pos1, *pos2;

    if (NULL != g_namenode_address) {
        return g_namenode_address;
    }

    if (discoC_MODE_Test_FakeNode == clientnode_running_mode) {
        g_namenode_address =
                (int8_t *)malloc(sizeof(int8_t)*(strlen(NNAddr_local) + 1));
        memcpy(g_namenode_address, NNAddr_local, strlen(NNAddr_local));
        g_namenode_address[strlen(NNAddr_local)] = '\0';
        return g_namenode_address;
    }

    if ((configfn = fopen(NamenodeConfigFile, "r")) == NULL) {
        g_namenode_address =
                (int8_t *)malloc(sizeof(int8_t)*(strlen(NNAddr_default) + 1));
        memcpy(g_namenode_address, NNAddr_default, strlen(NNAddr_default));
        g_namenode_address[strlen(NNAddr_default)] = '\0';
        return g_namenode_address;
    }

    while (fgets(buff, 256, configfn) != NULL) {
        pos1 = strstr(buff, startStr);
        if (pos1 != NULL) {
            pos1 = pos1 + strlen(startStr);
            pos2 = strstr(pos1, ":");
            if (pos2 != NULL) {
                g_namenode_address =
                     (int8_t *)malloc(sizeof(int8_t)*(strlen(pos1) - strlen(pos2) + 1));
                memcpy(g_namenode_address, pos1, strlen(pos1) - strlen(pos2));
                g_namenode_address[strlen(pos1) - strlen(pos2)] = '\0';
                fclose(configfn);
                return g_namenode_address;
            }
        }
    }
    fclose(configfn);

    g_namenode_address = (int8_t *)malloc(sizeof(int8_t)*strlen(NNAddr_default));
    memcpy(g_namenode_address, NNAddr_default, strlen(NNAddr_default));
    return g_namenode_address;
}

static int32_t check_dms_services_by_dns (void)
{
    int8_t *nn_addr;
    int32_t retry;

    nn_addr = getNamenodeAddress();
    retry = 0;
    syslog(LOG_INFO,
            "DMSC INFO check dms service exist by resolve dns: %s\n", nn_addr);
    g_nn_host = (struct hostent *)gethostbyname(nn_addr);

    while (g_nn_host == NULL) {
        retry++;
        if (retry > MAX_CHECK_DMS_SERVICES_CNT) {
            syslog(LOG_INFO, "gethostbyname %s fail and exceed max retry time "
                    "for 30 minutes\n", nn_addr);
            return -1;
        }
        g_nn_host = gethostbyname(nn_addr);
        sleep(1);
    }

    memcpy(&gdef_nnSrvIPAddr, g_nn_host->h_addr, g_nn_host->h_length);
    return 0;
}

static prev_stop_state_t get_prev_shutdown_state ()
{
    FILE *fp;
    prev_stop_state_t ret;

    fp = fopen(shutdown_chk_file, "r");

    if (fp) {
        fclose(fp);
        ret = PREV_SHUTDOWN_ABNORMAL;
    } else {
        ret = PERV_SHUTDOWN_NORMAL;
    }

    return ret;
}

static int32_t is_drv_running (void)
{
    int8_t buf[200];
    int8_t cmd[128];
    FILE *fp;
    int32_t ret;

    ret = 0;

    fp = popen("lsmod|grep ccma","r");
    memset(buf, 0, 200);
    if (fgets(buf, 200, fp) != EOF) {
        if (buf[0] == 0) {
            ret = 0;
        } else {
            ret = -EEXIST;
        }
    }

    pclose(fp);

    return ret;
}

int32_t main (int32_t argc, int8_t *argv[])
{
    int8_t hostname[64];
    int32_t prev_shutdown;

    syslog(LOG_INFO, "DMSC INFO User level daemon start up\n");

    if (NUM_STARTUP_PARAM == argc) {
        switch (argv[1][0]) {
        case '0':
            clientnode_running_mode = discoC_MODE_Normal;
            break;
        case '1':
            clientnode_running_mode = discoC_MODE_SelfTest;
            break;
        case '2':
            clientnode_running_mode = discoC_MODE_Test_FakeNode;
            break;
        case '3':
            clientnode_running_mode = discoC_MODE_Test_NN_Only;
            break;
        case '4':
            clientnode_running_mode = discoC_MODE_SelfTest_NN_Only;
            break;
        default:
            clientnode_running_mode = discoC_MODE_Normal;
            break;
        }
    } else {
        clientnode_running_mode = discoC_MODE_Normal;
    }

    init_config_list();
    load_configuration(dms_config_file);
    config_discoC_UsrD();

    if (g_nnSimulator_on == false) {
        if (check_dms_services_by_dns()) {
            syslog(LOG_INFO, "DMSC ERR fail get dms ipaddr by dns\n");
            release_globale_res();
            return -1;
        }
    }

    if (!is_drv_running()) {
        syslog(LOG_INFO, "DMSC INFO drv not running ready to install module\n");
        if (g_nnSimulator_on == false) {
            if (g_nn_host != NULL) {
                check_namenode_connection(g_nn_host);
            } else {
                syslog(LOG_INFO,
                    "DMSC ERROR Cannot resolve dns dms service isn't exist\n");
                //TODO: Check whether any memory leak;
                exit(0);
            }
        }

        prev_shutdown = get_prev_shutdown_state();
        if (install_driver(clientnode_running_mode, prev_shutdown)) {
            syslog(LOG_INFO, "DMSC ERROR User level install_driver fail\n");
            if (g_nn_host != NULL) {
                //free(g_nn_host); NOTE: memory return by gethostbyname, don't need free
                g_nn_host = NULL;
            }

            release_globale_res();
            return -1;
        }
    } else {
        syslog(LOG_INFO, "DMSC INFO drv running open char devices\n");
        dms_chdev_fd = open("/dev/chrdev_dmsc", O_RDWR);
        if (dms_chdev_fd == -1) {
            syslog(LOG_INFO, "DMSC ERROR char interface open fail /dev/chrdev_dmsc\n");
            perror("DMSC ERROR char interface open fail /dev/chrdev_dmsc");
            //TODO : release resources
            return -1;
        }
    }

    register_sighandler();
    create_socket_server();
    syslog(LOG_ERR, "DMSC sk server closed\n");
    release_globale_res();
    return 0;
}
