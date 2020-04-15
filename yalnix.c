

#include "kernel.h"


void (*interrupt_table[TRAP_VECTOR_SIZE])(ExceptionInfo *) = {NULL};



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

    for (i = 0; i < NUM_TERMINALS;i++) {
        terminals[i] = (struct terminal_info *)malloc(sizeof(struct terminal_info));
        *terminals[i] = (struct terminal_info) {
            .r_head = NULL,
            .r_tail = NULL,
            .w_head = NULL,
            .w_tail = NULL,
            .next_line = NULL,
            .last_line = NULL
        };
    }
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

    if (active_process->pid == 1) {
        TracePrintf(0, "init exiting kernelstart\n");
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


