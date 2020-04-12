#include <unistd.h>
#include <comp421/yalnix.h>

int
main()
{	
	TracePrintf(0, "pid %d\n", GetPid());
    write(2, "init!\n", 6);
    TracePrintf(0, "about to enter loop\n");
    while(1) {
    	TracePrintf(0, "enter loop\n");
    	TracePrintf(0, "delaying\n");
    	Delay(5);
    }
    Halt();
}
