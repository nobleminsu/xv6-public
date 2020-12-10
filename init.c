// init: The initial user-level program

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

char *argv[] = { "sh", 0 };
char *ckpt_argv[] = {"ckptdaemon", 0};

int
main(void)
{
  int pid, wpid, cpid;

  if(open("console", O_RDWR) < 0){
    mknod("console", 1, 1);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr

  for(;;){
    printf(1, "init: starting sh\n");
    pid = fork();
    if(pid < 0){
      printf(1, "init: fork failed\n");
      exit();
    }
    if(pid == 0){
      exec("sh", argv);
      printf(1, "init: exec sh failed\n");
      exit();
    }

    cpid = fork();
    if (cpid < 0)
    {
      printf(1, "init: fork failed\n");
      exit();
    }
    if (cpid == 0)
    {
      exec("ckptdaemon", ckpt_argv);
      printf(1, "init: exec ckpt failed\n");
      exit();
    }

    while((wpid=wait()) >= 0 && (wpid != pid && wpid != cpid))
      printf(1, "zombie!\n");
  }
}
