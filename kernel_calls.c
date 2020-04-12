
#include <stddef.h>
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
	TracePrintf(0, "hello from delay from active process %d\n", active_process->pid);
    if (clock_ticks < 0)
        return ERROR;
    else if (clock_ticks == 0)
        return 0;

    active_process->delay_ticks = clock_ticks;
    push_process(&waiting_queue, &wq_tail, active_process);

    if (process_queue != NULL) {
    	struct process_info *next = pop_process(&process_queue, &pq_tail);
    	ContextSwitch(ContextSwitchFunc, &active_process->ctx,
            (void *)active_process, (void *)next);
    }
    else {
    	TracePrintf(1, "Switching from pid %d to idle process\n",
    	    active_process->pid, idle->pid);
    	ContextSwitch(ContextSwitchFunc, &active_process->ctx,
    		(void*)active_process, (void*)idle);
    }
    return 0;
 }

int TtyRead(int tty_id, void *buf, int len) {
    return 0;
}

int TtyWrite(int tty_id, void *buf, int len) {
    return 0;
}

