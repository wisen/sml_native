#ifndef	__LOGCAT_IMPL_H__
#define	__LOGCAT_IMPL_H__
#include <time.h>

#define LOGBUFFER_SIZE "2M"
#define LOGBUFPROP "debug.logbuffersz.prop"

char buffer [PATH_MAX];
char tag_with_path [PATH_MAX];

void init_logcat();
int dump_logbuffer();

#endif
