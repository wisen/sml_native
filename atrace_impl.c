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

#define  VERSION "1.0"

const char* k_traceTagsProperty = "debug.atrace.tags.enableflags";

static const char *CONF_PATH = "/system/vendor/etc/SystraceEnabler.conf";
static const char *k_tracePath = "/sys/kernel/debug/tracing/trace";
static const char *k_traceBufferSizePath = "/sys/kernel/debug/tracing/buffer_size_kb";
static const char *attr_tracing_on = "/sys/kernel/debug/tracing/tracing_on";
static const char *attr_buffer_overrite = "/sys/kernel/debug/tracing/options/overwrite";
static const char *prop_trace_flags = "debug.atrace.tags.enableflags";
static const char *prefix_path = "/system/vendor/etc/tr_systrace_prefix";
static const char *postfix_path = "/system/vendor/etc/tr_systrace_postfix";

static const char *TRRACE_GRAPHICS = "TRACE_GRAPHICS";                 // 1L << 1
static const char *TRRACE_INPUT = "TRACE_INPUT";                       // 1L << 2
static const char *TRRACE_VIEW = "TRACE_VIEW";                         // 1L << 3
static const char *TRRACE_WEBVIEW = "TRACE_WEBVIEW";                   // 1L << 4
static const char *TRRACE_WINDOWS_MANAGER = "TRACE_WINDOWS_MANAGER";   // 1L << 5
static const char *TRRACE_ACTIVITY_MANAGER = "TRACE_ACTIVITY_MANAGER"; // 1L << 6
static const char *TRRACE_SYNC_MANAGER = "TRACE_SYNC_MANAGER";         // 1L << 7
static const char *TRRACE_AUDIO = "TRACE_AUDIO";                       // 1L << 8
static const char *TRRACE_VEDIO = "TRACE_VEDIO";                       // 1L << 9
static const char *TRRACE_ENABLE = "1";

const size_t FD_BUFFER_SIZE = 2 * 1024 * 1024; //buffer is 2048KB
const size_t FD_BUFFER_TMP_SIZE = 2 * 1024 * 1024 * 1.1; //buffer is 1024KB with extra 10% buffer

static int atrace_status = 0;//0:off 1:on

static char *debug_dump (const char *data, int len, char *ret, int buflen)
{
	const char *rec = "%02X(%c),";
	const int reclen = 6;	/* output length of rec */

	int i, count;
	int idx = 0;

	if (ret)
	{
		if (data && (len > 0))
		{
			count = (buflen / reclen) - 1;	/* maximum record count */

			if (len <= count)
			{
				count += 3;
			}

			for (i = 0; (i < len) && (count > 3); i ++, count --)
			{
				sprintf (& ret [idx], rec, (unsigned char) data [i], isprint (data [i]) ? data [i] : '.');
				idx += reclen;
			}

			if (i < len)	/* count == 3 */
			{
				strcpy (& ret [idx - 1], " ... ");
				idx += 4;

				for (i = len - 3; i < len; i ++)
				{
					sprintf (& ret [idx], rec, (unsigned char) data [i], isprint (data [i]) ? data [i] : '.');
					idx += reclen;
				}
			}
		}

		if ((idx > 0) && (ret [idx - 1] == ','))
			idx --;

		ret [idx] = 0;
	}
	return ret;
}

static int write_attr_file (const char *node, char *buf_value, int buf_value_size)
{
	char debug [512];
	int fd = -1;
	int ret = -1;

	if ((fd = open (node, O_WRONLY)) < 0)
	{
		ret = errno;

		DM ("open %s failed: %s\n", node, strerror (errno));

		return ret;
	}

	if ((ret = write (fd, buf_value, buf_value_size)) < 0)
	{
		ret = errno;

		DM ("write node %s (%d) failed: %s (%d)\n", node, fd, strerror (errno), errno);
	}
	else
	{
		DM ("write node %s (%d): %d bytes: [%s]\n", node, fd, ret, debug_dump (buf_value, ret, debug, sizeof (debug)));
	}

	close (fd);

	return ret;
}

static int set_trace_buffer_size_kb (int size)
{
	char str [32] = {0};

	if (size < 1)
	{
		size = 1;
	}

	snprintf (str, sizeof (str), "%d", size);

	return write_attr_file (k_traceBufferSizePath, str, strlen (str));
}

static int apply_config ()
{
	int ret = 0;
	char line [1024] = {0};
	char key [128] = {0};
	char value [128] = {0};
	FILE *file = NULL;

	if ((file = fopen (CONF_PATH, "r")) == NULL)
	{
		DM ("%s: open %s failed: %s\n", __func__, CONF_PATH, strerror (errno));

		return -1;
	}

	while (fgets (line, sizeof (line), file) != NULL)
	{
		memset (key, 0, sizeof (key));
		memset (value, 0, sizeof (value));
		sscanf (line, "%s = %s\n", key, value);

		if (*key == 0 || *value == 0)
		{
			DM ("%s: key=%s, value=%s, len=%d\n", __func__, key, value, strlen (value));
			DM ("%s:   invalid config: [%s]\n", __func__, line);
			continue;
		}
		else if (*key == '/')
		{
			write_attr_file (key, value, strlen (value));
		}
	}

	fclose (file);

	return ret;
}

int systrace_clean ()
{
	if (DEBUG) DM ("%s: begin\n", __func__);

	int traceFD = creat (k_tracePath, 0);

	if (traceFD == -1)
	{
		DM ("%s: error truncating %s: %s (%d)\n", __func__, k_tracePath, strerror (errno), errno);

		return false;
	}

	close (traceFD);

	if (DEBUG) DM ("%s: done\n", __func__);

	return true;
}

#define DEFAULT_TRACE_TAG  ATRACE_TAG_GRAPHICS|\
							ATRACE_TAG_INPUT|\
							ATRACE_TAG_VIEW|\
							ATRACE_TAG_WEBVIEW|\
							ATRACE_TAG_WINDOW_MANAGER|\
							ATRACE_TAG_ACTIVITY_MANAGER|\
							ATRACE_TAG_AUDIO|\
							ATRACE_TAG_VIDEO|\
							ATRACE_TAG_CAMERA|\
							ATRACE_TAG_HWUI|\
							ATRACE_TAG_PERF|\
							ATRACE_TAG_APP|\
							ATRACE_TAG_RESOURCES|\
							ATRACE_TAG_DALVIK|\
							ATRACE_TAG_RS|\
							ATRACE_TAG_POWER|\
							ATRACE_TAG_PACKAGE_MANAGER|\
							ATRACE_TAG_SYSTEM_SERVER

int systrace_init ()
{
	char buf[PROPERTY_VALUE_MAX];
	snprintf(buf, sizeof(buf), "%#" PRIx64, (uint64_t)DEFAULT_TRACE_TAG);

	if (property_set(k_traceTagsProperty, buf) < 0) {
		fprintf(stderr, "error setting trace tags system property\n");
		return false;
	}

	if (apply_config () < 0)
	{
		return -1;
	}

	write_attr_file ("/sys/kernel/debug/tracing/trace_clock", "global", strlen ("global"));

	write_attr_file (attr_buffer_overrite, "1", strlen ("1")); // rign buffer

	set_trace_buffer_size_kb(8192);

	return 0;
}

int systrace_on ()
{
	return write_attr_file (attr_tracing_on, "1", strlen ("1"));
}

int systrace_off ()
{
	return write_attr_file (attr_tracing_on, "0", strlen ("0"));
}

void set_atrace_status(int status)
{
	atrace_status = status;
}

int get_atrace_status()
{
	return atrace_status;
}

int systrace_dump ()
{
	gzFile gz_output = NULL;
	int fd_systrace = -1;
	int fd_prefix = -1;
	int fd_postfix = -1;
	int ret_read = -1;
	int ret_write = -1;
	int buf_read_idx = 0;
	int gz_error = 0;
	uint8_t *buf_read = NULL;
	uint8_t *buf_tmp = NULL;
	char output_dir [256] = {0};
	char output_filename [256] = {0};
	const char *GZ_OUT_MODE = "wb6f";
	const char *GZ_IN_MODE = "rb";
	time_t log_timestamp = time (NULL);
	struct tm *ptm = localtime (& log_timestamp);

	DM ("%s: begin with ver %s\n", __func__, VERSION);

	// set full dir path
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


	// set trace file name
	snprintf (output_filename, sizeof (output_filename), "%s/%s/systrace_%04d%02d%02d_%02d%02d%02d.gz",
		STORAGE, FOLDER_SYS, ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);

	output_filename [sizeof (output_filename) - 1] = 0;

	if ((buf_read = (uint8_t*) malloc (FD_BUFFER_SIZE)) == NULL)
	{
		DM ("%s: malloc %d bytes failed\n", __func__, FD_BUFFER_SIZE);
		goto end;
	}

	if ((buf_tmp = (uint8_t*) malloc (FD_BUFFER_TMP_SIZE)) == NULL)
	{
		DM ("%s: malloc %d bytes failed\n", __func__, FD_BUFFER_TMP_SIZE);
		goto end;
	}

	if ((gz_output = gzopen (output_filename, GZ_OUT_MODE)) == NULL)
	{
		DM ("%s: open %s failed: %s (%d)\n", __func__, output_filename, strerror (errno), errno);
		goto end;
	}

	if ((fd_systrace = open (k_tracePath, O_RDONLY)) < 0)
	{
		DM ("%s: open %s failed: %s (%d)\n", __func__, k_tracePath, strerror (errno), errno);
		goto end;
	}

       /*
	if ((fd_prefix = open (prefix_path, O_RDONLY)) < 0)
	{
		DM ("%s: open %s failed: %s (%d)\n", __func__, prefix_path, strerror (errno), errno);
		goto end;
	}

	if ((fd_postfix = open (postfix_path, O_RDONLY)) < 0)
	{
		DM ("%s: open %s failed: %s (%d)\n", __func__, postfix_path, strerror (errno), errno);
		goto end;
	}*/

	for (;;)
	{
		// add prefix to gz file
		/*
		if (fd_prefix >= 0)
		{
			ret_read = read (fd_prefix, buf_read, FD_BUFFER_SIZE);

			if (ret_read < 0)
			{
				DM ("%s: read prefix error: %s (%d)\n", __func__, strerror (errno), errno);
				goto end;
			}
			else if (ret_read == 0)
			{
				DM ("%s: prefix done\n", __func__);
				if (fd_prefix >= 0) close (fd_prefix);
				fd_prefix = -1;
				continue;
			}

			buf_read_idx = 0;

			for (;;)
			{
				if (buf_read_idx == ret_read) break;

				ret_write = gzwrite (gz_output, buf_read + buf_read_idx, ret_read - buf_read_idx);

				//returns the number of uncompressed bytes written or 0 in case of error
				if (ret_write > 0)
				{
					buf_read_idx += ret_write;
					DM ("%s: write prefix %d / %d bytes\n", __func__, buf_read_idx, ret_read);
				}
				else
				{
					DM ("%s: write prefix error: %s\n", __func__, gzerror (gz_output, &gz_error));
					goto end;
				}
			}

			continue;
		}*/

		// add sys log to gz file
		if (fd_systrace >= 0)
		{
			ret_read = read (fd_systrace, buf_read, FD_BUFFER_SIZE);
			if (ret_read < 0)
			{
				DM ("%s: read body error: %s (%d)\n", __func__, strerror (errno), errno);
				goto end;
			}
			else if (ret_read == 0)
			{
				DM ("%s: body done\n", __func__);
				if (fd_systrace >= 0) close (fd_systrace);
				fd_systrace = -1;
				//continue;
				break;
			}

			ret_read = str_replace (buf_read, buf_tmp, ret_read);
			buf_read_idx = 0;

			for (;;)
			{
				if (buf_read_idx == ret_read) break;

				ret_write = gzwrite (gz_output, buf_tmp + buf_read_idx, ret_read - buf_read_idx);
				//returns the number of uncompressed bytes written or 0 in case of error
				if (ret_write >= 0)
				{
					buf_read_idx += ret_write;
					if (DEBUG) DM ("%s: write body %d / %d bytes\n", __func__, buf_read_idx, ret_read);
				}
				else
				{
					DM ("%s: write body error: %s\n", __func__, gzerror (gz_output, &gz_error));
					goto end;
				}
			}

			continue;
		}

		// add postfix to gz file
		/*
		if (fd_postfix >= 0)
		{
			ret_read = read (fd_postfix, buf_read, FD_BUFFER_SIZE);

			if (ret_read < 0)
			{
				DM ("%s: read postfix error: %s (%d)\n", __func__, strerror (errno), errno);
				goto end;
			}
			else if (ret_read == 0)
			{
				DM ("%s: postfix done\n", __func__);
				if (fd_postfix > 0) close (fd_postfix);
				fd_postfix = -1;
				break;
			}

			buf_read_idx = 0;

			for (;;)
			{
				if (buf_read_idx == ret_read) break;

				ret_write = gzwrite (gz_output, buf_read + buf_read_idx, ret_read - buf_read_idx);

				//returns the number of uncompressed bytes written or 0 in case of error
				if (ret_write > 0)
				{
					buf_read_idx += ret_write;
					DM ("%s: write postfix %d / %d bytes\n", __func__, buf_read_idx, ret_read);
				}
				else
				{
					DM ("%s: write postfix error: %s\n", __func__, gzerror (gz_output, &gz_error));
					goto end;
				}
			}

			continue;
		}
		*/
	}

	//move and zip the log file
	//send_logcontrol (0, 2, 0, reserved_num, log_timestamp, output_dir);

end:;
	/*
	if (fd_prefix >= 0)
	{
		close (fd_prefix);
		fd_prefix = -1;
	}

	if (fd_postfix >= 0)
	{
		close (fd_postfix);
		fd_postfix = -1;
	}*/

	if (fd_systrace >= 0)
	{
		close (fd_systrace);
		fd_systrace = -1;
	}

	if (gz_output)
	{
		gzclose (gz_output);
		gz_output = NULL;
	}

	if (buf_read)
	{
		free (buf_read);
		buf_read = NULL;
	}

	if (buf_tmp)
	{
		free (buf_tmp);
		buf_tmp = NULL;
	}

	time_t time_end = time (NULL);

	DM ("%s: %s done: %.3lf sec\n", __func__, output_filename, difftime (time_end, log_timestamp));

	/*
	std::vector<std::string> am_args = {
		"--receiver-permission", "android.permission.DUMP",
	};
	send_broadcast("android.intent.action.SYSTRACE_FINISHED", am_args);
	*/

	return 0;
}

void init_atrace()
{
	if (DEBUG) DM("%s\n", __FUNCTION__);
	systrace_init();
	systrace_on();
	set_atrace_status(1);
}

void dump_systrace()
{
	if (DEBUG) DM("%s\n", __FUNCTION__);
	systrace_off();
	systrace_dump();
	systrace_clean();
	systrace_on();
	set_blockflag(0);
}

void clean_systrace()
{
	if (DEBUG) DM("%s\n", __FUNCTION__);
	systrace_off();
	{
		char buf[PROPERTY_VALUE_MAX];
		snprintf(buf, sizeof(buf), "%#" PRIx64, (uint64_t)0);

		if (property_set(k_traceTagsProperty, buf) < 0) {
			fprintf(stderr, "error setting trace tags system property\n");
			return;
		}
	}
	systrace_clean();
	set_atrace_status(0);
}