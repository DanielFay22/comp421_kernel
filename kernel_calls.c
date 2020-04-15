
#include <stddef.h>
#include <string.h>
#include "kernel.h"


int KernelFork(void) {
    int i, j;
    int pid = active_process->pid;
    int error = 0;

    TracePrintf(0, "FORK: pid = %d\n", active_process->pid);

    // Check if there is sufficient physical memory to fork
    if (active_process->user_pages + KERNEL_STACK_PAGES >
    tot_pmem_pages - allocated_pages)
        return ERROR;

    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

    // Create new page table
    struct pte *new_page_table = get_new_page_table();

    TracePrintf(1, "FORK: New page table: %x, active page table: %x\n",
        (unsigned int)new_page_table, (unsigned int)active_process->page_table);

    // Ensure virtual memory mappings are correct
    kernel_page_table[PAGE_TABLE_LEN - 1].pfn =
        (unsigned int)(active_process->page_table) >> PAGESHIFT;
    kernel_page_table[PAGE_TABLE_LEN - 2].pfn =
        (unsigned int)(new_page_table) >> PAGESHIFT;

    // Initialize pointers to the virtual addresses of the tables
    struct pte *cur_table_base = (struct pte *)(VMEM_1_LIMIT - PAGESIZE);
    struct pte *new_table_base = (struct pte *)(VMEM_1_LIMIT - 2 * PAGESIZE);
    void *temp = (void *)(VMEM_1_LIMIT - 3 * PAGESIZE);

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

            TracePrintf(1, "FORK: Assigned physical page %d to virtual page %d\n",
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
            TracePrintf(1, "FORK: Copying page %d, new_table_base = %x, pfn = %d, src_addr = %x, valid = %d\n",
                i, (unsigned int)new_table_base, (new_table_base + i)->pfn,
                        VMEM_0_BASE + i * PAGESIZE, (new_table_base + i)->valid);

            // Point temp to the new physical page
            kernel_page_table[PAGE_TABLE_LEN - 3].pfn =
                (new_table_base + i)->pfn;

            // Clear temp from TLB
            WriteRegister(REG_TLB_FLUSH, (RCS421RegVal)temp);

            // Copy page from region 0 into temp (new physical page)
            TracePrintf(1, "FORK: Calling memcpy, %x, %x\n", (unsigned int)temp,
                        (unsigned int)(VMEM_0_BASE + i * PAGESIZE));
            memcpy(temp, (const void *)(VMEM_0_BASE + i * PAGESIZE), PAGESIZE);
        }
    }

    ++(active_process->active_children);
    
    TracePrintf(1, "FORK: Setting up PCB\n");
    // Create new pcb and add it to the process queue
    struct process_info *pcb = (struct process_info *)
        malloc(sizeof(struct process_info));

    *pcb = (struct process_info) {
        .pid = next_pid++,
        .page_table = (void *)new_page_table,
        .user_pages = active_process->user_pages,
        .user_brk = active_process->user_brk,
        .parent = pid,
        .active_children = 0,
        .exited_children = 0
    };

    TracePrintf(1, "FORK: Adding PCB to queue\n");
    push_process(&process_queue, &pq_tail, pcb);

    // Use context switch to get context for child process
    ContextSwitch(ContextSwitchForkHelper, &(pcb->ctx),
        (void *)pcb, NULL);


    // Return 0 if in child process, new pid otherwise
    if (active_process->pid == pid)
        return pcb->pid;

    return 0;
}

void KernelExec(ExceptionInfo *info) {
    int c;

    char *fn = info->regs[1];
    char **av = info->regs[2];

    switch (c = LoadProgram(fn, av, info)) {
    case ERROR:
        info->regs[0] = ERROR;
        return;

    case -2:    // non-recoverable error
        KernelExit(ERROR);

    default:
        return;
    }


}

void KernelExit(int status) {
    int i;
    struct process_info *parent = NULL;

    TracePrintf(0, "EXIT: pid = %d\n", active_process->pid);

    TracePrintf(1, "EXIT: Finding parent %d of process %d\n",
        active_process->parent, active_process->pid);

    // Find the parent process if it is still active.
    if (active_process->parent != NO_PARENT) {
        struct process_info *head = process_queue;

        while (head != NULL) {
            if (head->pid == active_process->parent) {
                parent = head;
                break;
            }
            head = head->next_process;
        }

        if (parent == NULL)
            head = waiting_queue;
        else
            head = NULL;

        while (head != NULL) {
            if (head->pid == active_process->parent) {
                parent = head;
                break;
            }
            head = head->next_process;
        }

    }


    TracePrintf(1, "EXIT: Freeing physical memory of process %d\n",
        active_process->pid);
    // Free the physical memory used by the user
    for (i = 0; i < PAGE_TABLE_LEN - KERNEL_STACK_PAGES; ++i) {
        if ((CURRENT_PAGE_TABLE + i)->valid)
            free_page((CURRENT_PAGE_TABLE + i)->pfn);
    }

    // No active parent, process is an orphan
    if (parent != NULL) {
        TracePrintf(1, "EXIT: Found parent with pid = %d\n", active_process->parent);
        // Save the pid and status
        struct exit_status *es = (struct exit_status *)
            malloc(sizeof(struct exit_status));

        *es = (struct exit_status) {
            .pid = active_process->pid,
            .parent = active_process->parent,
            .status = status,
            .prev = eq_tail,
            .next = NULL
        };

        if (eq_tail == NULL) {
            exit_queue = es;
            eq_tail = exit_queue;
        } else {
            eq_tail->next = es;
            eq_tail = es;
        }

        --(parent->active_children);
        ++(parent->exited_children);
    }


    // Get rid of the current process, and switch to a new one
    struct process_info *next = pop_process(&process_queue, &pq_tail);
    if (next == NULL) {
        TracePrintf(1, "EXIT: Switching to idle process\n");
        next = idle;
    }

    TracePrintf(1, "EXIT: Context switching to process %d\n", next->pid);

    ContextSwitch(ContextSwitchExitHelper, &(active_process->ctx),
        active_process, next);

}

int KernelWait(int *status_ptr) {
    TracePrintf(0, "WAIT: pid = %d\n", active_process->pid);

    if (active_process->active_children == 0
    && active_process->exited_children == 0) {
        TracePrintf(1, "WAIT: Error\n");
        return ERROR;
    }


    while (1) {
        if (active_process->exited_children > 0) {
            TracePrintf(1, "WAIT: Searching for exit status, ec = %x\n",
                active_process->exited_children);

            // Find the first exit status struct for a child of this process
            struct exit_status *es = exit_queue;
            while (es != NULL) {
                if (es->parent == active_process->pid)
                    break;
                es = es->next;
            }

            if (es != NULL) {

                --(active_process->exited_children);

                // Save the exit status
                *status_ptr = es->status;

                // Remove es from queue
                if (es->prev != NULL)
                    es->prev->next = es->next;
                if (es->next != NULL)
                    es->next->prev = es->prev;

                // Free the status struct and return the pid
                int pid = es->pid;
                free(es);
                return pid;
            }
        } else {    // If no children have exited, block
            TracePrintf(1, "WAIT: No children exited, blocking\n");

            // Get the next available process. If none is available,
            // switch to idle
            struct process_info *next = pop_process(&process_queue, &pq_tail);
            if (next == NULL)
                next = idle;

            // Return the current process to the queue
            push_process(&process_queue, &pq_tail, active_process);

            // Switch to the new process
            ContextSwitch(ContextSwitchFunc, &(active_process->ctx),
                (void *)active_process, (void *)next);
        }
    }
}

int KernelGetPid(void) {
    return active_process->pid;
}

int KernelBrk(void *addr) {
    int i;
    long new_brk = (long)UP_TO_PAGE(addr);

    TracePrintf(0, "BRK: pid = %d\n", active_process->pid);

    // Ensure requested address is valid
    if (new_brk > VMEM_LIMIT - KERNEL_STACK_SIZE - PAGESIZE) {
        TracePrintf(1, "User heap attempting to grow into the User stack/redzone\n");
        return ERROR;
    }

    // Already allocated sufficient virtual memory
    if (new_brk <= active_process->user_brk) {
        TracePrintf(1, "Sufficient memory already allocated\n");
        return 0;
    }

    // Check if there is sufficient physical memory before allocating new pages
    int num_new_pages = (new_brk - (long)active_process->user_brk) >> PAGESHIFT;
    if (num_new_pages > tot_pmem_pages - allocated_pages) {
        TracePrintf(1,
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

    TracePrintf(1, "BRK: Expanded user heap to %x\n", (unsigned int)addr);

    return 0;
}

int KernelDelay(int clock_ticks) {
	TracePrintf(0, "DELAY: pid = %d\n", active_process->pid);

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

int KernelTtyRead(int tty_id, void *buf, int len) {
    return 0;
}

int KernelTtyWrite(int tty_id, void *buf, int len) {
    return 0;
}

