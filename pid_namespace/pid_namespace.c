//http://lwn.net/Articles/259217/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sched.h>

static int fork_child(void *arg)
{
	int a = (int)arg;

	int i;
	pid_t pid;

	printf("In the container, my pid is: %d\n", getpid());
	for (i = 0; i < a; i++) {
		pid = fork();
		if (pid < 0)
			return pid;

		else if (pid)
			printf("pid of my child is %d\n", pid);

		else if (pid == 0) {
			sleep(3);
			exit(0);
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	int cpid;
	void *childstack, *stack;

	int flags;
	int ret = 0;
	int stacksize = getpagesize() * 4;

	if (argc != 2) {
		fprintf(stderr, "wrong usage.\n");
		return -1;
	}

	stack = malloc(stacksize);
	if (!stack) {
		perror("malloc");
		return -1;
	}

	printf("Out of the container, my pid is: %d\n", getpid());

	childstack = stack + stacksize;

	flags = CLONE_NEWPID | CLONE_NEWNS;

	cpid = clone(fork_child, childstack, flags, (void*)atoi(argv[1]));
	printf("cpid: %d\n", cpid);

	if (cpid < 0) {
		perror("clone");
		ret = -1;
		goto out;
	}

	fprintf(stderr, "Parent sleeping 20 seconds\n");
	sleep(20);
	ret = 0;

out:
	free(stack);
	return ret;

}
