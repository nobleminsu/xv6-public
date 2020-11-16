#include "types.h"
#include "user.h"
#include "fcntl.h"

void
starttest(int count, void (*test)(int), int withreadlock)
{
	int i,pid = 0;

	if (withreadlock)
		rwsematest(1);

	for (i = 0; i < count; i++) {
		pid = fork();

		if (!pid) 
			break;
	}
	
	if (pid) {
		if(withreadlock) {
			sleep(50);
			rwsematest(2);
		}

		for (i = 0; i < count; i++) 
			wait();
	}
	else {
		test(i);
		exit();
	}
}

void 
readlocktest(int i, int time)
{
	rwsematest(1);
	printf (1, "%d down read\n", i);
	sleep(time);
	printf (1, "%d up read\n", i);
	rwsematest(2);
}

void
writelocktest(int i, int time)
{
	rwsematest(3);
	printf (1, "%d down write\n", i);
	sleep(time);
	printf (1, "%d up write\n", i);
	rwsematest(4);
}

void
test1(int i)
{
	readlocktest(i, (i+1)*20);
}

void
test2(int i)
{
	sleep((5-i)*10);
	writelocktest(i, (i+2)*10);
}

void 
test3(int i)
{
	switch (i) {
		case 0:
			sleep(10);
			writelocktest(i, 50);
			break;
		case 1:
		case 2:
			sleep(25 + i*10);
		case 3:
		case 4:
			readlocktest(i, 50 + i*10);
	}
}
		
int 
main()
{
	// initialize the semaphore
	rwsematest(0);

	printf(1, "\nread lock test\n");
	starttest(5, test1, 0);
	
	printf(1, "\nwrite lock test\n");
	starttest(5, test2, 0);

	printf(1, "\nread & write lock test\n");
	starttest(5, test3, 0);

	exit();
}
