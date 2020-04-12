
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
    active_process->delay_ticks = clock_ticks;
    if (waiting_queue != NULL) {
    	TracePrintf(0, "adding to waiting_queue\n");
    	wq_tail->next_process = active_process;
    	wq_tail = wq_tail->next_process;
    }
    else {
    	TracePrintf(0, "starting waiting_queue\n");
    	wq_tail = active_process;
    	waiting_queue = active_process;
    }
    if (process_queue != NULL) {
    	struct process_info *next = process_queue;
    	ContextSwitch(ContextSwitchFunc, &active_process->ctx,
            (void *)active_process, (void *)next);
    }
    else {
    	TracePrintf(1, "attempting switch from pid %d to pid %d\n"), active_process->pid, idle->pid;
    	TracePrintf(1, "%p\n", (void*)(idle->page_table << PAGESHIFT));
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

