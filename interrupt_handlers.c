

#include "kernel.h"

/*
 * Interrupt handler for TRAP_KERNEL interrupt.
 */
void trap_kernel_handler(ExceptionInfo *exceptionInfo) {

    switch (exceptionInfo->code) {
    case YALNIX_FORK:
        exceptionInfo->regs[0] = KernelFork();
        break;

    case YALNIX_EXEC:
        KernelExec(exceptionInfo);
        break;

    case YALNIX_EXIT:
        KernelExit((int) (exceptionInfo->regs[1]));

    case YALNIX_WAIT:
        exceptionInfo->regs[0] =
            KernelWait((int *) (exceptionInfo->regs[1]));
        break;

    case YALNIX_GETPID:
        exceptionInfo->regs[0] = active_process->pid;
        break;

    case YALNIX_BRK:
        exceptionInfo->regs[0] = KernelBrk(
            (void *) (exceptionInfo->regs[1]));
        break;

    case YALNIX_DELAY:
        exceptionInfo->regs[0] = KernelDelay(
            (int) (exceptionInfo->regs[1]));
        break;

    case YALNIX_TTY_READ:
        exceptionInfo->regs[0] = KernelTtyRead(
            (int) (exceptionInfo->regs[1]),
            (void *) (exceptionInfo->regs[2]),
            (int) (exceptionInfo->regs[3]));
        break;

    case YALNIX_TTY_WRITE:
        exceptionInfo->regs[0] = KernelTtyWrite(
            (int) (exceptionInfo->regs[1]),
            (void *) (exceptionInfo->regs[2]),
            (int) (exceptionInfo->regs[3]));
        break;

    default :
        TracePrintf(0, "invalid trap call");
        Halt();
    }

}

/*
 * Interrupt handler for TRAP_CLOCK interrupt.
 */
void trap_clock_handler(ExceptionInfo *exceptionInfo) {
    TracePrintf(1, "trap_clock_handler - Active process: %u\n",
                active_process->pid);

    struct process_info* temp;

    // Decrement all waiting processes and move any completed
    // processes onto the ready queue.
    struct process_info *waiting_process = waiting_queue;
    while (waiting_process != NULL) {
        TracePrintf(1, "decrementing waiting_process process %d\n", waiting_process->pid);
        waiting_process->delay_ticks--;
        TracePrintf(1, "new value %d\n", waiting_process->delay_ticks);

        if (waiting_process->delay_ticks < 1) {
            TracePrintf(1, "process %d unblocking\n", waiting_process->pid);

            // Remove process from waiting queue
            remove_process(&waiting_queue, &wq_tail, waiting_process);

            // Push process onto process queue
            push_process(&process_queue, &pq_tail, waiting_process);

        }
        else {
            waiting_process = waiting_process->next_process;
        }
    }

    // If any processes are available, switch to them.
    TracePrintf(1, "pq before switch %p\n", (void*)process_queue);
    if ((last_switch - (++clock_count) <= -2 || active_process->pid == 0)
        && process_queue != NULL) {
        struct process_info *next = pop_process(&process_queue, &pq_tail);

        TracePrintf(1, "Popped process %d off queue\n", next->pid);

        last_switch = clock_count;

        // If active process is not idle, return it to the queue.
        if (active_process->pid != idle->pid)
            push_process(&process_queue, &pq_tail, active_process);

        ContextSwitch(ContextSwitchFunc, &(active_process->ctx),
                      (void *)active_process, (void *)next);
    }
}

/*
 * Interrupt handler for TRAP_ILLEGAL interrupt.
 */
void trap_illegal_handler(ExceptionInfo *exceptionInfo) {
    TracePrintf(1, "trap_illegal_handler");
    Halt();
}

/*
 * Interrupt handler for TRAP_MEMORY interrupt.
 */
void trap_memory_handler(ExceptionInfo *exceptionInfo) {
    TracePrintf(1, "trap_memory_handler");
    Halt();
}

/*
 * Interrupt handler for TRAP_MATH interrupt.
 */
void trap_math_handler(ExceptionInfo *exceptionInfo) {
    TracePrintf(1, "trap_math_handler");
    Halt();
}

/*
 * Interrupt handler for TRAP_TTY_TRANSMIT interrupt.
 */
void trap_tty_transmit_handler(ExceptionInfo *exceptionInfo) {
    TracePrintf(1, "trap_tty_transmit_handler");
    Halt();
}

/*
 * Interrupt handler for TRAP_TTY_RECEIVE interrupt.
 */
void trap_tty_receive_handler(ExceptionInfo *exceptionInfo) {
    TracePrintf(1, "trap_tty_receive_handler");

    //get the correct terminal info
    int term = info->code;
    struct terminal_info *terminal = terminals[term];

    //read the line into a buffer
    char *newline = (char *)malloc(TERMINAL_MAX_LINE);
    int len = TtyReceive(term, newline, TERMINAL_MAX_LINE);

    //initialize an avaliable line struct to represent this input line
    struct avaliable_line *new = (struct avaliable_line *) malloc(sizeof(struct avaliable_line));
        *new = (struct avaliable_line) {
            .line = newline,
            .orig_ptr = newline,
            .free = 1,
            .len = len,
            .next = NULL
        }

    //pop process off the terminal's read wait queue
    struct process_info *popped = pop_process(&terminal->r_head, &terminal->r_tail);
    //while the queue was not empty
    while (popped != NULL) {
        //if the popped process was seeking less characters than the line len
        if (popped->seeking_len < len) {
            //make a new avaliable line struct to attach to pcb
            struct available_line *newnew = (struct avaliable_line *) malloc(sizeof(struct avaliable_line));
            *newnew = (struct avaliable_line) {
                .line = newline,
                .free = 0,
                .len = popped->seeking_len,
                .next = NULL
            }
            //attach the line to process pcb and return it to the process queue
            popped->line = newnew;
            push_process(&process_queue, &pq_tail, popped);

            //update the input line
            new->line += popped->seeking_len;
            new->len -= popped->seeking_len;

            len -= popped->seeking_len;
        }
        //popped process consumes the rest of the line
        else {
            popped->line = new;
            push_process(&process_queue, &pq_tail);
            len = 0;
            break;
        }
        popped = pop_process(&terminal->r_head, &terminal->r_tail);
    }
    
    //add any remaining line input to the avaliable lines
    if(len > 0) {
        if (terminal->next_line == NULL) {
        terminal->next_line = new;
        terminal->last_line = new;
        }
        else {
            (terminal->last_line)->next = new;
            terminal->last_line = (terminal->last_line)->next; 
        }
    }


}