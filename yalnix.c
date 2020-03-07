
#include <stddef.h>

#include "comp421/yalnix.h"
#include "comp421/hardware.h"



void trap_kernel_handler(ExceptionInfo *exceptionInfo);
void trap_clock_handler(ExceptionInfo *exceptionInfo);
void trap_illegal_handler(ExceptionInfo *exceptionInfo);
void trap_memory_handler(ExceptionInfo *exceptionInfo);
void trap_math_handler(ExceptionInfo *exceptionInfo);
void trap_tty_transmit_handler(ExceptionInfo *exceptionInfo);
void trap_tty_receive_handler(ExceptionInfo *exceptionInfo);



void (*interrupt_table)(ExceptionInfo *)[TRAP_VECTOR_SIZE] = {NULL};






void trap_kernel_handler(ExceptionInfo *exceptionInfo) {

}

void trap_clock_handler(ExceptionInfo *exceptionInfo) {

}

void trap_illegal_handler(ExceptionInfo *exceptionInfo) {

}

void trap_memory_handler(ExceptionInfo *exceptionInfo) {

}

void trap_math_handler(ExceptionInfo *exceptionInfo) {

}

void trap_tty_transmit_handler(ExceptionInfo *exceptionInfo) {

}

void trap_tty_receive_handler(ExceptionInfo *exceptionInfo) {

}






void KernelStart(ExceptionInfo *info, unsigned int pmem_size,
    void *orig_brk, char **cmd_args) {

    // Setup interrupt vector table
    interrupt_table[TRAP_KERNEL] = &trap_kernel_handler;
    interrupt_table[TRAP_CLOCK] = &trap_clock_handler;
    interrupt_table[TRAP_ILLEGAL] = &trap_illegal_handler;
    interrupt_table[TRAP_MEMORY] = &trap_memory_handler;
    interrupt_table[TRAP_MATH] = &trap_math_handler;
    interrupt_table[TRAP_TTY_TRANSMIT] = &trap_tty_transmit_handler;
    interrupt_table[TRAP_TTY_RECEIVE] = &trap_tty_receive_handler;

    // Write address of interrupt vector table to REG_VECTOR_BASE register
    (void (**)(ExceptionInfo *)) (REG_VECTOR_BASE) = interrupt_table;



}



