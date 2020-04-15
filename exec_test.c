#include <unistd.h>
#include <comp421/yalnix.h>


int main() {

    TracePrintf(0, "Hello from exec_test!\n");

    Delay(5);

    Exit(0);
}

