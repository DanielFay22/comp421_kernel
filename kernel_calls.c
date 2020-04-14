
#include <stddef.h>
#include <string.h>
#include "kernel.h"


SavedContext *ForkContextSwitchHelper(SavedContext *ctxp,
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
        TracePrintf(0, "Calling memcpy, %x, %x\n", (unsigned int)temp,
                    (unsigned int)(VMEM_0_BASE + i * PAGESIZE));
        memcpy(temp, (const void *)(VMEM_0_BASE + i * PAGESIZE), PAGESIZE);
    }

    // Return the current context so that the child process
    // has a copy of the current context
    return ctxp;
}


int Fork(void) {
    int i, j;
    int pid = active_process->pid;
    int error = 0;

    // Check if there is sufficient physical memory to fork
    if (active_process->user_pages + KERNEL_STACK_PAGES >
    tot_pmem_pages - allocated_pages)
        return ERROR;

    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

    // Create new page table
    struct pte *new_page_table = get_new_page_table();

    TracePrintf(0, "FORK: New page table: %x, active page table: %x\n",
        (unsigned int)new_page_table, (unsigned int)active_process->page_table);

    // Ensure virtual memory mappings are correct
    kernel_page_table[PAGE_TABLE_LEN - 1].pfn =
        (unsigned int)(active_process->page_table) >> PAGESHIFT;
    kernel_page_table[PAGE_TABLE_LEN - 2].pfn =
        (unsigned int)(new_page_table) >> PAGESHIFT;

    // Initialize pointers to the virtual addresses of the tables
    struct pte *cur_table_base = (struct pte *)(VMEM_1_LIMIT - PAGESIZE);
    struct pte *new_table_base = (struct pte *)(VMEM_1_LIMIT - 2 * PAGESIZE);
    void *temp = VMEM_1_LIMIT - 3 * PAGESIZE;

    // Copy active page table to new page table
    memcpy((void *)new_table_base, (void *)cur_table_base,
        PAGE_TABLE_SIZE);

    // For every valid page, allocate a physical page and update the pfn
    for (i = 0; i < PAGE_TABLE_LEN; ++i) {
        if ((new_table_base + i)->valid == 1) {
            if (((new_table_base + i)->pfn = alloc_page()) == ERROR) {
                error = 1;
                break;
            }

            TracePrintf(0, "FORK: Assigned physical page %d to virtual page %d\n",
                        (new_table_base + i)->pfn, i);
        }
    }


    // Encountered error
    if (error) {
        // Free any physical pages which were allocated
        for (j = 0; j < i; ++j) {
            if ((new_table_base + j)->valid == 1)
                free_page((new_table_base + i)->pfn);
        }

        free_page_table(new_page_table);

        return ERROR;
    }

    // Copy the old memory to the new memory, excluding kernel stack
    for (i = 0; i < PAGE_TABLE_LEN - KERNEL_STACK_PAGES; ++i) {

        if ((new_table_base + i)->valid == 1) {
            TracePrintf(0, "FORK: Copying page %d, new_table_base = %x, pfn = %d, src_addr = %x, valid = %d\n",
                i, (unsigned int)new_table_base, (new_table_base + i)->pfn,
                        VMEM_0_BASE + i * PAGESIZE, (cur_table_base + i)->valid);

            // Point temp to the new physical page
            kernel_page_table[PAGE_TABLE_LEN - 3].pfn =
                (new_table_base + i)->pfn;

            // Clear temp from TLB
            WriteRegister(REG_TLB_FLUSH, (RCS421RegVal)temp);

            // Copy page from region 0 into temp (new physical page)
            TracePrintf(0, "FORK: Calling memcpy, %x, %x\n", (unsigned int)temp,
                        (unsigned int)(VMEM_0_BASE + i * PAGESIZE));
            memcpy(temp, (const void *)(VMEM_0_BASE + i * PAGESIZE), PAGESIZE);
        }
    }

    
    TracePrintf(0, "Setting up PCB\n");
    // Create new pcb and add it to the process queue
    struct process_info *pcb = (struct process_info *)
        malloc(sizeof(struct process_info));

    *pcb = (struct process_info) {
        .pid = next_pid++,
        .page_table = (void *)new_page_table,
        .user_pages = active_process->user_pages,
        .user_brk = active_process->user_brk,
        .parent = pid
    };

    TracePrintf(0, "Adding PCB to queue\n");
    push_process(&process_queue, &pq_tail, pcb);

    // Use context switch to get context for child process
    ContextSwitch(ForkContextSwitchHelper, &(pcb->ctx),
        (void *)pcb, NULL);


    TracePrintf(0, "Returning\n");
    // Return 0 if in child process, new pid otherwise
    if (active_process->pid == pid)
        return pcb->pid;

    return 0;
}


int Exec(char *filename, char **argvec) {
    return 0;
}


void Exit(int status) {
    TracePrintf(0, "Exit from process %d\n", active_process->pid);
    Halt();
}

int Wait(int *status_ptr) {
    return 0;
}

int GetPid(void) {
    return active_process->pid;
}

int Brk(void *addr) {
    int i;
    long new_brk = (long)UP_TO_PAGE(addr);

//    TracePrintf(0, "Brk called on active process %d, user_brk = %x, addr = %x\n",
//        active_process->pid, (unsigned int)active_process->user_brk,
//                (unsigned int)addr);

    // Ensure requested address is valid
    if (new_brk > VMEM_LIMIT - KERNEL_STACK_SIZE - PAGESIZE) {
        TracePrintf(0, "User heap attempting to grow into the User stack/redzone\n");
        return ERROR;
    }

    // Already allocated sufficient virtual memory
    if (new_brk <= active_process->user_brk) {
        TracePrintf(0, "Sufficient memory already allocated\n");
        return 0;
    }

    // Check if there is sufficient physical memory before allocating new pages
    int num_new_pages = (new_brk - (long)active_process->user_brk) >> PAGESHIFT;
    if (num_new_pages > tot_pmem_pages - allocated_pages) {
        TracePrintf(0,
            "Unable to expand user heap, insufficient physical memory\n");
        return ERROR;
    }

    // Expand user heap
    int cur_page = (long)active_process->user_brk >> PAGESHIFT;
    struct pte* page_table = (struct pte*)(VMEM_LIMIT - PAGESIZE);
    for (i = cur_page; i < cur_page + num_new_pages; ++i) {
        if (!page_table[i].valid) {
            *(page_table + i) = (struct pte){
                .valid = 1,
                .kprot = PROT_READ | PROT_WRITE,
                .uprot = PROT_READ | PROT_WRITE,
                .pfn = alloc_page()
            };

            // Increment the number of pages assigned to the process
            ++active_process->user_pages;
        }
    }

    active_process->user_brk = (void *)new_brk;

    TracePrintf(0, "BRK: Expanded user heap to %x\n", (unsigned int)addr);

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
    	TracePrintf(1, "Switching from pid %d to %d\n", active_process->pid, idle->pid);
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

