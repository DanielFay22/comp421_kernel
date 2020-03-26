
#ifndef _kernel_h
#define _kernel_h

#ifndef _yalnix_h
#include "comp421/yalnix.h"

#endif

#ifndef _hardware_h
#include "comp421/hardware.h"

#endif



// Interrupt handlers
void trap_kernel_handler(ExceptionInfo *exceptionInfo);
void trap_clock_handler(ExceptionInfo *exceptionInfo);
void trap_illegal_handler(ExceptionInfo *exceptionInfo);
void trap_memory_handler(ExceptionInfo *exceptionInfo);
void trap_math_handler(ExceptionInfo *exceptionInfo);
void trap_tty_transmit_handler(ExceptionInfo *exceptionInfo);
void trap_tty_receive_handler(ExceptionInfo *exceptionInfo);

void idle_process(void);

int alloc_page(void);
int free_page(int pfn);

SavedContext *ContextSwitchFunc(SavedContext *, void *, void *);


struct free_page {
    char in_use;
};

struct free_page *free_pages;

unsigned int tot_pmem_size;
unsigned int tot_pmem_pages;
void *cur_brk;

char vmem_enabled;


struct pte kernel_page_table[PAGE_TABLE_LEN];
struct pte idle_page_table[PAGE_TABLE_LEN];



struct process_info {
    unsigned int pid;
    struct pte *page_table;
    unsigned int user_pages;    // Number of allocated pages (excluding kernel stack)
    SavedContext ctx;
    struct process_info *next_process;
};

struct process_info *processes;
struct process_info *active_process;
struct process_info *process_queue;


int allocated_pages;



#define MAX_CLOCK_TICKS 2

unsigned int active_clock_count;


#endif
