

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
    char *msg;

    switch (exceptionInfo->code) {
    case TRAP_ILLEGAL_ILLOPC:
        msg = "Illegal opcode";
        break;
    case TRAP_ILLEGAL_ILLOPN:
        msg = "Illegal operand";
        break;
    case TRAP_ILLEGAL_ILLADR:
        msg = "Illegal addressing mode";
        break;
    case TRAP_ILLEGAL_ILLTRP:
        msg = "Illegal software trap";
        break;
    case TRAP_ILLEGAL_PRVOPC:
        msg = "Privileged opcode";
        break;
    case TRAP_ILLEGAL_PRVREG:
        msg = "Privileged register";
        break;
    case TRAP_ILLEGAL_COPROC:
        msg = "Coprocessor error";
        break;
    case TRAP_ILLEGAL_BADSTK:
        msg = "Bad stack";
        break;
    case TRAP_ILLEGAL_KERNELI:
        msg = "Linux kernel sent SIGILL";
        break;
    case TRAP_ILLEGAL_USERIB:
        msg = "Received SIGILL or SIGBUS from user";
        break;
    case TRAP_ILLEGAL_ADRALN:
        msg = "Invalid address alignment";
        break;
    case TRAP_ILLEGAL_ADRERR:
        msg = "Non-existent physical address";
        break;
    case TRAP_ILLEGAL_OBJERR:
        msg = "Object-specific HW error";
        break;
    case TRAP_ILLEGAL_KERNELB:
        msg = "Linux kernel sent SIGBUS";
        break;
    }

    printf("ERROR: Process %d terminated. Reason: %s\n",
        active_process->pid, msg);

    KernelExit(ERROR);
}

/*
 * Interrupt handler for TRAP_MEMORY interrupt.
 */
void trap_memory_handler(ExceptionInfo *exceptionInfo) {
    int i;

    TracePrintf(0, "TRAP_MEMORY - pid = %d\n", active_process->pid);

    void *addr = exceptionInfo->addr;

    if (addr > USER_STACK_LIMIT) {
        printf("ERROR: Process %d attempted to access an invalid address: %p\n",
            active_process->pid, addr);
        KernelExit(ERROR);
    }

    // Check if expanding stack pushes into the heap.
    if (DOWN_TO_PAGE(addr) - PAGESIZE < active_process->user_brk) {
        printf("ERROR: Stackoverflow, process %d\n", active_process->pid);
        KernelExit(ERROR);
    }

    int heap_pages = ((long)active_process->user_brk - MEM_INVALID_SIZE)
        >> PAGESHIFT;
    int stack_pages = active_process->user_pages - heap_pages;

    void *cur_stack = USER_STACK_LIMIT - (stack_pages << PAGESHIFT);

    int num_new_pages = (DOWN_TO_PAGE(cur_stack) - DOWN_TO_PAGE(addr))
        >> PAGESHIFT;

    if (num_new_pages > tot_pmem_pages - allocated_pages) {
        printf("ERROR: Unable to allocate memory for stack, process %d\n",
            active_process->pid);
        KernelExit(ERROR);
    }

    struct pte *page_table = (struct pte *)(VMEM_LIMIT - PAGESIZE);

    for (i = 0; i < num_new_pages; ++i) {
        *(page_table + (USER_STACK_LIMIT >> PAGESHIFT) - stack_pages - i - 1) =
            (struct pte) {
            .pfn = alloc_page(),
            .uprot = PROT_READ | PROT_WRITE,
            .kprot = PROT_READ | PROT_WRITE,
            .valid = 1
        };
    }

}

/*
 * Interrupt handler for TRAP_MATH interrupt.
 */
void trap_math_handler(ExceptionInfo *exceptionInfo) {
    char *msg;

    switch (exceptionInfo->code) {
    case TRAP_MATH_INTDIV:
        msg = "Integer divide by zero";
        break;
    case TRAP_MATH_INTOVF:
        msg = "Integer overflow";
        break;
    case TRAP_MATH_FLTDIV:
        msg = "Floating divide by zero";
        break;
    case TRAP_MATH_FLTOVF:
        msg = "Floating overflow";
        break;
    case TRAP_MATH_FLTUND:
        msg = "Floating underflow";
        break;
    case TRAP_MATH_FLTRES:
        msg = "Floating inexact result";
        break;
    case TRAP_MATH_FLTINV:
        msg = "Invalid floating operation";
        break;
    case TRAP_MATH_FLTSUB:
        msg = "FP subscript out of range";
        break;
    case TRAP_MATH_KERNEL:
        msg = "Linux kernel sent SIGFPE";
        break;
    case TRAP_MATH_USER:
        msg = "Received SIGFPE from user";
        break;
    }

    printf("ERROR: Process %d terminated. Reason: %s\n",
        active_process->pid, msg);

    KernelExit(ERROR);
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
    Halt();
}