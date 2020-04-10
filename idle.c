#include <unistd.h>
#include <comp421/yalnix.h>
#include <comp421/hardware.h>

int
main()
{	
	TracePrintf(0, "pid %d\n", GetPid());
    while(1) {
    	Pause();
    }
    Exit(0);
}
