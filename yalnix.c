
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


struct available_page_table {
    void *page_table;
    struct available_page_table *next;
};

struct available_page_table *page_tables = NULL;


void (*interrupt_table[TRAP_VECTOR_SIZE])(ExceptionInfo *) = {NULL};


// Interrupt handlers
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

struct pte *get_new_page_table() {
    void *page_table;

    // Check if there is an available allocated page table
    if (page_tables != NULL) {
        struct available_page_table *pt = page_tables;
        page_tables = page_tables->next;


        page_table = pt->page_table;
        free(pt);


    } else {
        // Allocate a new page
        unsigned int pfn = alloc_page();
        struct available_page_table *pt_ptr = (struct available_page_table *)
            malloc(sizeof(struct available_page_table));

        // Add the extra half page to the free tables list
        pt_ptr->page_table = (void *)(pfn << PAGESHIFT + PAGESIZE / 2);
        pt_ptr->next = page_tables;
        page_tables = pt_ptr;

        page_table = (void *)(pfn << PAGESHIFT);
    }

    return (struct pte *)page_table;
//
//
//
//
//
//
//
//
//
//    int i;
//    //TODO don't know if this is necessary
//
//    struct pte *table = (struct pte *)malloc(sizeof(struct pte) * PAGE_TABLE_LEN);
//
//    //struct pte *table = (struct pte *)malloc(sizeof(struct pte));
//
//    for (i = 0; i < MEM_INVALID_PAGES; ++i)
//        (table + i)->valid = 0;
//
//    for (i = 0; i < KERNEL_STACK_PAGES; ++i) {
//        struct pte entry = {
//            .pfn = alloc_page(),
//            .unused = 0b00000,
//            .uprot = PROT_NONE,
//            .kprot = PROT_READ | PROT_WRITE,
//            .valid = 0b1
//        };
//
//        *(table + KERNEL_STACK_BASE / PAGESIZE + i) = entry;
//    }
//
//    // Initialize user stack
//    struct pte entry = {
//        .pfn = alloc_page(),
//        .unused = 0,
//        .uprot = PROT_READ | PROT_WRITE,
//        .kprot = PROT_READ | PROT_WRITE,
//        .valid = 1
//    };
//
//
//    (table + USER_STACK_LIMIT / PAGESIZE)->valid = 0;
//    *(table + USER_STACK_LIMIT / PAGESIZE - 1) = entry;
//
//    // TODO: allocate and initializing a new page table
//    return table;
}

void free_page_table(struct pte *pt) {
    unsigned int pfn = ((unsigned int)pt) >> PAGESHIFT;

    struct available_page_table *head = page_tables;
    struct available_page_table *prev = NULL;
    while (head != NULL) {
        if (((unsigned int)head->page_table) >> PAGESHIFT == pfn)
            break;
        prev = head;
        head = head->next;
    }

    // If both halves of the page frame are not in use, free the page
    if (head != NULL) {
        if (prev == NULL)
            page_tables = head->next;
        else
            prev->next = head->next;

        free(head);
        free_page(pfn);
    } else {
        // Otherwise add an entry to the list of free pagetables
        struct available_page_table *apt = (struct available_page_table *)
            malloc(sizeof(struct available_page_table));
        *apt = (struct available_page_table) {
            .page_table = (void *)pt,
            .next = page_tables
        };

        page_tables = apt;
    }

}


unsigned int alloc_page(void) {
    unsigned int i;

    // No physical memory available
    if (allocated_pages == tot_pmem_pages)
        return ERROR;

    // Find next available page
    for (i = 0; i < tot_pmem_pages; ++i) {
        if ((free_pages + i)->in_use == 0) {
            (free_pages + i)->in_use = 1;
            ++allocated_pages;
            return i;
        }
    }

    return ERROR;
}

int free_page(int pfn) {
    // Check page was actually marked used
    if ((free_pages + pfn)->in_use) {
        (free_pages + pfn)->in_use = 0;
        --allocated_pages;
    }
    return 0;
}


// Add a new pcb to the given queue
void push_process(struct process_info **head, struct process_info **tail,
    struct process_info *new_pcb) {

    TracePrintf(1, "PUSH PROCESS: Pushing process %d onto PQ\n", new_pcb->pid);

    if (*head == NULL) {
        *head = new_pcb;
        *tail = new_pcb;

        new_pcb->next_process = NULL;
        new_pcb->prev_process = NULL;
    } else {
        (*tail)->next_process = new_pcb;

        new_pcb->next_process = NULL;
        new_pcb->prev_process = *tail;

        *tail = new_pcb;
    }
}

// Pop the next process off the given queue
struct process_info *pop_process(
    struct process_info **head, struct process_info **tail) {

    struct process_info *new_head = *head;

    if (new_head != NULL) {
        *head = new_head->next_process;
        new_head->next_process = NULL;
    }

    if (*head != NULL)
        (*head)->prev_process = NULL;

    return new_head;
}

// Remove a pcb from whatever queue it is part of
void remove_process(struct process_info **head, struct process_info **tail, 
    struct process_info *pi) {

//    TracePrintf(0, "Remove process: head address = %x, tail address = %x\n",
//                (unsigned int)*head, (unsigned int)*tail);
//    TracePrintf(0, "Remove process: pi = %x\n", (unsigned int)pi);
//    TracePrintf(0, "Remove process: pi.next_process = %x, pi.prev_process = %x\n",
//                (unsigned int)pi->next_process, (unsigned int)pi->prev_process);
    
    // If pi is head or tail, update accordingly.
    if (*head == pi)
        *head = pi->next_process;
    if (*tail == pi)
        *tail = pi->prev_process;
    
    // Remove pi from linked list
    if (pi->prev_process != NULL)
        pi->prev_process->next_process = pi->next_process;
    
    if (pi->next_process != NULL)
        pi->next_process->prev_process = pi->prev_process;
    
    pi->prev_process = NULL;
    pi->next_process = NULL;

    TracePrintf(0, "Removing process %d from queue\n", pi->pid);
    
}

void KernelStart(ExceptionInfo *info, unsigned int pmem_size,
    void *orig_brk, char **cmd_args) {
    //test with ./yalnix -lk 0 -lu 0 -n -s init1
    TracePrintf(0, "hello\n");
    int i;

    process_queue = NULL;

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
        malloc(sizeof(struct free_page) * tot_pmem_pages);

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

    // Setup idle process
    TracePrintf(0, "assign idle PCB\n");
    idle = malloc(sizeof(struct process_info));
    TracePrintf(0, "%p\n", idle_page_table);
    *idle = (struct process_info){
        .pid = next_pid++,
        .user_pages = 0,
        .user_brk = (void *)MEM_INVALID_SIZE,
        .page_table = (void *)idle_page_table,
        .parent = NO_PARENT,
        .next_process = NULL,
        .active_children = 0,
        .exited_children = 0
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
        .pid = next_pid++,
        .user_pages = 0,
        .user_brk = (void *)MEM_INVALID_SIZE,
        .page_table = (void *)(VMEM_LIMIT - 2 * PAGESIZE),
        .delay_ticks = 0,
        .parent = NO_PARENT,
        .active_children = 0,
        .exited_children = 0
    };

    // Get current context for init process
    ContextSwitch(ContextSwitchInitHelper, (SavedContext *)&idle->ctx,
        init, NULL);

    if (GetPid()) {
        TracePrintf(0, "init exiting kernelstart\n");
        active_process = init;
        LoadProgram(cmd_args[0], &cmd_args[0], info);
    }
    else {
        TracePrintf(0, "idle exiting kernelstart\n");
    }
}


int SetKernelBrk(void *addr) {
    TracePrintf(0, "SetKernelBrk - addr = %x\n", (unsigned long)addr);

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

            if (addr >= VMEM_1_LIMIT - NUM_RESERVED_KERNEL_PAGES * PAGESIZE)
                return -1;

            // TODO: handle decreasing heap size
            if (addr < cur_brk)
                return 0;
            else {
                while ((long) addr > (long) UP_TO_PAGE(cur_brk)) {

                    // Allocate a page frame and assign it to the page table
                    long pte_offset = (long)(cur_brk - VMEM_1_BASE) / PAGESIZE;
                    kernel_page_table[pte_offset] =
                        (struct pte){
                        .pfn = alloc_page(),
                        .unused = 0b00000,
                        .uprot = PROT_NONE,
                        .kprot = PROT_READ | PROT_WRITE,
                        .valid = 0b1
                    };

                    // Increment cur_brk by one page
                    cur_brk = cur_brk + PAGESIZE;
                }
            }
        }
    }

    return 0;
}


