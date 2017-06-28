#ifndef	__ATRACE_IMPL_H__
#define	__ATRACE_IMPL_H__

int systrace_clean ();
int systrace_clean ();
int systrace_on ();
int systrace_off ();
int systrace_dump ();
void init_atrace();
void dump_systrace();
int get_atrace_status();
void clean_systrace();

#endif
