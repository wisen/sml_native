#ifndef __VIBRATOR_IMPL_H__
#define __VIBRATOR_IMPL_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void vibrate(FILE* vibrator, int ms);
void do_vibrate(int times, int interval, int ms);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif