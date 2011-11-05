#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

struct shared {
	sem_t mutex;
	int count;
} shared;

int main(int argc, char **argv)
{
	int fd;
	struct shared *ptr;

	fd = open("./sharefile", O_RDWR);
	if (fd < 0) {
		printf("open file error\n");
		return 1;
	}

	write(fd, &shared, sizeof(struct shared));

	ptr = mmap(NULL, sizeof(struct shared), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (ptr == NULL) {
		printf("mmap error\n");
		return 1;
	}

	close(fd);

	sem_init(&ptr->mutex, 1, 1);

	for (;;) {
		sem_wait(&ptr->mutex);
		printf("%s:%d\n", __FILE__, ptr->count++);
		sem_post(&ptr->mutex);
	}

	return 0;
}



