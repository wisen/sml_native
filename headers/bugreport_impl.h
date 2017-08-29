#ifndef	__BUGREPORT_IMPL_H__
#define	__BUGREPORT_IMPL_H__
#include <time.h>

//when bugreport had doing over 5 minites, we will stop dumpstate service
//#define BUGREPORT_TIMEOUT 300

void set_bugreport(int flag);
time_t get_bugreport_duration(time_t now);
void set_bugreport_timeout(int timeout);
int get_bugreport_timeout();


#endif
