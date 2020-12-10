#include "types.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "user.h"
#include "syscall.h"

int main(void)
{
    printf(0, "running logckpt\n");
    runckpt();
}
