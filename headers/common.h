#ifndef __ATRACE_COMMON_H__
#define __ATRACE_COMMON_H__
#include  <android/log.h>

#define DEBUG 0

#define LOG_TAG "ATM"
#define DM(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
//#define LOG_TAG "ATM:main"
//#define DM(...) printf("["LOG_TAG"]: " __VA_ARGS__)
//#define DM(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

#define STORAGE "/sdcard"
#define FOLDER_SYS   "trlog"

#define  DEFAULT_DIR_MODE	(0777)
#define  DEFAULT_FILE_MODE	(0666)

#define KEY_TIMEOUT 2 // 2senconds

#define ATRACE_TAG_NEVER            0       // This tag is never enabled.
#define ATRACE_TAG_ALWAYS           (1<<0)  // This tag is always enabled.
#define ATRACE_TAG_GRAPHICS         (1<<1)
#define ATRACE_TAG_INPUT            (1<<2)
#define ATRACE_TAG_VIEW             (1<<3)
#define ATRACE_TAG_WEBVIEW          (1<<4)
#define ATRACE_TAG_WINDOW_MANAGER   (1<<5)
#define ATRACE_TAG_ACTIVITY_MANAGER (1<<6)
#define ATRACE_TAG_SYNC_MANAGER     (1<<7)
#define ATRACE_TAG_AUDIO            (1<<8)
#define ATRACE_TAG_VIDEO            (1<<9)
#define ATRACE_TAG_CAMERA           (1<<10)
#define ATRACE_TAG_HWUI             (1<<11)
#define ATRACE_TAG_PERF             (1<<12)
#define ATRACE_TAG_HAL              (1<<13)
#define ATRACE_TAG_APP              (1<<14)
#define ATRACE_TAG_RESOURCES        (1<<15)
#define ATRACE_TAG_DALVIK           (1<<16)
#define ATRACE_TAG_RS               (1<<17)
#define ATRACE_TAG_BIONIC           (1<<18)
#define ATRACE_TAG_POWER            (1<<19)
#define ATRACE_TAG_PACKAGE_MANAGER  (1<<20)
#define ATRACE_TAG_SYSTEM_SERVER    (1<<21)
#define ATRACE_TAG_DATABASE         (1<<22)
#define ATRACE_TAG_LAST             ATRACE_TAG_DATABASE

int dir_exists (const char *path);
int dir_create_recursive (const char *path);
int unlink_file (const char *path);
int str_replace (uint8_t *orig, uint8_t *repl, int len);

//extern static int isblock_fromUI;

void set_blockflag (int flag);
int get_blockflag();
void dump_systrace();
int do_bugreport ();

#define	SAFE_SPRINTF(b,l,f,...)\
{\
	void *__pb = b;\
	if (__pb && (l > 0))\
	{\
		snprintf (b, l, f, __VA_ARGS__);\
		b [l - 1] = 0;\
	}\
}

#define PATH_MAX 256

void do_vibrate(int times, int interval, int ms);
#endif
