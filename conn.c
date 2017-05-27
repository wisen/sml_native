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

#include <cutils/sockets.h>
#include "headers/common.h"
#include "headers/conn.h"

/* 1 ctrl listen socket, 1 ctrl data socket, 1 tbd */
#define MAX_EPOLL_EVENTS 3
static int epollfd;
static int maxevents;

/* control socket listen and data */
static int ctrl_lfd;
static int ctrl_dfd = -1;
static int ctrl_dfd_reopened; /* did we reopen ctrl conn on this loop? */

#define ATRACE_MONITOR_SOCKET_NAME "atrace_monitor_sk"
#define CTRL_PACKET_MAX (sizeof(int) * (2))

enum atm_cmd {
    ATM_START_SYSTRACE,
    ATM_FINISH_SYSTRACE,
    ATM_START_BGREPORT,
    ATM_FINISH_BGREPORT,
    ATM_START_LOGCAT,
    ATM_FINISH_LOGCAT,
    ATM_START_ALL,
    ATM_FINISH_ALL,
    ATM_MAX_CMD,
};

static int ctrl_data_read(char *buf, size_t bufsz) {
    int ret = 0;

    ret = read(ctrl_dfd, buf, bufsz);

    if (ret == -1) {
        DM("control data socket read failed; errno=%d", errno);
    } else if (ret == 0) {
        DM("Got EOF on control data socket");
        ret = -1;
    }

    return ret;
}

static int ctrl_data_write(char *buf, size_t bufsz) {
    int ret = 0;

    ret = write(ctrl_dfd, buf, bufsz);

    if (ret == -1) {
        DM("control data socket write failed; errno=%d", errno);
    } else if (ret == 0) {
        DM("Got EOF on control data socket");
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

    len = ctrl_data_read((char *)ibuf, CTRL_PACKET_MAX);
    if (len <= 0)
        return;

    nargs = len / sizeof(int) - 1;
    if (nargs < 0)
        goto wronglen;

    cmd = ntohl(ibuf[0]);
    DM("Recv from UI: %d %d", ntohl(ibuf[0]), ntohl(ibuf[1]));
    switch(cmd) {
    case ATM_START_SYSTRACE:
        DM("Start collect systrace");
        set_blockflag(1);
        dump_systrace();
        break;
    case ATM_FINISH_SYSTRACE:
        DM("finish collect systrace");
	 set_blockflag(0);
        break;
    case ATM_START_LOGCAT:
        DM("Start collect logcat");
        set_blockflag(1);
        break;
    case ATM_FINISH_LOGCAT:
        DM("finish collect logcat");
	 set_blockflag(0);
        break;
    case ATM_START_BGREPORT:
	 DM("Start collect bugreport");
	 set_blockflag(1);
	 do_bugreport();
        break;
    case ATM_FINISH_BGREPORT:
	 DM("finish collect bugreport");
	 set_bugreport(0);
	 set_blockflag(0);
        break;
    case ATM_START_ALL:
	 DM("start collect all");
	 set_blockflag(1);
	 dump_systrace();
	 set_blockflag(1);
	 do_bugreport();
        break;
    case ATM_FINISH_ALL:
	 DM("finish collect all");
	 set_blockflag(0);
        break;
    default:
        DM("Received unknown command code %d", cmd);
        return;
    }

    return;

wronglen:
    DM("Wrong control socket read length cmd=%d len=%d", cmd, len);
}

static void ctrl_data_close(void) {
    DM("Closing SML data connection");
    close(ctrl_dfd);
    ctrl_dfd = -1;
    maxevents--;
}

static void ctrl_data_handler(uint32_t events) {
    if (events & EPOLLHUP) {
        DM("SML disconnected");
        if (!ctrl_dfd_reopened)
            ctrl_data_close();
    } else if (events & EPOLLIN) {
        ctrl_command_handler();
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
        DM("sml control socket accept failed; errno=%d", errno);
        return;
    }

    DM("SML connected");
    maxevents++;
    epev.events = EPOLLIN;
    epev.data.ptr = (void *)ctrl_data_handler;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, ctrl_dfd, &epev) == -1) {
        DM("epoll_ctl for data connection socket failed; errno=%d", errno);
        ctrl_data_close();
        return;
    }
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

    ctrl_lfd = android_get_control_socket(ATRACE_MONITOR_SOCKET_NAME);
    if (ctrl_lfd < 0) {
        DM("get atrace_monitor_sk control socket failed:%d", errno);
        ctrl_lfd = socket_local_server(ATRACE_MONITOR_SOCKET_NAME,
                                   ANDROID_SOCKET_NAMESPACE_RESERVED,
                                   SOCK_STREAM);
	if (ctrl_lfd < 0) {
		DM("create atrace_monitor_sk control socket failed:%d", errno);
		return -1;
	} 
    }
    DM("get atrace_monitor_sk[%d]", ctrl_lfd);

    ret = listen(ctrl_lfd, 1);
    if (ret < 0) {
        DM("atrace_monitor_sk control socket listen failed (errno=%d)", errno);
        return -1;
    }
    DM("listen to atrace_monitor_sk[%d]", ctrl_lfd);

    epev.events = EPOLLIN;
    epev.data.ptr = (void *)ctrl_connect_handler;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, ctrl_lfd, &epev) == -1) {
        DM("epoll_ctl for sml control socket failed (errno=%d)", errno);
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
