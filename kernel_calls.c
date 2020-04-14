
#include <stddef.h>
#include <string.h>
#include "kernel.h"


SavedContext *ForkContextSwitchHelper(SavedContext *ctxp,
    void *p1, void *p2) {

    struct process_info *curProc = (struct process_info *)p1;
    struct process_info *newProc = (struct process_info *)p2;

    curProc->ctx = *ctxp;

    active_process = newProc;

    // Set region 0 page table to new process table
    TracePrintf(0, "writing new region 0 PT addr %p\n", newProc->page_table);
    WriteRegister(REG_PTR0, (RCS421RegVal) newProc->page_table);

    // Flush region 0 entries from the TLB
    WriteRegister(REG_TLB_FLUSH, (RCS421RegVal) TLB_FLUSH_0);

    return ctxp;
}


int Fork(void) {
    int i;
    int pid = active_process->pid;

    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

    // Create new page table
    struct pte *new_page_table = get_new_page_table();

    TracePrintf(0, "New page table: %x, active page table: %x\n",
        (unsigned int)new_page_table, (unsigned int)active_process->page_table);

    // Ensure virtual memory mappings are correct
    kernel_page_table[PAGE_TABLE_LEN - 1] = (struct pte) {
        .pfn = (unsigned int)(active_process->page_table) >> PAGESHIFT,
        .uprot = PROT_NONE,
        .kprot = PROT_READ | PROT_WRITE,
        .valid = 1
    };


    kernel_page_table[PAGE_TABLE_LEN - 2] = (struct pte) {
        .pfn = (unsigned int)(new_page_table) >> PAGESHIFT,
        .uprot = PROT_NONE,
        .kprot = PROT_READ | PROT_WRITE,
        .valid = 1
    };

    TracePrintf(0, "pfn = %u\n", kernel_page_table[PAGE_TABLE_LEN - 2].pfn);


    struct pte *cur_table_base = (struct pte *)(VMEM_1_LIMIT - PAGESIZE);
    struct pte *new_table_base = (struct pte *)(VMEM_1_LIMIT - 2 * PAGESIZE);
    void *temp = VMEM_1_LIMIT - 3 * PAGESIZE;

    int j = 0;
    while (*((unsigned int *)(new_table_base + j++ * sizeof(struct pte))) == 0);

    TracePrintf(0, "%x\n", *((unsigned int *)(new_table_base + j * sizeof(struct pte))));
    // Set up PTE's for new table
    memcpy((void *)new_table_base, (void *)cur_table_base,
        PAGE_TABLE_SIZE);
    TracePrintf(0, "%x\n", *((unsigned int *)(new_table_base + j * sizeof(struct pte))));

    for (i = 0; i < PAGE_TABLE_LEN; ++i) {
        if ((new_table_base + i)->valid == 1) {
            (new_table_base + i)->pfn = alloc_page();
            TracePrintf(0, "Assigned physical page %d to virtual page %d\n",
                        (new_table_base + i)->pfn, i);
        }
    }

//    TracePrintf(0, "%x\n", *((unsigned int *)(new_table_base )));

//    WriteRegister(REG_TLB_FLUSH, (RCS421RegVal) new_table_base);

    TracePrintf(0, "%x\n", *((unsigned int *)(new_table_base + j * sizeof(struct pte))));


    TracePrintf(0, "pfn = %u\n", kernel_page_table[PAGE_TABLE_LEN - 2].pfn);

    // Copy the old memory to the new memory
    for (i = 0; i < PAGE_TABLE_LEN; ++i) {

//        TracePrintf(0, "Page %d, valid = %d\n", i, (new_table_base + i)->valid);

        if ((new_table_base + i)->valid == 1) {

            TracePrintf(0, "Copying page %d, new_table_base = %x, pfn = %d, src_addr = %x, valid = %d\n",
                i, (unsigned int)new_table_base, (new_table_base + i)->pfn,
                        VMEM_0_BASE + i * PAGESIZE, (cur_table_base + i)->valid);

//            TracePrintf(0, "Updating region 1 page table\n");

            kernel_page_table[PAGE_TABLE_LEN - 3].pfn =
                (new_table_base + i)->pfn;

            WriteRegister(REG_TLB_FLUSH, (RCS421RegVal)temp);

            TracePrintf(0, "Calling memcpy, %x, %x\n", (unsigned int)temp,
                        (unsigned int)(VMEM_0_BASE + i * PAGESIZE));
            memcpy(temp, (const void *)(VMEM_0_BASE + i * PAGESIZE), PAGESIZE);
        }
    }


    TracePrintf(0, "pfn = %u, uprot = %x, kprot = %x, valid = %x\n",
                (new_table_base + 508)->pfn, (new_table_base + 508)->uprot,
                (new_table_base + 508)->kprot, (new_table_base + 508)->valid);

    TracePrintf(0, "pfn = %u, uprot = %x, kprot = %x, valid = %x\n",
                (cur_table_base + 508)->pfn, (cur_table_base + 508)->uprot,
                (cur_table_base + 508)->kprot, (cur_table_base + 508)->valid);
    
    TracePrintf(0, "Setting up PCB\n");
    // Create new pcb
    struct process_info *pcb = (struct process_info *)
        malloc(sizeof(struct process_info));

    pcb->pid = next_pid++;
    pcb->page_table = (void *)new_page_table;

    TracePrintf(0, "Adding PCB to queue\n");
    push_process(&process_queue, &pq_tail, active_process);

    // Flush the region 0 TLB
    // For some reason this is causing the PC to go to 0
//    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

    TracePrintf(0, "Context switching to child\n");
    // Switch to the child process
    ContextSwitch(ForkContextSwitchHelper, &(active_process->ctx),
        (void *)active_process, (void *)pcb);

    // Return 0 if in calling process, new pid otherwise
    if (active_process->pid == pid)
        return 0;

    return active_process->pid;
}


int Exec(char *filename, char **argvec) {
    return 0;
}


void Exit(int status) {
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
    long new_brk = (long)UP_TO_PAGE(addr) / PAGESIZE;

    if (new_brk > VMEM_LIMIT / PAGESIZE - 5) {
        TracePrintf(0, "User heap attempting to grow into the User stack/redzone\n");
        return ERROR;
    }
    
    struct pte* page_table = (struct pte*)(VMEM_LIMIT - PAGESIZE);
    for (i = MEM_INVALID_PAGES; i < VMEM_LIMIT / PAGESIZE - 5; i++) {
        if (!page_table[i].valid) {
            *(page_table + i) = (struct pte){
                .valid = 1,
                .kprot = PROT_READ | PROT_WRITE,
                .uprot = PROT_READ | PROT_WRITE,
                .pfn = alloc_page()
            };
        }
    }
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

