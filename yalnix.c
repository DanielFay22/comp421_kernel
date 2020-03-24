
#include <stddef.h>
#include <stdio.h>

#include "comp421/yalnix.h"
#include "comp421/hardware.h"



void trap_kernel_handler(ExceptionInfo *exceptionInfo);
void trap_clock_handler(ExceptionInfo *exceptionInfo);
void trap_illegal_handler(ExceptionInfo *exceptionInfo);
void trap_memory_handler(ExceptionInfo *exceptionInfo);
void trap_math_handler(ExceptionInfo *exceptionInfo);
void trap_tty_transmit_handler(ExceptionInfo *exceptionInfo);
void trap_tty_receive_handler(ExceptionInfo *exceptionInfo);



void (*interrupt_table[TRAP_VECTOR_SIZE])(ExceptionInfo *) = {NULL};



struct free_page {
    char in_use;
};

struct free_page *free_pages;


unsigned int tot_pmem_size;
void *cur_brk = NULL;


struct pte kernel_page_table[PAGE_TABLE_LEN];

struct pte idle_page_table[PAGE_TABLE_LEN];




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
    TracePrintf(1, "trap_clock_handler");
    Halt();
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

    struct pte *table = (struct pte *)cur_brk;

    SetKernelBrk((void *) (table + PAGE_TABLE_LEN));

    // TODO: allocate and initializing a new page table
    return table;
}


void KernelStart(ExceptionInfo *info, unsigned int pmem_size,
    void *orig_brk, char **cmd_args) {

    tot_pmem_size = pmem_size;
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
    free_pages = (struct free_page *)cur_brk;
    cur_brk = UP_TO_PAGE(free_pages + (pmem_size / PAGESIZE));

    // TODO: Initialize kernel page table

    int i;
    long end_text = (long)&_etext;
    int text_pages = (end_text - VMEM_1_BASE) / PAGESIZE;
    for (i = 0; i < text_pages; i++) {
        struct pte entry = {
            .pfn = (VMEM_1_BASE + i * PAGESIZE) >> PAGESHIFT,
            .unused = 0b11111,
            .uprot = 0b000,
            .kprot = 0b101,
            .valid = 0b1
        };
        kernel_page_table[i] = entry;
    }

    int heap_pages = ((long)cur_brk - (long)&_etext) / PAGESIZE;
    for (i = text_pages; i < text_pages + heap_pages; i++) {
        struct pte entry = {
            .pfn = (VMEM_1_BASE + i * PAGESIZE) >> PAGESHIFT,
            .unused = 0b11111,
            .uprot = 0b000,
            .kprot = 0b011,
            .valid = 0b1
        };
        kernel_page_table[i] = entry;
    }
    
    int k_unused_pages = (VMEM_LIMIT - (long)cur_brk) / PAGESIZE;
    for (i = text_pages + heap_pages; i < text_pages + heap_pages + k_unused_pages; i++) {
        struct pte entry = {(VMEM_1_BASE + i * PAGESIZE) >> PAGESHIFT, 0b00000, 0b000, 0b000, 0b0};
        kernel_page_table[i] = entry;
    }

    // Initialize Region 0
    for (i = 4; i > 0; i--) {
        struct pte entry = {
            .pfn = (VMEM_1_BASE - i * PAGESIZE) >> PAGESHIFT,
            .unused = 0b00000,
            .uprot = 0b000,
            .kprot = 0b011,
            .valid = 0b1
        };

        idle_page_table[PAGE_TABLE_LEN - i] = entry;
    }

    struct pte entry = {
        .pfn = (VMEM_1_BASE - 5 * PAGESIZE) >> PAGESHIFT,
        .unused = 0b00000,
        .uprot = 0b110,
        .kprot = 0b011,
        .valid = 0b1
    };

    idle_page_table[PAGE_TABLE_LEN - 5] = entry;

    for (i = 0; i < PAGE_TABLE_LEN - 5; i++) {
        struct pte entry = {
            .pfn = (VMEM_BASE + i * PAGESIZE) >> PAGESHIFT,
            .unused = 0b00000,
            .uprot = 0b000,
            .kprot = 0b000,
            .valid = 0b0
        };

        idle_page_table[i] = entry;
    }

    WriteRegister(REG_PTR0, (RCS421RegVal) &idle_page_table);
    WriteRegister(REG_PTR1, (RCS421RegVal) &kernel_page_table);

    // TODO: structure associating Pid's with page tables
    //  -hash table with Pid as key


    WriteRegister(REG_VM_ENABLE, 1);
}


int SetKernelBrk(void *addr) {
    return 0;
}




