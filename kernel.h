
#ifndef _kernel_h
#define _kernel_h

#ifndef _yalnix_h
#include "comp421/yalnix.h"

#endif

#ifndef _hardware_h
#include "comp421/hardware.h"

#endif

#include <stddef.h>
#include <stdio.h>

#define NUM_RESERVED_KERNEL_PAGES   3

#define CURRENT_PAGE_TABLE          ((struct pte *)(VMEM_LIMIT - PAGESIZE))

#define MAX_CLOCK_TICKS 2

#define NO_PARENT   -1


// Struct definitions
struct avaliable_line {
    char *line;
    char *orig_ptr;
    unsigned int free : 1;
    int len;
    struct avaliable_line *next;
};

struct terminal_info {
    struct process_info *r_head;
    struct process_info *r_tail;

    struct process_info *w_head;
    struct process_info *w_tail;

    struct avaliable_line *next_line;
    struct avaliable_line *last_line;
};

struct process_info {
    unsigned int pid;
    unsigned int delay_ticks;
    void *page_table;
    unsigned int user_pages;    // Number of allocated pages (excluding kernel stack)
    SavedContext ctx;
    void *user_brk;
    unsigned int parent;
    int active_children;
    int exited_children;
    struct process_info *next_process;
    struct process_info *prev_process;
    struct avaliable_line *line;
    int seeking_len;
};

struct free_page {
    char in_use;
};

struct exit_status {
    unsigned int pid;
    int status;
    unsigned int parent;
    struct exit_status *prev;
    struct exit_status *next;
};

struct terminal_info *terminals[NUM_TERMINALS];

// Util function definitions
extern struct pte *get_new_page_table(void);
extern void free_page_table(struct pte *pt);
extern unsigned int alloc_page(void);
extern int free_page(int pfn);

extern void push_process(struct process_info **head, struct process_info **tail,
    struct process_info *new_pcb);
extern struct process_info *pop_process(struct process_info **head,
    struct process_info **tail);
extern void remove_process(struct process_info **head,
    struct process_info **tail, struct process_info *pi);

// Context Switch function definitions
extern SavedContext *ContextSwitchFunc(SavedContext *, void *, void *);
extern SavedContext *ContextSwitchForkHelper(SavedContext *, void *, void *);
extern SavedContext *ContextSwitchInitHelper(SavedContext *, void *, void *);
extern SavedContext *ContextSwitchExitHelper(SavedContext *, void *, void *);

// Kernel Call function definitions
extern int KernelFork(void);
extern void KernelExec(ExceptionInfo *info);
extern void KernelExit(int status);
extern int KernelWait(int *status_ptr);
extern int KernelBrk(void *addr);
extern int KernelDelay(int clock_ticks);
extern int KernelTtyRead(int tty_id, void *buf, int len);
extern int KernelTtyWrite(int tty_id, void *buf, int len);

// Load Program function definitions
extern int LoadProgram(char *name, char **args, ExceptionInfo *info);

// Interrupt Handler function definitions
extern void trap_kernel_handler(ExceptionInfo *exceptionInfo);
extern void trap_clock_handler(ExceptionInfo *exceptionInfo);
extern void trap_illegal_handler(ExceptionInfo *exceptionInfo);
extern void trap_memory_handler(ExceptionInfo *exceptionInfo);
extern void trap_math_handler(ExceptionInfo *exceptionInfo);
extern void trap_tty_transmit_handler(ExceptionInfo *exceptionInfo);
extern void trap_tty_receive_handler(ExceptionInfo *exceptionInfo);




unsigned int next_pid;

struct free_page *free_pages;

unsigned int tot_pmem_size;
unsigned int tot_pmem_pages;
void *cur_brk;

char vmem_enabled;


struct pte kernel_page_table[PAGE_TABLE_LEN];
//struct pte idle_page_table[PAGE_TABLE_LEN];


struct process_info *idle;
struct process_info *processes;
struct process_info *active_process;
struct process_info *process_queue;
struct process_info *pq_tail;

struct process_info *waiting_queue;
struct process_info *wq_tail;

int allocated_pages;


struct exit_status *exit_queue;
struct exit_status *eq_tail;


unsigned int clock_count;
unsigned int last_switch;


#endif
