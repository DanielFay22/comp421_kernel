
#ifndef _kernel_h
#define _kernel_h

#ifndef _yalnix_h
#include "comp421/yalnix.h"

#endif

#ifndef _hardware_h
#include "comp421/hardware.h"

#endif

#define NUM_RESERVED_KERNEL_PAGES   3

#define CURRENT_PAGE_TABLE          ((struct pte *)(VMEM_LIMIT - PAGESIZE))

unsigned int next_pid;


extern unsigned int alloc_page(void);
extern int free_page(int pfn);

extern SavedContext *ContextSwitchFunc(SavedContext *, void *, void *);
extern SavedContext *ContextSwitchForkHelper(SavedContext *, void *, void *);
extern SavedContext *ContextSwitchInitHelper(SavedContext *, void *, void *);
extern SavedContext *ContextSwitchExitHelper(SavedContext *, void *, void *);

extern int LoadProgram(char *name, char **args, ExceptionInfo *info);

struct free_page {
    char in_use;
};

struct free_page *free_pages;

unsigned int tot_pmem_size;
unsigned int tot_pmem_pages;
void *cur_brk;

char vmem_enabled;


struct pte kernel_page_table[PAGE_TABLE_LEN];
//struct pte idle_page_table[PAGE_TABLE_LEN];



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
};

struct process_info *idle;
struct process_info *processes;
struct process_info *active_process;
struct process_info *process_queue;
struct process_info *pq_tail;

struct process_info *waiting_queue;
struct process_info *wq_tail;


int allocated_pages;


struct exit_status {
    unsigned int pid;
    int status;
    unsigned int parent;
    struct exit_status *prev;
    struct exit_status *next;
};

struct exit_status *exit_queue;
struct exit_status *eq_tail;



#define MAX_CLOCK_TICKS 2

#define NO_PARENT   -1

unsigned int clock_count;
unsigned int last_switch;



void push_process(struct process_info **head, struct process_info **tail,
                  struct process_info *new_pcb);
struct process_info *pop_process(
    struct process_info **head, struct process_info **tail);
void remove_process(struct process_info **head, struct process_info **tail,
                    struct process_info *pi);

struct pte *get_new_page_table(void);
void free_page_table(struct pte *pt);

#endif
