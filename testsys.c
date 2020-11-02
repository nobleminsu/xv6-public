//testsys.c

#include "types.h"
#include "user.h"
#include "fcntl.h"

int main()
{
    int i = 1;
    char *str = "hello";
    int fd = open("test", O_CREATE);
    testsys(i, &i, str, fd);
    exit();
}