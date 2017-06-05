#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include "headers/vibrator_impl.h"

void vibrate(FILE* vibrator, int ms) {
    fprintf(vibrator, "%d\n", ms);
    fflush(vibrator);
}

void do_vibrate(int times, int interval, int ms) {
    int i;
    std::unique_ptr<FILE, int(*)(FILE*)> vibrator(NULL, fclose);
    vibrator.reset(fopen("/sys/class/timed_output/vibrator/enable", "we"));

    for (i=0; i<times; i++) {
        vibrate(vibrator.get(), ms);
        usleep(interval);
    }
}