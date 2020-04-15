
#include <string.h>

#include "kernel.h"


// Draft of context switch function
SavedContext *ContextSwitchFunc(SavedContext *ctxp,
    void *p1, void *p2) {

    struct process_info *curProc = (struct process_info *)p1;
    struct process_info *newProc = (struct process_info *)p2;

    curProc->ctx = *ctxp;

    active_process = newProc;

    // Set region 0 page table to new process table
    TracePrintf(1, "CONTEXT SWITCH: Writing new region 0 PT addr %p\n", newProc->page_table);
    WriteRegister(REG_PTR0, (RCS421RegVal) newProc->page_table);
    kernel_page_table[PAGE_TABLE_LEN - 1].pfn =
        DOWN_TO_PAGE(newProc->page_table) >> PAGESHIFT;

    // Flush region 0 entries from the TLB and page table entry from region 1
    WriteRegister(REG_TLB_FLUSH, (RCS421RegVal) TLB_FLUSH_0);
    WriteRegister(REG_TLB_FLUSH, (RCS421RegVal) (VMEM_LIMIT - PAGESIZE));

    return &newProc->ctx;
}

SavedContext *ContextSwitchForkHelper(SavedContext *ctxp,
    void *p1, void *p2) {
    int i;

    struct pte *new_table_base = (struct pte *)(VMEM_1_LIMIT - 2 * PAGESIZE);
    struct process_info *curProc = (struct process_info *)p1;
    curProc->ctx = *ctxp;


    // Copy kernel stack
    void *temp = VMEM_1_LIMIT - 3 * PAGESIZE;
    for (i = PAGE_TABLE_LEN - KERNEL_STACK_PAGES; i < PAGE_TABLE_LEN; ++i) {
        // Point temp at the assigned physical frame
        kernel_page_table[PAGE_TABLE_LEN - 3].pfn =
            (new_table_base + i)->pfn;

        // Flush TLB entry for temp
        WriteRegister(REG_TLB_FLUSH, (RCS421RegVal)temp);

        // Copy kernel stack contents to new physical frame
        TracePrintf(1, "FORK: Calling memcpy, %x, %x\n", (unsigned int)temp,
                    (unsigned int)(VMEM_0_BASE + i * PAGESIZE));
        memcpy(temp, (const void *)(VMEM_0_BASE + i * PAGESIZE), PAGESIZE);
    }

    // Return the current context so that the child process
    // has a copy of the current context
    return ctxp;
}

SavedContext *ContextSwitchInitHelper(SavedContext *ctxp,
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
        init_page_table[PAGE_TABLE_LEN - KERNEL_STACK_PAGES + i] = init_stack_entry;
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


SavedContext *ContextSwitchExitHelper(SavedContext *ctxp,
    void *p1, void *p2) {
    int i;

    struct process_info *curProc = (struct process_info *)p1;
    struct process_info *newProc = (struct process_info *)p2;

    // Free the kernel stack of the old process
    for (i = PAGE_TABLE_LEN - KERNEL_STACK_PAGES; i < PAGE_TABLE_LEN; ++i)
        free_page((CURRENT_PAGE_TABLE + i)->pfn);

    TracePrintf(1, "Freeing page table of process %d\n", curProc->pid);
    // Free the process page table
    free_page_table(active_process->page_table);

    // Free the process pcb
    free(p1);

    // Setup the new active process
    active_process = newProc;

    // Set region 0 page table to new process table
    TracePrintf(1, "writing new region 0 PT addr %p\n", newProc->page_table);
    WriteRegister(REG_PTR0, (RCS421RegVal) newProc->page_table);
    kernel_page_table[PAGE_TABLE_LEN - 1].pfn =
        DOWN_TO_PAGE(newProc->page_table) >> PAGESHIFT;

    // Flush region 0 entries from the TLB and page table entry from region 1
    WriteRegister(REG_TLB_FLUSH, (RCS421RegVal) TLB_FLUSH_0);
    WriteRegister(REG_TLB_FLUSH, (RCS421RegVal) (VMEM_LIMIT - PAGESIZE));

    // Reset the switch counter
    last_switch = clock_count;

    return &newProc->ctx;

}
