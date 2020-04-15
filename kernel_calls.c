
#include <string.h>

#include "kernel.h"

/*
 * Implements the Fork() kernel call.
 *
 * Creates a new process with a copy of the current process's
 * memory space. The new process will be a child of the calling
 * process.
 *
 * Returns the pid of the created process to the calling process,
 * and returns 0 to the child process.
 */
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
    ContextSwitch(ContextSwitchForkHelper, &(pcb->ctx), NULL, NULL);


    // Return 0 if in child process, new pid otherwise
    if (active_process->pid == pid)
        return pcb->pid;

    return 0;
}

/*
 * Implements the Exec() kernel call.
 *
 * Loads the program given by the arguments in the ExceptionInfo
 * struct into memory and begins executing it. Replaces the
 * currently executing process with the new process.
 *
 * If LoadProgram receives a recoverable error, a value of ERROR
 * is returned through the ExceptionInfo struct.
 * If LoadProgram experiences an irrecoverable error, the current
 * process exits with status ERROR.
 * On success, the current process begins executing the new code.
 */
void KernelExec(ExceptionInfo *info) {
    int c;

    // Unpack the arguments from the ExceptionInfo struct
    char *fn = info->regs[1];
    char **av = info->regs[2];

    // In case of error, either return or exit with ERROR status
    switch (c = LoadProgram(fn, av, info)) {
    case ERROR: // Continue running
        info->regs[0] = ERROR;
        return;

    case -2:    // non-recoverable error
        KernelExit(ERROR);

    default:    // LoadProgram was successful, don't change info struct
        return;
    }

}

/*
 * Implements the Exit() kernel call.
 *
 * Terminates the current process, freeing all allocated
 * memory.
 *
 * If the calling process has a parent that is not exited,
 * the process pid, parent pid, and exit status will be
 * saved for later recovery with a call to Wait().
 */
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

/*
 * Implements the Wait() kernel call.
 *
 * If the calling process has unreaped children, returns the
 * pid of the earliest child to exit and stores the status of
 * that child in status_ptr.
 *
 * If the calling process has no unreaped children, but does
 * have actively running children, process blocks until a child
 * Exits, at which point the return is as described above.
 *
 * If the calling process has no children, actively running or
 * unreaped, returns ERROR.
 */
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

/*
 * Implements the Brk() kernel call.
 *
 * Moves the location of a user process break to the
 * specified address.
 *
 * Returns 0 on success, ERROR on failure.
 */
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

/*
 * Implements the Delay() kernel call.
 *
 * The calling process will be blocked for the provided number
 * of clock ticks.
 *
 * After the necessary number of ticks have passed, the process
 * will return a value of 0.
 *
 * If a negative number of ticks is provided, returns ERROR.
 */
int KernelDelay(int clock_ticks) {
	TracePrintf(0, "DELAY: pid = %d\n", active_process->pid);

	// Check invalid or trivial input
	if (clock_ticks < 0)
        return ERROR;
    else if (clock_ticks == 0)
        return 0;

    // Block current process
    active_process->delay_ticks = clock_ticks;
    push_process(&waiting_queue, &wq_tail, active_process);

    // Switch to next available process, or idle if no process ready
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

 /*
  * Implements the TtyRead() kernel call.
  */
int KernelTtyRead(int tty_id, void *buf, int len) {
    if (len == 0) {
        return 0;
    }

    //get the correct terminal info
    struct terminal_info *terminal = terminals[tty_id];
    struct available_line *line = terminal->next_line;

    //if there are available lines on this terminal, use one
    if (line != NULL) {
        //if the first line is longer than this call is looking for
        if (len < line->len) {
            memcpy(buf, (void*)line->line, len);
            //update the avaliable line
            line->len = line->len - len;
            line->line += len;
            return len;
        }
        else {
            //move the line queue up
            terminal->next_line = line->next;
            memcpy(buf, line->line, line->len);
            //free the avaliable line
            free(line->orig_ptr);
            len = line->len;
            free(line);
            return len;
        }
    }
    //if there are no available lines, block
    else {
        //indicate how many chacters this process is looking for
        active_process->seeking_len = len;

        push_process(&terminal->r_head, &terminal->r_tail, active_process);

        RemoveSwitch();

        memcpy(buf, active_process->line->line, active_process->line->len);
        //free the line if it was the end
        if (active_process->line->free) {
            free(active_process->line->orig_ptr);
        }
        len = active_process->line->len;
        free(active_process->line);
        return len;
    }
}

/*
 * Implements the TtyWrite() kernel call.
 */
int KernelTtyWrite(int tty_id, void *buf, int len) {
    TracePrintf(0, "TtyWrite\n");
    Halt();
    // if (terminals_w[tty_id] == NULL) {
    //     terminals_w[tty_id]->head = active_process;
    //     terminals_w[tty_id]->tail = active_process;

    //     TtyTransmit(tty_id, buf, len);
    //     RemoveSwitch();
    // }
    // else {
    //     push_process(&terminals_w[tty_id]->head, &terminals_w[tty_id]->tail, terminals_w[tty_id]);

    //     RemoveSwitch();
    // }
    // return len;
}

