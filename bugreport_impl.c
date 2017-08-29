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
#include <linux/input.h>
#include <cutils/properties.h>

#include "headers/common.h"
#define	BUGREPORT	"/system/bin/bugreport"
#define	DUMPSTATE	"/system/bin/dumpstate"

time_t bugreport_start_time;
static int is_bugreporting = 0;
//char buffer [PATH_MAX];
static int bugreport_timeout = 300;

void set_bugreport_timeout(int timeout)
{
	bugreport_timeout = timeout;
}

int get_bugreport_timeout()
{
	return bugreport_timeout;
}

void set_bugreport(int flag)
{
	is_bugreporting = flag;
}

time_t get_bugreport_duration(time_t now)
{
	if(is_bugreporting)
		return now - bugreport_start_time;
	else
		return 0;
}

int do_bugreport ()
{
	/*
	struct stat st;
	char buf [PATH_MAX];
	int vibrate_fd, ret;
	char output_dir [256] = {0};
	char output_filename [256] = {0};
	time_t log_timestamp = time (NULL);
	struct tm *ptm = localtime (& log_timestamp);

	snprintf (output_dir, sizeof (output_dir), "%s/%s", STORAGE, FOLDER_SYS);
	output_dir [sizeof (output_dir) - 1] = 0;
	if(!dir_exists(output_dir)) {
		if(mkdir (output_dir, DEFAULT_DIR_MODE) < 0) {
			DM ("mkdir [%s]: %s\n", output_dir, strerror (errno));
			return -1;
		}
	}
	//if (dir_create_recursive (output_dir) < 0)
	//	return -1;

	//snprintf (output_filename, sizeof (output_filename), "%s/%s/bugreport_%04d%02d%02d_%02d%02d%02d.gz",
	//	STORAGE, FOLDER_SYS, ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
	snprintf (output_filename, sizeof (output_filename), "%s/%s/bugreport_", STORAGE, FOLDER_SYS);

	output_filename [sizeof (output_filename) - 1] = 0;

	snprintf (buf, sizeof (buf) - 1, DUMPSTATE " -d -z -p -o %s", output_filename);
	ret = (system (buf) == 0) ? 0 : -1;

	DM ("end of [%s][%d]\n", buf, ret);

	return ret;
	*/
	//property_set("ctl.start", "dpstate");
	bugreport_start_time = time(NULL);
	property_set("debug.atrace_monitor.flag", "start_dumpstate");
	set_bugreport(1);
	sleep(3);
	property_set("debug.atrace_monitor.flag", "stop_dumpstate");
	/*
	snprintf (buffer, sizeof (buffer), "/system/bin/start dpstat", " -d -p -B -o /sdcard/trlog/bugreport");
	buffer [sizeof (buffer) - 1] = 0;
	system(buffer);
	*/
	return 0;
}
