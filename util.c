
#include "kernel.h"



struct available_page_table {
    void *page_table;
    struct available_page_table *next;
};

struct available_page_table *page_tables = NULL;



struct pte *get_new_page_table() {
    void *page_table;

    // Check if there is an available allocated page table
    if (page_tables != NULL) {
        struct available_page_table *pt = page_tables;
        page_tables = page_tables->next;


        page_table = pt->page_table;
        free(pt);


    } else {
        // Allocate a new page
        unsigned int pfn = alloc_page();
        struct available_page_table *pt_ptr = (struct available_page_table *)
            malloc(sizeof(struct available_page_table));

        // Add the extra half page to the free tables list
        pt_ptr->page_table = (void *)(pfn << PAGESHIFT + PAGESIZE / 2);
        pt_ptr->next = page_tables;
        page_tables = pt_ptr;

        page_table = (void *)(pfn << PAGESHIFT);
    }

    return (struct pte *)page_table;
//
//
//
//
//
//
//
//
//
//    int i;
//    //TODO don't know if this is necessary
//
//    struct pte *table = (struct pte *)malloc(sizeof(struct pte) * PAGE_TABLE_LEN);
//
//    //struct pte *table = (struct pte *)malloc(sizeof(struct pte));
//
//    for (i = 0; i < MEM_INVALID_PAGES; ++i)
//        (table + i)->valid = 0;
//
//    for (i = 0; i < KERNEL_STACK_PAGES; ++i) {
//        struct pte entry = {
//            .pfn = alloc_page(),
//            .unused = 0b00000,
//            .uprot = PROT_NONE,
//            .kprot = PROT_READ | PROT_WRITE,
//            .valid = 0b1
//        };
//
//        *(table + KERNEL_STACK_BASE / PAGESIZE + i) = entry;
//    }
//
//    // Initialize user stack
//    struct pte entry = {
//        .pfn = alloc_page(),
//        .unused = 0,
//        .uprot = PROT_READ | PROT_WRITE,
//        .kprot = PROT_READ | PROT_WRITE,
//        .valid = 1
//    };
//
//
//    (table + USER_STACK_LIMIT / PAGESIZE)->valid = 0;
//    *(table + USER_STACK_LIMIT / PAGESIZE - 1) = entry;
//
//    // TODO: allocate and initializing a new page table
//    return table;
}

void free_page_table(struct pte *pt) {
    unsigned int pfn = ((unsigned int)pt) >> PAGESHIFT;

    struct available_page_table *head = page_tables;
    struct available_page_table *prev = NULL;
    while (head != NULL) {
        if (((unsigned int)head->page_table) >> PAGESHIFT == pfn)
            break;
        prev = head;
        head = head->next;
    }

    // If both halves of the page frame are not in use, free the page
    if (head != NULL) {
        if (prev == NULL)
            page_tables = head->next;
        else
            prev->next = head->next;

        free(head);
        free_page(pfn);
    } else {
        // Otherwise add an entry to the list of free pagetables
        struct available_page_table *apt = (struct available_page_table *)
            malloc(sizeof(struct available_page_table));
        *apt = (struct available_page_table) {
            .page_table = (void *)pt,
            .next = page_tables
        };

        page_tables = apt;
    }

}


unsigned int alloc_page(void) {
    unsigned int i;

    // No physical memory available
    if (allocated_pages == tot_pmem_pages)
        return ERROR;

    // Find next available page
    for (i = 0; i < tot_pmem_pages; ++i) {
        if ((free_pages + i)->in_use == 0) {
            (free_pages + i)->in_use = 1;
            ++allocated_pages;
            return i;
        }
    }

    return ERROR;
}

int free_page(int pfn) {
    // Check page was actually marked used
    if ((free_pages + pfn)->in_use) {
        (free_pages + pfn)->in_use = 0;
        --allocated_pages;
    }
    return 0;
}


// Add a new pcb to the given queue
void push_process(struct process_info **head, struct process_info **tail,
                  struct process_info *new_pcb) {

    TracePrintf(1, "PUSH PROCESS: Pushing process %d onto PQ\n", new_pcb->pid);

    if (*head == NULL) {
        *head = new_pcb;
        *tail = new_pcb;

        new_pcb->next_process = NULL;
        new_pcb->prev_process = NULL;
    } else {
        (*tail)->next_process = new_pcb;

        new_pcb->next_process = NULL;
        new_pcb->prev_process = *tail;

        *tail = new_pcb;
    }
}

// Pop the next process off the given queue
struct process_info *pop_process(
    struct process_info **head, struct process_info **tail) {

    struct process_info *new_head = *head;

    if (new_head != NULL) {
        *head = new_head->next_process;
        new_head->next_process = NULL;
    }

    if (*head != NULL)
        (*head)->prev_process = NULL;

    return new_head;
}

// Remove a pcb from whatever queue it is part of
void remove_process(struct process_info **head, struct process_info **tail,
                    struct process_info *pi) {

//    TracePrintf(0, "Remove process: head address = %x, tail address = %x\n",
//                (unsigned int)*head, (unsigned int)*tail);
//    TracePrintf(0, "Remove process: pi = %x\n", (unsigned int)pi);
//    TracePrintf(0, "Remove process: pi.next_process = %x, pi.prev_process = %x\n",
//                (unsigned int)pi->next_process, (unsigned int)pi->prev_process);

    // If pi is head or tail, update accordingly.
    if (*head == pi)
        *head = pi->next_process;
    if (*tail == pi)
        *tail = pi->prev_process;

    // Remove pi from linked list
    if (pi->prev_process != NULL)
        pi->prev_process->next_process = pi->next_process;

    if (pi->next_process != NULL)
        pi->next_process->prev_process = pi->prev_process;

    pi->prev_process = NULL;
    pi->next_process = NULL;

    TracePrintf(0, "Removing process %d from queue\n", pi->pid);

}
