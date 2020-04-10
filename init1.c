#include <unistd.h>
#include <comp421/yalnix.h>

int
main()
{	
	TracePrintf(0, "pid %d\n", GetPid());
    write(2, "init!\n", 6);
    Exit(0);
}
