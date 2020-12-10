#include "types.h"
#include "syscall.h"
#include "user.h"

int main(void)
{
    printf(0, "running ckptdaemon\n");
    
    startckpt();

    return 0;
}