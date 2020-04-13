
#ifndef _kernel_h
#define _kernel_h

#ifndef _yalnix_h
#include "comp421/yalnix.h"

#endif

#ifndef _hardware_h
#include "comp421/hardware.h"

#endif

unsigned int next_pid;


extern unsigned int alloc_page(void);
extern int free_page(int pfn);

extern SavedContext *ContextSwitchFunc(SavedContext *, void *, void *);

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
    void *pc;
    void *sp;
    SavedContext ctx;
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



#define MAX_CLOCK_TICKS 2

unsigned int clock_count;
unsigned int last_switch;



void push_process(struct process_info **head, struct process_info **tail,
                  struct process_info *new_pcb);
struct process_info *pop_process(
    struct process_info **head, struct process_info **tail);
void remove_process(struct process_info **head, struct process_info **tail,
                    struct process_info *pi);


#endif
