#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "test.h"

int main(int argc, char **argv)
{
	int fd;
	struct shared shared, *ptr;

	fd = open("./sharedfile", O_RDWR | O_CREAT);
	if (fd < 0) {
		perror("open file failed");
		return -1;
	}

	write(fd, &shared, sizeof(struct shared));

	ptr = mmap(NULL, sizeof(struct shared), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (ptr == NULL) {
		perror("mmap failed");
		return -1;
	}

	close(fd);

	sem_init(&ptr->mutex, 1, 1);

	for (;;) {
		sem_wait(&ptr->mutex);
		printf("%s:%d\n", __FILE__, ptr->count++);
		sem_post(&ptr->mutex);
	}
	
	munmap(ptr, sizeof(struct shared));

	return 0;
}



