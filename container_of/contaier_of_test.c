#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#define container_of(ptr, type, member) ({ \
	const typeof(((type *)0)->member) * __mptr = (ptr); \
	(type *)((char *)__mptr - offsetof(type, member)); })

struct test_struct {
	int x;
	char y;
};

struct test_struct *s;

int main(int argc, char **argv)
{
	struct test_struct *p;

	s = (struct test_struct*)malloc(sizeof(struct test_struct));
	if (!s)
		return 1;

	printf("s=0x%08x\n", s);

	p = container_of(&s->x, struct test_struct, x);

	printf("p=0x%08x\n", p);

	return 0;
}
