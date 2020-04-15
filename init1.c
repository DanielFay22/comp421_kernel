#include <unistd.h>
#include <comp421/yalnix.h>

int
main()
{
    TracePrintf(0, "Init: %d\n", GetPid());

//    int *status_ptr = (int *)malloc(sizeof(int));
    int i, stat;

    while (1) {
        if (Fork() == 0) {
            Exec("exec_test", NULL);
        } else {
            TracePrintf(0, "Parent process waiting\n");
            i = Wait(&stat);
            TracePrintf(0, "Process %d exited with status %d\n",
                i, stat);

            Delay(1);
        }
    }

//	TracePrintf(0, "pid %d\n", Fork());
//    write(2, "init!\n", 6);
//    TracePrintf(0, "about to enter loop\n");
//
//    void *ptr = malloc(1000);
//
//    if (ptr != NULL) {
//        TracePrintf(0, "Successfully allocated 1000 bytes at address %x\n",
//            (unsigned int) ptr);
//    } else
//        TracePrintf(0, "Allocation failed\n");
//
//    while(1) {
//    	TracePrintf(0, "enter loop\n");
//    	TracePrintf(0, "delaying\n");
//    	Delay(5);
//    }
//    Halt();
}
