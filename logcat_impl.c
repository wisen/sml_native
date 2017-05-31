#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <inttypes.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <utils/Log.h>
#include <cutils/properties.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <time.h>
#include <zlib.h>
#include <dirent.h>

#include "headers/common.h"
#define LOGBUFFER_SIZE "2M"

char buffer [PATH_MAX];
char tag_with_path [PATH_MAX];

void init_logcat()
{
/*
	char *argv [6];
	argv [0] = "/system/bin/logcat"; // the default is -b main -b system -b crash
	argv [1] = "-v";
	argv [2] = "threadtime";
	argv [3] = "-G";
	argv [4] = LOGBUFFER_SIZE; // set logbuffer size
	argv [5] = NULL;

	DM ("execv %s\n", argv);
	execv (argv [0], argv);
	DM ("execv: %s\n", strerror (errno));
	exit (127);
	*/
	SAFE_SPRINTF (buffer, sizeof (buffer), "/system/bin/logcat -G %s", LOGBUFFER_SIZE);
	DM ("%s\n", buffer);
	system (buffer);
}

int dump_logbuffer()
{
	char output_dir [256] = {0};
	char output_filename_main [256] = {0};
	char output_filename_system [256] = {0};
	char output_filename_crash [256] = {0};
	char output_filename_events [256] = {0};
	char output_filename_radio [256] = {0};
	time_t log_timestamp = time (NULL);
	struct tm *ptm = localtime (& log_timestamp);
	// set full dir path
	snprintf (output_dir, sizeof (output_dir), "%s/%s", STORAGE, FOLDER_SYS);
	output_dir [sizeof (output_dir) - 1] = 0;
	if(!dir_exists(output_dir)) {
		if(mkdir (output_dir, DEFAULT_DIR_MODE) < 0) {
			DM ("mkdir [%s]: %s\n", output_dir, strerror (errno));
			return -1;
		}
	}

	/*
	char *argv [9];
	argv [0] = "/system/bin/logcat"; // the default is -b main -b system -b crash
	argv [1] = "-v";
	argv [2] = "threadtime";
	argv [3] = "-b";
	//argv [4] = LOGBUFFER_SIZE; // set logbuffer size
	argv [5] = "-d";
	argv [6] = "-f";
	//argv[7]="";
	argv [8] = NULL;

	snprintf (output_filename, sizeof (output_filename), "%s/%s/main_%04d%02d%02d_%02d%02d%02d.txt",
		STORAGE, FOLDER_SYS, ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
	output_filename [sizeof (output_filename) - 1] = 0;
	argv [4] = "main";
	argv [7] = output_filename;
	execv (argv [0], argv);
	DM ("execv: %s\n", strerror (errno));

	snprintf (output_filename1, sizeof (output_filename1), "%s/%s/system_%04d%02d%02d_%02d%02d%02d.txt",
		STORAGE, FOLDER_SYS, ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
	output_filename1 [sizeof (output_filename1) - 1] = 0;
	argv [4] = "system";
	argv [7] = output_filename1;
	execv (argv [0], argv);
	DM ("execv: %s\n", strerror (errno));

	snprintf (output_filename2, sizeof (output_filename2), "%s/%s/crash_%04d%02d%02d_%02d%02d%02d.txt",
		STORAGE, FOLDER_SYS, ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
	output_filename2 [sizeof (output_filename2) - 1] = 0;
	argv [4] = "crash";
	argv [7] = output_filename2;
	execv (argv [0], argv);
	DM ("execv: %s\n", strerror (errno));
	*/
	snprintf (output_filename_main, sizeof (output_filename_main), "%s/%s/main_%04d%02d%02d_%02d%02d%02d.txt",
		STORAGE, FOLDER_SYS, ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
	output_filename_main [sizeof (output_filename_main) - 1] = 0;
	SAFE_SPRINTF (buffer, sizeof (buffer), "/system/bin/logcat -v threadtime -b %s -d -f %s", "main", output_filename_main)
	DM ("%s\n", buffer);
	system (buffer);

	snprintf (output_filename_system, sizeof (output_filename_system), "%s/%s/system_%04d%02d%02d_%02d%02d%02d.txt",
		STORAGE, FOLDER_SYS, ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
	output_filename_system [sizeof (output_filename_system) - 1] = 0;
	SAFE_SPRINTF (buffer, sizeof (buffer), "/system/bin/logcat -v threadtime -b %s -d -f %s", "system", output_filename_system)
	DM ("%s\n", buffer);
	system (buffer);

	snprintf (output_filename_crash, sizeof (output_filename_crash), "%s/%s/crash_%04d%02d%02d_%02d%02d%02d.txt",
		STORAGE, FOLDER_SYS, ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
	output_filename_crash [sizeof (output_filename_crash) - 1] = 0;
	SAFE_SPRINTF (buffer, sizeof (buffer), "/system/bin/logcat -v threadtime -b %s -d -f %s", "crash", output_filename_crash)
	DM ("%s\n", buffer);
	system (buffer);

	snprintf (output_filename_events, sizeof (output_filename_events), "%s/%s/events_%04d%02d%02d_%02d%02d%02d.txt",
		STORAGE, FOLDER_SYS, ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
	output_filename_events [sizeof (output_filename_events) - 1] = 0;
	SAFE_SPRINTF (buffer, sizeof (buffer), "/system/bin/logcat -v threadtime -b %s -d -f %s", "events", output_filename_events)
	DM ("%s\n", buffer);
	system (buffer);

	snprintf (output_filename_radio, sizeof (output_filename_radio), "%s/%s/radio_%04d%02d%02d_%02d%02d%02d.txt",
		STORAGE, FOLDER_SYS, ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
	output_filename_radio [sizeof (output_filename_radio) - 1] = 0;
	SAFE_SPRINTF (buffer, sizeof (buffer), "/system/bin/logcat -v threadtime -b %s -d -f %s", "radio", output_filename_radio)
	DM ("%s\n", buffer);
	system (buffer);
	return 0;
}
