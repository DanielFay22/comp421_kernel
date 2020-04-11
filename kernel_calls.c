

#include "kernel.h"




int Fork(void) {
    return 0;
}


int Exec(char *filename, char **argvec) {
    return 0;
}


void Exit(int status) {
    Halt();
}

int Wait(int *status_ptr) {
    return 0;
}

int GetPid(void) {
    return active_process->pid;
}

int Brk(void *addr) {
    return 0;
}

int Delay(int clock_ticks) {

    if (clock_ticks < 0)
        return ERROR;

    while (clock_ticks-- > 0)
        Pause();

    return 0;
}

int TtyRead(int tty_id, void *buf, int len) {
    return 0;
}

int TtyWrite(int tty_id, void *buf, int len) {
    return 0;
}

