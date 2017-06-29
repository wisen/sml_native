#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/cdefs.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <cutils/sockets.h>
#include "headers/common.h"
#include "headers/conn.h"
#include "headers/bugreport_impl.h"
#include "headers/logcat_impl.h"
#include "headers/atrace_impl.h"

/* ctrl and rsp listen socket, ctrl and rsp data socket, 2 tbd */
#define MAX_EPOLL_EVENTS 6
static int epollfd;
static int maxevents;

/* control socket listen and data */
static int ctrl_lfd;
static int ctrl_dfd = -1;
static int ctrl_dfd_reopened; /* did we reopen ctrl conn on this loop? */
/* response socket listen and data*/
static int rsp_lfd;
static int rsp_dfd = -1;
static int rsp_dfd_reopened;
/* write socket listen and data*/
static int wr_lfd;
static int wr_dfd = -1;
static int wr_sk_enabled = 0;

#define ATRACE_MONITOR_CMD_SK_NAME "atrace_monitor_cmd_sk"
#define ATRACE_MONITOR_RSP_SK_NAME "atrace_monitor_rsp_sk"
#define ATRACE_MONITOR_WR_SK_NAME "atrace_monitor_wr_sk"
#define CTRL_PACKET_MAX (sizeof(int) * (2))

static int ctrl_data_read(char *buf, size_t bufsz) {
    int ret = 0;

    ret = read(ctrl_dfd, buf, bufsz);

    if (ret == -1) {
        DM("cmd data socket read failed; errno=%d", errno);
    } else if (ret == 0) {
        DM("Got EOF on cmd data socket");
        ret = -1;
    }

    return ret;
}

static int rsp_data_read(char *buf, size_t bufsz) {
    int ret = 0;

    ret = read(rsp_dfd, buf, bufsz);

    if (ret == -1) {
        DM("rsp data socket read failed; errno=%d", errno);
    } else if (ret == 0) {
        DM("Got EOF on rsp data socket");
        ret = -1;
    }

    return ret;
}

static int ctrl_data_write(char *buf, size_t bufsz) {
    int ret = 0;

    ret = write(ctrl_dfd, buf, bufsz);

    if (ret == -1) {
        DM("cmd data socket write failed; errno=%d", errno);
    } else if (ret == 0) {
        DM("Got EOF on cmd data socket");
        ret = -1;
    }

    return ret;
}

static void ctrl_command_handler(void) {
    int ibuf[CTRL_PACKET_MAX / sizeof(int)];
    int len;
    int cmd = -1;
    int nargs;
    int targets;
    time_t bugreport_duration,action_now;

    len = ctrl_data_read((char *)ibuf, CTRL_PACKET_MAX);
    if (len <= 0)
        return;

    nargs = len / sizeof(int) - 1;
    if (nargs < 0)
        goto wronglen;

    cmd = ntohl(ibuf[0]);
    DM("Recv cmd from UI: %d %d", ntohl(ibuf[0]), ntohl(ibuf[1]));
    if (get_blockflag())
    {
        action_now = time(NULL);
        bugreport_duration = get_bugreport_duration(action_now);
        DM("block from %s, bugreport_duration=%d", get_blockflag()==1?"UI":"self", bugreport_duration);
        if(bugreport_duration < BUGREPORT_TIMEOUT){
            return;
        } else {
            //stop dumpstate service TODO
            set_blockflag(0);
        }
    }

    switch(cmd) {
    case ATM_START_SYSTRACE:
        DM("Start collect systrace");
        set_blockflag(1);
        dump_systrace();
        break;
    case ATM_START_LOGCAT:
        DM("Start collect logcat");
        //set_blockflag(1);
        dump_logbuffer();
        break;
    case ATM_START_BGREPORT:
	 DM("Start collect bugreport");
	 set_blockflag(1);
	 do_bugreport();
        break;
    case ATM_START_ALL:
	 DM("start collect all");
	 set_blockflag(1);
	 dump_systrace();
	 dump_logbuffer();
	 set_blockflag(1);
	 do_bugreport();
        break;
    case ATM_REINIT_LOGCAT:
	 DM("reinit logcat");
	 init_logcat();
        break;
    case ATM_ENABLE_WRSK:
	 DM("enable write socket");
	 if (!init_write_socket()) wr_sk_enabled = 1;
        break;
    case ATM_SWITCH_ATRACE:
	 DM("enable write socket");
	 if(1==get_atrace_status())
	{
		clean_systrace();
		do_vibrate(2, 300 * 1000, 200);
	} else {
		init_atrace();
		do_vibrate(1, 10, 1000);
	}
	 break;
    default:
        DM("Received unknown command code %d", cmd);
        return;
    }

    return;

wronglen:
    DM("Wrong control socket read length cmd=%d len=%d", cmd, len);
}

static void rsp_handler(void) {
    int ibuf[CTRL_PACKET_MAX / sizeof(int)];
    int len;
    int cmd = -1;
    int nargs;
    int targets;
    time_t bugreport_duration,action_now;

    len = rsp_data_read((char *)ibuf, CTRL_PACKET_MAX);
    if (len <= 0)
        return;

    nargs = len / sizeof(int) - 1;
    if (nargs < 0)
        goto wronglen;

    cmd = ntohl(ibuf[0]);
    DM("Recv RSP from UI: %d %d", ntohl(ibuf[0]), ntohl(ibuf[1]));

    switch(cmd) {
    case ATM_FINISH_SYSTRACE:
        DM("finish collect systrace");
	 set_blockflag(0);
        break;
    case ATM_FINISH_LOGCAT:
        DM("finish collect logcat");
	 //set_blockflag(0);
        break;
    case ATM_FINISH_BGREPORT:
	 DM("finish collect bugreport");
	 do_vibrate(1, 10, 1000);
	 set_bugreport(0);
	 set_blockflag(0);
        break;
    case ATM_FINISH_ALL:
	 DM("finish collect all");
	 set_blockflag(0);
        break;
    default:
        DM("Received unknown response code %d", cmd);
        return;
    }

    return;

wronglen:
    DM("Wrong response socket read length rsp=%d len=%d", cmd, len);
}

static void ctrl_data_close(void) {
    DM("Closing cmd data connection");
    close(ctrl_dfd);
    ctrl_dfd = -1;
    maxevents--;
}

static void rsp_data_close(void) {
    DM("Closing rsp data connection");
    close(rsp_dfd);
    rsp_dfd = -1;
    maxevents--;
}

static void ctrl_data_handler(uint32_t events) {
    if (events & EPOLLHUP) {
        DM("cmd data socket disconnected");
        if (!ctrl_dfd_reopened)
            ctrl_data_close();
    } else if (events & EPOLLIN) {
        ctrl_command_handler();
    }
}

static void rsp_data_handler(uint32_t events) {
    if (events & EPOLLHUP) {
        DM("rsp data socket disconnected");
        if (!rsp_dfd_reopened)
            rsp_data_close();
    } else if (events & EPOLLIN) {
        rsp_handler();
    }
}

static void ctrl_connect_handler(uint32_t events __unused) {
    struct sockaddr_storage ss;
    struct sockaddr *addrp = (struct sockaddr *)&ss;
    socklen_t alen;
    struct epoll_event epev;

    if (ctrl_dfd >= 0) {
        ctrl_data_close();
        ctrl_dfd_reopened = 1;
    }

    alen = sizeof(ss);
    ctrl_dfd = accept(ctrl_lfd, addrp, &alen);

    if (ctrl_dfd < 0) {
        DM("sml cmd socket accept failed; errno=%d", errno);
        return;
    }

    DM("SML cmd connected");
    maxevents++;
    epev.events = EPOLLIN;
    epev.data.ptr = (void *)ctrl_data_handler;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, ctrl_dfd, &epev) == -1) {
        DM("epoll_ctl for cmd data connection socket failed; errno=%d", errno);
        ctrl_data_close();
        return;
    }
}

static void rsp_connect_handler(uint32_t events __unused) {
    struct sockaddr_storage ss;
    struct sockaddr *addrp = (struct sockaddr *)&ss;
    socklen_t alen;
    struct epoll_event epev;

    if (rsp_dfd >= 0) {
        rsp_data_close();
        rsp_dfd_reopened = 1;
    }

    alen = sizeof(ss);
    rsp_dfd = accept(rsp_lfd, addrp, &alen);

    if (rsp_dfd < 0) {
        DM("sml rsp socket accept failed; errno=%d", errno);
        return;
    }

    DM("SML rsp connected");
    maxevents++;
    epev.events = EPOLLIN;
    epev.data.ptr = (void *)rsp_data_handler;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, rsp_dfd, &epev) == -1) {
        DM("epoll_ctl for rsp data connection socket failed; errno=%d", errno);
        rsp_data_close();
        return;
    }
}

int is_wrsk_enabled() {
	return wr_sk_enabled;
}

int write_data_toJ(enum atm_cmd cmd) {
    int ret = 0;
    int len = 0;
    int wr_cmd[4];
    wr_cmd[0] = cmd;
    wr_cmd[1] = 0;
    wr_cmd[2] = 0;
    wr_cmd[3] = 0;
    do {
        len = write(wr_lfd, wr_cmd, sizeof(wr_cmd));
    } while (len < 0 && errno == EINTR);

    if(len != sizeof(wr_cmd)) {
        DM("write_data_toJ len=%d errno=%d", len, errno);
        return -1;
    }

    DM("write_data_toJ %d sucess", wr_cmd[0]);

    return 0;
}

int init_write_socket()
{
    struct sockaddr_storage ss;
    struct sockaddr *addrp = (struct sockaddr *)&ss;
    socklen_t alen;
    int ret = 0;
	
    //wr_lfd = android_get_control_socket(ATRACE_MONITOR_WR_SK_NAME);
    wr_lfd = socket_local_client(ATRACE_MONITOR_WR_SK_NAME,
                  ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
    if (wr_lfd < 0) {
        DM("create atrace_monitor_wr_sk cmd socket failed:%d", errno);
        return -1;
    }

    //ret = listen(wr_lfd, 1);
    //if (ret < 0) {
    //    DM("atrace_monitor_wr_sk cmd socket listen failed (errno=%d)", errno);
    //    return -1;
    //}
    //DM("listen to atrace_monitor_wr_sk[%d]", wr_lfd);

    //if (wr_lfd >= 0) {
    //    ctrl_data_close();
     //   ctrl_dfd_reopened = 1;
    //}

    //alen = sizeof(ss);
    //wr_dfd = accept(wr_lfd, addrp, &alen);

   // if (wr_dfd < 0) {
   //    DM("sml write socket accept failed; errno=%d", errno);
    //    return -1;
    //}

    DM("SML write socket connected");
    return 0;
}

int init(void) {
    struct epoll_event epev;
    int i;
    int ret;

    epollfd = epoll_create(MAX_EPOLL_EVENTS);
    if (epollfd == -1) {
        DM("epoll_create failed (errno=%d)", errno);
        return -1;
    }

    ctrl_lfd = android_get_control_socket(ATRACE_MONITOR_CMD_SK_NAME);
    if (ctrl_lfd < 0) {
        DM("get atrace_monitor_cmd_sk cmd socket failed:%d", errno);
        ctrl_lfd = socket_local_server(ATRACE_MONITOR_CMD_SK_NAME,
                                   ANDROID_SOCKET_NAMESPACE_RESERVED,
                                   SOCK_STREAM);
	if (ctrl_lfd < 0) {
		DM("create atrace_monitor_cmd_sk cmd socket failed:%d", errno);
		return -1;
	} 
    }
    DM("get atrace_monitor_cmd_sk[%d]", ctrl_lfd);

    ret = listen(ctrl_lfd, 1);
    if (ret < 0) {
        DM("atrace_monitor_cmd_sk cmd socket listen failed (errno=%d)", errno);
        return -1;
    }
    DM("listen to atrace_monitor_cmd_sk[%d]", ctrl_lfd);

    epev.events = EPOLLIN;
    epev.data.ptr = (void *)ctrl_connect_handler;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, ctrl_lfd, &epev) == -1) {
        DM("epoll_ctl for sml cmd socket failed (errno=%d)", errno);
        return -1;
    }
    maxevents++;

    rsp_lfd = android_get_control_socket(ATRACE_MONITOR_RSP_SK_NAME);
    if (rsp_lfd < 0) {
        DM("get atrace_monitor_rsp_sk rsp socket failed:%d", errno);
        rsp_lfd = socket_local_server(ATRACE_MONITOR_RSP_SK_NAME,
                                   ANDROID_SOCKET_NAMESPACE_RESERVED,
                                   SOCK_STREAM);
	if (rsp_lfd < 0) {
		DM("create atrace_monitor_rsp_sk rsp socket failed:%d", errno);
		return -1;
	} 
    }
    DM("get atrace_monitor_rsp_sk[%d]", rsp_lfd);

    ret = listen(rsp_lfd, 1);
    if (ret < 0) {
        DM("atrace_monitor_rsp_sk rsp socket listen failed (errno=%d)", errno);
        return -1;
    }
    DM("listen to atrace_monitor_rsp_sk[%d]", rsp_lfd);

    epev.events = EPOLLIN;
    epev.data.ptr = (void *)rsp_connect_handler;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, rsp_lfd, &epev) == -1) {
        DM("epoll_ctl for sml rsp socket failed (errno=%d)", errno);
        return -1;
    }
    maxevents++;

    return 0;
}

void mainloop(void) {
    while (1) {
        struct epoll_event events[maxevents];
        int nevents;
        int i;

        ctrl_dfd_reopened = 0;
        rsp_dfd_reopened = 0;
        nevents = epoll_wait(epollfd, events, maxevents, -1);

        if (nevents == -1) {
            if (errno == EINTR)
                continue;
            DM("epoll_wait failed (errno=%d)", errno);
            continue;
        }

        for (i = 0; i < nevents; ++i) {
            if (events[i].events & EPOLLERR)
                DM("EPOLLERR on event #%d", i);
            if (events[i].data.ptr)
                (*(void (*)(uint32_t))events[i].data.ptr)(events[i].events);
        }
    }
}

/*
int main(int argc __unused, char **argv __unused) {
	if (!init())
        mainloop();

    DM("exiting");
    return 0;
}*/
