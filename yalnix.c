
#include <stddef.h>
#include <stdio.h>


#include "kernel.h"



// Interrupt handlers
void trap_kernel_handler(ExceptionInfo *exceptionInfo);
void trap_clock_handler(ExceptionInfo *exceptionInfo);
void trap_illegal_handler(ExceptionInfo *exceptionInfo);
void trap_memory_handler(ExceptionInfo *exceptionInfo);
void trap_math_handler(ExceptionInfo *exceptionInfo);
void trap_tty_transmit_handler(ExceptionInfo *exceptionInfo);
void trap_tty_receive_handler(ExceptionInfo *exceptionInfo);

void idle_process(void);



void (*interrupt_table[TRAP_VECTOR_SIZE])(ExceptionInfo *) = {NULL};



void trap_kernel_handler(ExceptionInfo *exceptionInfo) {

    switch (exceptionInfo->code) {
        case YALNIX_FORK:
            exceptionInfo->regs[0] = Fork();
            break;

        case YALNIX_EXEC:
            exceptionInfo->regs[0] = Exec(
                (char *) (exceptionInfo->regs[1]),
                (char **) (exceptionInfo->regs[2]));
            break;

        case YALNIX_EXIT:
            Exit((int) (exceptionInfo->regs[1]));

        case YALNIX_WAIT:
            exceptionInfo->regs[0] =
                Wait((int *) (exceptionInfo->regs[1]));
            break;

        case YALNIX_GETPID:
            exceptionInfo->regs[0] = GetPid();
            break;

        case YALNIX_BRK:
            exceptionInfo->regs[0] = Brk(
                (void *) (exceptionInfo->regs[1]));
            break;

        case YALNIX_DELAY:
            exceptionInfo->regs[0] = Delay(
                (int) (exceptionInfo->regs[1]));
            break;

        case YALNIX_TTY_READ:
            exceptionInfo->regs[0] = TtyRead(
                (int) (exceptionInfo->regs[1]),
                (void *) (exceptionInfo->regs[2]),
                (int) (exceptionInfo->regs[3]));
            break;

        case YALNIX_TTY_WRITE:
            exceptionInfo->regs[0] = TtyWrite(
                (int) (exceptionInfo->regs[1]),
                (void *) (exceptionInfo->regs[2]),
                (int) (exceptionInfo->regs[3]));
            break;

        default :
            TracePrintf(0, "invalid trap call");
            Halt();
    }

}

void trap_clock_handler(ExceptionInfo *exceptionInfo) {
    TracePrintf(1, "trap_clock_handler - Active process: %u\n",
        active_process->pid);

    // If any processes are available, switch to them.
    if (last_switch - (++clock_count) >= 2 && process_queue != NULL) {
        struct process_info *next = process_queue;
        process_queue = process_queue->next_process;

        last_switch = clock_count;

        ContextSwitch(ContextSwitchFunc, &active_process->ctx,
            (void *)active_process, (void *)next);
    }
}

void trap_illegal_handler(ExceptionInfo *exceptionInfo) {
    TracePrintf(1, "trap_illegal_handler");
    Halt();
}

void trap_memory_handler(ExceptionInfo *exceptionInfo) {
    TracePrintf(1, "trap_memory_handler");
    Halt();
}

void trap_math_handler(ExceptionInfo *exceptionInfo) {
    TracePrintf(1, "trap_math_handler");
    Halt();
}

void trap_tty_transmit_handler(ExceptionInfo *exceptionInfo) {
    TracePrintf(1, "trap_tty_transmit_handler");
    Halt();
}

void trap_tty_receive_handler(ExceptionInfo *exceptionInfo) {
    TracePrintf(1, "trap_tty_receive_handler");
    Halt();
}


// Draft of context switch function
SavedContext *ContextSwitchFunc(SavedContext *ctxp,
    void *p1, void *p2) {

    struct process_info *curProc = (struct process_info *)p1;
    struct process_info *newProc = (struct process_info *)p2;

    curProc->ctx = *ctxp;

    // If the running process is not the idle process, put it back on the queue.
    if (curProc->pid != 0) {
        if (process_queue == NULL)
            process_queue = curProc;
        else
            process_queue->next_process = curProc;
    }

    active_process = newProc;

    // Set region 0 page table to new process table
    WriteRegister(REG_PTR0, (RCS421RegVal) newProc->page_table);

    // Flush region 0 entries from the TLB
    WriteRegister(REG_TLB_FLUSH, (RCS421RegVal) TLB_FLUSH_0);

    return &newProc->ctx;
}


struct pte *get_new_page_table() {
    int i;

    struct pte *table = (struct pte *)malloc(sizeof(struct pte));

    for (i = 0; i < MEM_INVALID_PAGES; ++i)
        (table + i)->valid = 0;



    // TODO: allocate and initializing a new page table
    return table;
}


int alloc_page(void) {
    int i;

    // No physical memory available
    if (allocated_pages == tot_pmem_pages)
        return -1;

    // Find next available page
    for (i = 0; i < tot_pmem_pages; ++i) {
        if ((free_pages + i)->in_use == 0) {
            (free_pages + i)->in_use = 1;
            ++allocated_pages;
            return i;
        }
    }

    return -1;
}

int free_page(int pfn) {
    // Check page was actually marked used
    if ((free_pages + pfn)->in_use) {
        (free_pages + pfn)->in_use = 0;
        --allocated_pages;
    }
    return 0;
}


void KernelStart(ExceptionInfo *info, unsigned int pmem_size,
    void *orig_brk, char **cmd_args) {
    int i;

    tot_pmem_size = pmem_size;
    tot_pmem_pages = pmem_size / PAGESIZE;
    cur_brk = orig_brk;

    // Setup interrupt vector table
    interrupt_table[TRAP_KERNEL] = &trap_kernel_handler;
    interrupt_table[TRAP_CLOCK] = &trap_clock_handler;
    interrupt_table[TRAP_ILLEGAL] = &trap_illegal_handler;
    interrupt_table[TRAP_MEMORY] = &trap_memory_handler;
    interrupt_table[TRAP_MATH] = &trap_math_handler;
    interrupt_table[TRAP_TTY_TRANSMIT] = &trap_tty_transmit_handler;
    interrupt_table[TRAP_TTY_RECEIVE] = &trap_tty_receive_handler;

    // Write address of interrupt vector table to REG_VECTOR_BASE register
    WriteRegister(REG_VECTOR_BASE, (RCS421RegVal) &interrupt_table);

    // Allocate a structure for storing the status of all physical pages.
    free_pages = (struct free_page *)
        malloc(sizeof(struct free_page) * tot_pmem_pages);//cur_brk;

    for (i = VMEM_1_BASE / PAGESIZE; i < (long)cur_brk / PAGESIZE; ++i) {
        (free_pages + i)->in_use = 1;
        ++allocated_pages;
    }

    // Initialize kernel page table
    long end_text = (long)&_etext;
    int text_pages = (end_text - VMEM_1_BASE) / PAGESIZE;
    for (i = 0; i < text_pages; i++) {
        struct pte entry = {
            .pfn = (VMEM_1_BASE + i * PAGESIZE) >> PAGESHIFT,
            .unused = 0b00000,
            .uprot = PROT_NONE,
            .kprot = PROT_READ | PROT_EXEC,
            .valid = 0b1
        };
        kernel_page_table[i] = entry;
    }

    int heap_pages = ((long)cur_brk - (long)&_etext) / PAGESIZE;
    for (i = text_pages; i < text_pages + heap_pages; i++) {
        struct pte entry = {
            .pfn = (VMEM_1_BASE + i * PAGESIZE) >> PAGESHIFT,
            .unused = 0b00000,
            .uprot = PROT_NONE,
            .kprot = PROT_READ | PROT_WRITE,
            .valid = 0b1
        };
        kernel_page_table[i] = entry;
    }
    
    int k_unused_pages = (VMEM_LIMIT - (long)cur_brk) / PAGESIZE;
    for (i = text_pages + heap_pages; i < text_pages + heap_pages + k_unused_pages; i++) {
        struct pte entry = {
            .pfn = (VMEM_1_BASE + i * PAGESIZE) >> PAGESHIFT,
            .unused = 0b00000,
            .uprot = PROT_NONE,
            .kprot = PROT_NONE,
            .valid = 0b0
        };

        kernel_page_table[i] = entry;
    }

    // Initialize Region 0
    for (i = 0; i < KERNEL_STACK_PAGES; ++i) {
        struct pte entry = {
            .pfn = (KERNEL_STACK_BASE >> PAGESHIFT) + i,
            .unused = 0b00000,
            .uprot = PROT_NONE,
            .kprot = PROT_READ | PROT_WRITE,
            .valid = 0b1
        };

        (free_pages + KERNEL_STACK_BASE / PAGESIZE + i)->in_use = 1;
        ++allocated_pages;

        idle_page_table[KERNEL_STACK_BASE / PAGESIZE + i] = entry;
    }

    // Initialize user stack
    struct pte entry = {
        .pfn = alloc_page(),
        .unused = 0,
        .uprot = PROT_READ | PROT_WRITE,
        .kprot = PROT_READ | PROT_WRITE,
        .valid = 1
    };

    idle_page_table[USER_STACK_LIMIT / PAGESIZE].valid = 0;
    idle_page_table[USER_STACK_LIMIT / PAGESIZE - 1] = entry;

    for (i = 0; i < PAGE_TABLE_LEN - KERNEL_STACK_PAGES - 2; i++)
        idle_page_table[i].valid = 0;

    WriteRegister(REG_PTR0, (RCS421RegVal) &idle_page_table);
    WriteRegister(REG_PTR1, (RCS421RegVal) &kernel_page_table);

    // TODO: structure associating Pid's with page tables
    //  -hash table with Pid as key

    // Setup idle process
    struct process_info *idle = malloc(sizeof(struct process_info));
    *idle = (struct process_info){
        .pid = 0,
        .user_pages = 1,
        .page_table = idle_page_table,
        .pc = (void *)&idle_process,
        .sp = (void *)USER_STACK_LIMIT,
        .ctx = NULL,
        .next_process = NULL
    };

    processes = idle;


    // enable virtual memory
    WriteRegister(REG_VM_ENABLE, 1);
    vmem_enabled = 1;

    active_process = idle;

    // Set the PC and SP of active process
    info->pc = active_process->pc;
    info->sp = active_process->sp;

    process_queue = NULL;


}


int SetKernelBrk(void *addr) {
    TracePrintf(1, "SetKernelBrk\n");

    if (vmem_enabled == 0) {
        cur_brk = addr;
    } else {
        // TODO: allocate more pages of vmem
    }

    return 0;
}




void idle_process(void) {
    while (1)
        Pause();
}



