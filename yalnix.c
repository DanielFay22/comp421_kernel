
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
void init_process(void);


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

    struct process_info* temp;
    struct process_info* head = waiting_queue;
    while (waiting_queue != NULL) {
        TracePrintf(1, "decrementing waiting_queue process %d\n", waiting_queue->pid);
        waiting_queue->delay_ticks--;
        TracePrintf(1, "new value %d\n", waiting_queue->delay_ticks);
        if (waiting_queue->delay_ticks < 1) {
            TracePrintf(0, "process %d unblocking\n", waiting_queue->pid);
            if (waiting_queue->prev_process != NULL || waiting_queue->next_process != NULL){
                //middle
                if (waiting_queue->prev_process != NULL && waiting_queue->next_process != NULL) {
                    (waiting_queue->prev_process)->next_process = waiting_queue->next_process;
                    (waiting_queue->next_process)->prev_process = waiting_queue->prev_process;
                }
                //head
                else if (waiting_queue->prev_process == NULL) {
                    (waiting_queue->next_process)->prev_process = NULL; 
                    head = waiting_queue->next_process;
                }
                //tail
                else {
                    (waiting_queue->prev_process)->next_process = NULL;
                }
                temp = waiting_queue;
                waiting_queue = waiting_queue->next_process;

            }
            else {
                TracePrintf(0, "set waiting_queue to null\n");
                temp = waiting_queue;
                waiting_queue = NULL;
                head = NULL;
            }
            if (process_queue == NULL) {
                TracePrintf(0, "point to new pq\n");
                process_queue = temp;
                pq_tail = temp;
            }
            else {
                TracePrintf(0, "adding to pq\n");
                pq_tail->next_process = temp;
                TracePrintf(0, "setting prev to new entry\n");
                temp->prev_process = pq_tail;
                TracePrintf(0, "setting next to null\n");
                temp->next_process = NULL;
                TracePrintf(0, "pointing tail to new\n");
                pq_tail = temp;
            }
        }
        else {
            waiting_queue = waiting_queue->next_process;
        }
    }
    waiting_queue = head;
    // If any processes are available, switch to them.
    TracePrintf(0, "pq before switch %p\n", (void*)process_queue);
    if (last_switch - (++clock_count) <= -2 && process_queue != NULL) {
        struct process_info *next = process_queue;
        TracePrintf(1, "attempting switch from pid %d to pid %d\n"), GetPid(), next->pid;
        process_queue = process_queue->next_process;
        TracePrintf(0, "pq incremented %p\n", (void*)process_queue);
        last_switch = clock_count;

        ContextSwitch(ContextSwitchFunc, &(active_process->ctx),
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

    // The pids are getting screwed up for some reason
    // If the running process is not the idle process, put it back on the queue.
    // if (curProc->pid != 0) {
    //     if (process_queue == NULL)
    //         process_queue = curProc;
    //     else
    //         process_queue->next_process = curProc;
    //}

    active_process = newProc;

    // Set region 0 page table to new process table
    TracePrintf(0, "writing new region 0 PT addr %p", (void *)((newProc->page_table) << PAGESHIFT));
    WriteRegister(REG_PTR0, (RCS421RegVal) ((newProc->page_table) << PAGESHIFT));

    // Flush region 0 entries from the TLB
    WriteRegister(REG_TLB_FLUSH, (RCS421RegVal) TLB_FLUSH_0);

    return &newProc->ctx;
}

SavedContext *ContextSwitchOne(SavedContext *ctxp,
    void *p1, void *p2) {
    int i;
    //redefine the init PT pointer
    struct pte *init_page_table = (struct pte *) (VMEM_LIMIT - PAGESIZE * 2);
    //copy the Kernel Stack
    for (i = 0; i < KERNEL_STACK_PAGES; ++i) {
        TracePrintf(1, "copying to %p from %p\n",(void*)(VMEM_LIMIT - 3 * PAGESIZE),(void*)(KERNEL_STACK_BASE + i * PAGESIZE));
        struct pte  init_stack_entry = {
            .pfn = alloc_page(),
            .unused = 0b00000,
            .uprot = PROT_NONE,
            .kprot = PROT_READ | PROT_WRITE,
            .valid = 0b1
        };
        kernel_page_table[PAGE_TABLE_LEN - 3] = init_stack_entry;
        init_page_table[PAGE_TABLE_LEN - 4 + i] = init_stack_entry;
        WriteRegister(REG_TLB_FLUSH, (RCS421RegVal) (VMEM_LIMIT - PAGESIZE * 3));
        memcpy((void*)(VMEM_LIMIT - 3 * PAGESIZE), (void*)(KERNEL_STACK_BASE + i * PAGESIZE), PAGESIZE);
    }

    //Switch the idle region 0 Page Table for the Init Page Table
    kernel_page_table[PAGE_TABLE_LEN - 1].pfn = (VMEM_LIMIT - PAGESIZE * 2) >> PAGESHIFT;
    WriteRegister(REG_TLB_FLUSH, VMEM_LIMIT - PAGESIZE); 

    //write the new Region 0 Page Table pointer to hardware and flush TLB
    WriteRegister(REG_PTR0, (RCS421RegVal) init_page_table);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

    active_process = (struct pte *) p1;

    return ctxp;
}

struct pte *get_new_page_table() {
    int i;
    //TODO don't know if this is necessary
    
    struct pte *table = (struct pte *)malloc(sizeof(struct pte) * PAGE_TABLE_LEN);
    
    //struct pte *table = (struct pte *)malloc(sizeof(struct pte));

    for (i = 0; i < MEM_INVALID_PAGES; ++i)
        (table + i)->valid = 0;

    for (i = 0; i < KERNEL_STACK_PAGES; ++i) {
        struct pte entry = {
            .pfn = alloc_page(),
            .unused = 0b00000,
            .uprot = PROT_NONE,
            .kprot = PROT_READ | PROT_WRITE,
            .valid = 0b1
        };

        *(table + KERNEL_STACK_BASE / PAGESIZE + i) = entry;
    }

    // Initialize user stack
    struct pte entry = {
        .pfn = alloc_page(),
        .unused = 0,
        .uprot = PROT_READ | PROT_WRITE,
        .kprot = PROT_READ | PROT_WRITE,
        .valid = 1
    };


    (table + USER_STACK_LIMIT / PAGESIZE)->valid = 0;
    *(table + USER_STACK_LIMIT / PAGESIZE - 1) = entry;

    // TODO: allocate and initializing a new page table
    return table;
}


unsigned int alloc_page(void) {
    unsigned int i;

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
    //test with ./yalnix -lk 0 -lu 0 -n -s init1
    TracePrintf(0, "hello\n");
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

    // Initial Region 0 (idle) Page Table, always at the top of VMEM
    struct pte *idle_page_table = (struct pte *)(VMEM_LIMIT - PAGESIZE); 
    struct pte *init_page_table = (struct pte *)(VMEM_LIMIT - 2 * PAGESIZE);

    // Mark Physical Pages used by heap as used
    for (i = VMEM_1_BASE / PAGESIZE; i < (long)cur_brk / PAGESIZE; ++i) {
        (free_pages + i)->in_use = 1;
        ++allocated_pages;
    }

    // Mark physical page used by idle_page_table
    (free_pages + VMEM_LIMIT / PAGESIZE -  1)->in_use = 1;
    ++allocated_pages;

    //Mark physical page used by init page table
    (free_pages + VMEM_LIMIT / PAGESIZE - 2)->in_use = 1;
    ++allocated_pages;

    TracePrintf(0, "writing kernel ptes\n");
    // Initialize kernel page table
    TracePrintf(0, "writing kernel text ptes\n");
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

    TracePrintf(0, "writing kernel heap ptes\n");
    // Initialize kernel heap
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
    
    TracePrintf(0, "writing unused ptes\n");
    int k_unused_pages = (VMEM_LIMIT - (long)cur_brk) / PAGESIZE;
    for (i = text_pages + heap_pages; i < text_pages + heap_pages + k_unused_pages - 1; i++) {
        struct pte entry = {
            .pfn = (VMEM_1_BASE + i * PAGESIZE) >> PAGESHIFT,
            .unused = 0b00000,
            .uprot = PROT_NONE,
            .kprot = PROT_NONE,
            .valid = 0b0
        };

        kernel_page_table[i] = entry;
    }

    //special entry for region 0 page table
    
    struct pte idle_PT_entry = {
        .pfn = (VMEM_LIMIT - PAGESIZE) >> PAGESHIFT,
        .unused = 0b00000,
        .uprot = PROT_NONE,
        .kprot = PROT_READ | PROT_WRITE,
        .valid = 0b1
    };
    kernel_page_table[PAGE_TABLE_LEN - 1] = idle_PT_entry;
    
    //special entry for init region 0 
    struct pte init_PT_entry = {
        .pfn = (VMEM_LIMIT - 2 * PAGESIZE) >> PAGESHIFT,
        .unused = 0b00000,
        .uprot = PROT_NONE,
        .kprot = PROT_READ | PROT_WRITE,
        .valid = 0b1
    };
    kernel_page_table[PAGE_TABLE_LEN - 2] = init_PT_entry;


    // Initialize Region 0
    TracePrintf(0, "writing kernel stack ptes\n");
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

    for (i = 0; i < PAGE_TABLE_LEN - KERNEL_STACK_PAGES - 1; i++) {
        idle_page_table[i].valid = 0;
        init_page_table[i].valid = 0;
    }

    TracePrintf(0, "pointer registers to initial r0 and r1 PT\n");
    WriteRegister(REG_PTR0, (RCS421RegVal) idle_page_table);
    WriteRegister(REG_PTR1, (RCS421RegVal) &kernel_page_table);

    // TODO: structure associating Pid's with page tables
    //  -hash table with Pid as key

    // Setup idle process
    TracePrintf(0, "assign idle PCB\n");
    idle = malloc(sizeof(struct process_info));
    TracePrintf(0, "%p\n", (void*)((long)idle_page_table >> PAGESHIFT));
    *idle = (struct process_info){
        .pid = 0,
        .user_pages = 0,
        .page_table = ((unsigned int)idle_page_table) >> PAGESHIFT,
        .pc = NULL,
        .sp = NULL,
        .ctx = NULL,
        .next_process = NULL
    };

    // enable virtual memory
    TracePrintf(0, "enabling vmem\n");
    WriteRegister(REG_VM_ENABLE, 1);
    vmem_enabled = 1;

    active_process = idle;
    LoadProgram("idle", NULL, info);

    // Setup init process
    struct process_info *init = malloc(sizeof(struct process_info));
    *init = (struct process_info) {
        .pid = 1,
        .user_pages = 0,
        .page_table = VMEM_LIMIT / PAGESIZE - 2,
        .delay_ticks = 0
    };
    
    ContextSwitch(ContextSwitchOne, (SavedContext *)&idle->ctx, init, NULL);
    //WriteRegister(REG_PTR0, (RCS421RegVal) (active_process->page_table << PAGESHIFT) );

    if (GetPid()) {
        TracePrintf(0, "init exiting kernelstart\n");
        active_process = init;
        LoadProgram(cmd_args[0], &cmd_args[1], info);
    }
    else {
        TracePrintf(0, "idle exiting kernelstart\n");
    }
    process_queue = NULL;
}


int SetKernelBrk(void *addr) {
    TracePrintf(1, "SetKernelBrk\n");

    if (vmem_enabled == 0) {
        cur_brk = addr;
    } else {
        // TODO: test vmem allocation, malloc doesn't seem to call this after vmem enabled

        //if requested memory is still within last allocated page
        if ((long)UP_TO_PAGE(cur_brk) > (long)addr) {
            cur_brk = addr;
        }
        //else allocate more physical pages
        else {
            cur_brk = (void*)UP_TO_PAGE(cur_brk);
            while((long)addr > (long)UP_TO_PAGE(cur_brk)) {
                struct pte entry = {
                    .pfn = alloc_page(),
                    .unused = 0b00000,
                    .uprot = PROT_NONE,
                    .kprot = PROT_READ | PROT_WRITE,
                    .valid = 0b1
                };
                kernel_page_table[(long)cur_brk / PAGESIZE] = entry;
                cur_brk = cur_brk + PAGESIZE;
            }
        }
    }

    return 0;
}


