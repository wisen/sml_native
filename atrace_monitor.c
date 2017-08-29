#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/prctl.h>

#include "headers/common.h"
#include "headers/conn.h"
#include "headers/atrace_impl.h"
#include "headers/bugreport_impl.h"
#include "headers/logcat_impl.h"
#include "headers/vibrator_impl.h"
#include "version.d"

#define ATM_VERSION "1.01-"ATM_VERSION_POSTFIX

#define APP_DIR  "/system/"
#define BIN_DIR  APP_DIR "bin/"
#define LIB_DIR  APP_DIR "lib/"
#define DAT_DIR  "/data/"
#define MAX_DEVICES	32

static int commfd = -1;
static int done = 0;

#define	POLL_PIPE_READ	0
#define	POLL_PIPE_WRITE	1
#define	POLL_INITIAL	{{-1,-1}}

typedef struct {
	int pipefds [2];
} POLL;

static POLL pl;

static int inlist (const char **list, const char *name)
{
	int i;
	for (i = 0; list [i] != NULL; i ++)
		if (strstr (name, list [i]) != NULL)
			return 1;
	return 0;
}

ssize_t read_nointr (int fd, void *buf, size_t count)
{
	ssize_t ret = -1;

	for (;;)
	{
		ret = read (fd, buf, count);

		if (ret < 0)
		{
			if (errno == EINTR)
			{
				DM ("%s, retry read fd [%d]\n", strerror (errno), fd);
				usleep (10000);
				continue;
			}

			if (DEBUG) DM ("read fd %d: %s\n", fd, strerror (errno));
		}

		break;
	}

	return ret;
}


/*
 * Open all input devices.
 *
 * Return an integer array contains fds, the first integer is the fd count.
 */
int *open_input_devices (const char **white_list, const char **black_list)
{
	char devname [80];
	char dev [20] = "/dev/input/event";
	int fds [MAX_DEVICES];
	int i, idx, *ret = NULL;

	for (i = 0, idx = 0; i < MAX_DEVICES; i ++)
	{
		sprintf (& dev [16], "%d", i);

		fds [idx] = open (dev, O_RDWR);

		if (fds [idx] < 0)
			break;

		memset (devname, 0, sizeof (devname));

		if (ioctl (fds [idx], EVIOCGNAME (sizeof (devname) - 1), & devname) < 1)
		{
			DM ("ioctl %s: %s\n", dev, strerror (errno));
			close (fds [idx]);
			continue;
		}

		if (white_list && black_list)
		{
			if (! inlist (white_list, devname))
				goto skipped;

			if (inlist (black_list, devname))
				goto skipped;
		}
		else if (white_list)
		{
			if (! inlist (white_list, devname))
				goto skipped;
		}
		else if (black_list)
		{
			if (inlist (black_list, devname))
				goto skipped;
		}

		DM ("opened [%s][%s][%d]\n", dev, devname, fds [idx]);

		idx ++;
		continue;

	skipped:;
		close (fds [idx]);
		if (DEBUG)  DM  ("filter [%s][%s]\n", dev, devname);
	}

	if (idx == 0)
	{
		DM ("cannot open any input event node!");
		goto end;
	}

	ret = (int *) malloc (sizeof (int) * (idx + 1));

	if (ret)
	{
		ret [0] = idx;

		for (i = 0; i < idx; i ++)
			ret [i + 1] = fds [i];
	}

end:;
	return ret;
}

void close_input_devices (int *list)
{
	if (list)
	{
		int i;

		for (i = 1; i <= list [0]; i ++)
		{
			close (list [i]);
			DM ("closed [%d]\n", list [i]);
		}

		free (list);
	}
}

/*
 * return < 0 on error, 0 on user break or timeout, others for fd index (1 base).
 */
int poll_multiple_wait (POLL *pl, int timeout_ms, int *fd, int count)
{
	struct pollfd *fds;
	int nr;
	int debug;
	if ((! pl) || (! fd) || (count <= 0)) return -1;
	fds = (struct pollfd *) malloc ((count + 1) * sizeof (struct pollfd));
	if (! fds) return -1;
	for (;;)
	{
		//if (DEBUG) DM ("count = %d (+1)\n", count);
		if (DEBUG) DM ("count = %d\n", count);

		for (nr = 0; nr < count; nr ++)
		{
			if (DEBUG) DM ("  device fd %d = %d\n", nr, fd [nr]);
			fds [nr].fd = fd [nr];
			fds [nr].events = POLLIN;
			fds [nr].revents = 0;
		}

		/*
		if (DEBUG) DM ("    pipe fd %d = %d\n", nr, pl->pipefds [POLL_PIPE_READ]);
		fds [nr].fd = pl->pipefds [POLL_PIPE_READ];
		fds [nr].events = POLLIN;
		fds [nr].revents = 0;
		*/

		for (;;)
		{
			//nr = poll (fds, count + 1, timeout_ms);
			nr = poll (fds, count, timeout_ms);

			if ((nr < 0) && (errno == EINTR))
			{
				DM ("%s, retry poll in poll_multiple_wait\n", strerror (errno));
				usleep (10000);
				continue;
			}

			break;
		}

		if (DEBUG) DM ("  poll() got %d\n", nr);

		if (nr <= 0)
		{
			if (nr < 0)
			{
				DM ("poll: %d: %s\n", nr, strerror (errno));
			}
			break;
		}

		if (fds [count].revents & POLLIN)
		{
			/* user break */
			char buffer [1];
			read_nointr (fds [count].fd, buffer, 1);
			if (DEBUG) DM ("  user break, return 0\n");
			nr = 0;
			break;
		}

		for (nr = 0; nr < count; nr ++)
		{
			if (fds [nr].revents & POLLIN)
			{
				/* have data */
				if (DEBUG) DM ("  data return %d (+1)\n", nr);
				free (fds);
				return (nr + 1);
			}
		}

		if (DEBUG) DM ("  no valid data!\n");

		/* nr > 0 but no valid data found */
		for (nr = 0; nr <= count; nr ++)
		{
			if (fds [nr].revents)
			{
				DM ("poll no valid data! fd[%d]=%d revents=0x%04X\n", nr, fds [nr].fd, fds [nr].revents);
			}
		}
		nr = -1;
		break;
	}
	free (fds);
	if (DEBUG) DM ("  return %d\n", nr);
	return nr;
}

void *thr_connJ(void *arg){
    DM("Start connj..");
    if (!init())
        mainloop();
    return ((void *)0);
}

int main()
{
	DM("Version: %s\n", ATM_VERSION);

	init_atrace();
	init_logcat();

	pthread_t ntid;
	int err;
	err = pthread_create(&ntid, NULL, thr_connJ, NULL);
	if(err != 0){
		DM ("can't create thread: %s\n",strerror(err));
		return 1;
	}

	const char *white_list [] = {
		//wisen: mark unnecessary input event source to reduce log out
		//"mtk-tpd-kpd",
		//"mtk-tpd",
		"mtk-kpd",
		NULL
	};

	const char *black_list [] = {
		"dummy",
		"projector",
		NULL
	};

	struct input_event ie;
	int timeout_ms = -1;
	int *fds;
	int nr, trigger, trigger_close = -1;
	time_t combkey_last, combkey_now, combkey_interval, bugreport_duration;
	combkey_last = combkey_now = combkey_interval = 0;

	fds = open_input_devices (white_list, black_list);

	if (! fds) return -1;

	DM ("got %d fds\n", fds [0]);
	timeout_ms = -1;

	while (! done)
	{
		nr = poll_multiple_wait (& pl, timeout_ms, & fds [1], fds [0]);

		if (nr == 0) //nr == 0 : timeout or poll_break
		{
			if (done) break;

			DM ("timeout clear triggers\n");
			timeout_ms = -1;
			trigger = -1;
			trigger_close = -1;
			continue;
		}

		if (nr < 0) break;

		while (read (fds [nr], & ie, sizeof (ie)) == sizeof (ie))
		{
			int is_combine_key = 0;
			if (ie.type == EV_KEY)
			{
				DM ("fds #%d got EV_KEY, type = %d, code=%d, value=%d\n", nr, ie.type, ie.code, ie.value);
				if (ie.code == KEY_POWER)
				{
					if (DEBUG) DM("KEY_POWER nothing to do..\n");
				}
				else if (ie.code == KEY_VOLUMEUP)
				{
					is_combine_key = 1;
					if (DEBUG) DM("KEY_VOLUMEUP %s\n", (ie.value != 0) ? "DOWN" : "UP");
				}
				else if (ie.code == KEY_VOLUMEDOWN)
				{
					is_combine_key = 1;
					if (DEBUG) DM("KEY_VOLUMEDOWN %s\n", (ie.value != 0) ? "DOWN" : "UP");
				}

				if (! is_combine_key)
					break;
			}

			if (get_blockflag())
			{
				//break;
				combkey_now = time(NULL);
				bugreport_duration = get_bugreport_duration(combkey_now);
				DM("block from %s, bugreport_duration=%d", get_blockflag()==1?"UI":"ATM", bugreport_duration);
				if(bugreport_duration < get_bugreport_timeout()){
					is_combine_key = 0;
				} else {
					//stop dumpstate service, restore the block flag
					DM("bugreport timeout(may be UI don't announce it.)");
					do_vibrate(1, 10, 1000);
					set_blockflag(0);
				}
			}

			/* ignore non-key and key down events */
			if ((ie.type != EV_KEY) || (ie.value != 0 /* not KEY UP */)) break;

			if (is_combine_key)
			{
				combkey_now = time(NULL);

				if (trigger == -1 && trigger_close == -1) combkey_interval = 0;
				else combkey_interval = combkey_now - combkey_last;

				DM("now: %d, last: %d, interval: %d\n", combkey_now, combkey_last, combkey_interval);
			}

			if (is_combine_key)
			{
				if (combkey_interval <= KEY_TIMEOUT)
				{
					trigger++;
					trigger_close++;
				}else {
					trigger = -1;
					trigger_close = -1;
				}
			}
			DM ("==> trigger [%d][%d]\n", trigger, trigger_close);

			/* up down up down down */
			switch (trigger)
			{
			case 0: if (ie.code != KEY_VOLUMEUP)	trigger = -1; break;
			case 1: if (ie.code != KEY_VOLUMEDOWN)	trigger = -1; break;
			case 2: if (ie.code != KEY_VOLUMEUP)	trigger = -1; break;
			case 3: if (ie.code != KEY_VOLUMEDOWN)	trigger = -1; break;
			case 4: 
				if (ie.code == KEY_VOLUMEDOWN)
				{
					if(is_wrsk_enabled()) {
						write_data_toJ(ATM_START_ALL);
					}
					set_bugreport_timeout(300);
					do_vibrate(3, 300 * 1000, 200);
					set_blockflag(2);
					dump_systrace();
					set_blockflag(2);
					dump_logbuffer();
					set_blockflag(2);
					do_bugreport ();
				}
				trigger = -1; 
				break;
			default: trigger = -1;
			}

			/*down up down up up */
			switch (trigger_close)
			{
			case 0: if (ie.code != KEY_VOLUMEDOWN)	trigger_close = -1; break;
			case 1: if (ie.code != KEY_VOLUMEUP)	trigger_close = -1; break;
			case 2: if (ie.code != KEY_VOLUMEDOWN)	trigger_close = -1; break;
			case 3: if (ie.code != KEY_VOLUMEUP)	trigger_close = -1; break;
			case 4: 
				if (ie.code == KEY_VOLUMEUP)
				{
					if(1==get_atrace_status())
					{
						clean_systrace();
						do_vibrate(2, 300 * 1000, 200);
					} else {
						init_atrace();
						do_vibrate(1, 10, 1000);
					}
				}
				trigger_close = -1; 
				break;
			default: trigger_close = -1;
			}
			
			if (is_combine_key)
			{
				combkey_last = combkey_now;
			}
			break;
		}
	}

	close_input_devices (fds);
	fds = NULL;
}
