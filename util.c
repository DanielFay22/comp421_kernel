
#include "kernel.h"



struct available_page_table {
    void *page_table;
    struct available_page_table *next;
};

struct available_page_table *page_tables = NULL;


/*
 * Gets a pointer to a new page table.
 *
 * If there is an existing region of physical memory
 * allocated but not in use, that is returned. Otherwise
 * a new page is allocated, half is saved for a later
 * call and the other half is returned.
 */
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
//        struct available_page_table *pt_ptr = (struct available_page_table *)
//            malloc(sizeof(struct available_page_table));
//
//        // Add the extra half page to the free tables list
//        pt_ptr->page_table = (void *)(pfn << PAGESHIFT + PAGESIZE / 2);
//        pt_ptr->next = page_tables;
//        page_tables = pt_ptr;

        page_table = (void *)(pfn << PAGESHIFT);
    }

    return (struct pte *)page_table;
}

/*
 * Frees the physical memory used to store a page table.
 *
 * If the other half of the physical page containing the
 * given page table is still in use, the memory is saved
 * to be reused by later calls. If the other half is also
 * free, then the physical page is freed.
 */
void free_page_table(struct pte *pt) {
    unsigned int pfn = ((unsigned int)pt) >> PAGESHIFT;
    free_page(pfn);
//    struct available_page_table *head = page_tables;
//    struct available_page_table *prev = NULL;
//    while (head != NULL) {
//        if (((unsigned int)head->page_table) >> PAGESHIFT == pfn)
//            break;
//        prev = head;
//        head = head->next;
//    }
//
//    // If both halves of the page frame are not in use, free the page
//    if (head != NULL) {
//        if (prev == NULL)
//            page_tables = head->next;
//        else
//            prev->next = head->next;
//
//        free(head);
//        free_page(pfn);
//    } else {
//        // Otherwise add an entry to the list of free pagetables
//        struct available_page_table *apt = (struct available_page_table *)
//            malloc(sizeof(struct available_page_table));
//        *apt = (struct available_page_table) {
//            .page_table = (void *)pt,
//            .next = page_tables
//        };
//
//        page_tables = apt;
//    }

}

/*
 * Allocates a page of physical memory.
 *
 * If there is an available page of physical memory, returns
 * the pfn of a newly allocated page.
 *
 * Returns ERROR if there are no free pages.
 */
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

/*
 * Marks the provided page frame as free.
 *
 * pfn should be a number corresponding to a page of physical memory.
 *
 * Returns ERROR if the provided pfn is invalid, 0 otherwise.
 */
int free_page(int pfn) {

    if (pfn < 0 || pfn >= tot_pmem_pages)
        return ERROR;

    // Check page was actually marked used
    if ((free_pages + pfn)->in_use) {
        (free_pages + pfn)->in_use = 0;
        --allocated_pages;
    }
    return 0;
}

/*
 * Adds a single pcb to the provided queue.
 *
 * *head should be a pointer to the head of the queue
 * *tail should be a pointer to the tail of the queue
 * new_pcb is the pcb you want to add to the queue
 */
void push_process(struct process_info **head, struct process_info **tail,
    struct process_info *new_pcb) {

    TracePrintf(1, "PUSH PROCESS: Pushing process %d onto a queue\n", new_pcb->pid);

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

/*
 * Removes and returns a single pcb from the provided queue
 *
 * *head should be a pointer to the head of the queue
 * *tail should be a pointer to the tail of the queue
 *
 * Returns a pointer to the pcb that was previously at the
 * head of the queue, unless the queue is empty, in which case
 * returns NULL.
 */
struct process_info *pop_process(struct process_info **head,
    struct process_info **tail) {

    struct process_info *new_head = *head;

    if (new_head != NULL) {
        *head = new_head->next_process;
        new_head->next_process = NULL;
    }

    if (*head != NULL)
        (*head)->prev_process = NULL;

    return new_head;
}

/*
 * Removes the given pcb from the provided queue.
 *
 * *head should be a pointer to the head of the queue
 * *tail should be a pointer to the tail of the queue
 * pi should be a pointer to a pcb which is contained in the given queue
 */
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
