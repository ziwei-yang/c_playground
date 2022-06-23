#include <stdio.h>
#include <stdbool.h>

int main() {
	bool flag = 5;
	printf("flag bool size is %lu\n", sizeof(flag));
	printf("flag bool 5 is %d\n", flag);
	flag = true;
	printf("flag bool size is %lu\n", sizeof(flag));
	printf("flag bool true is %d\n", flag);
	flag = false;
	printf("flag bool size is %lu\n", sizeof(flag));
	printf("flag bool true is %d\n", flag);
	if (5 == true)
		printf("5 == true\n");
	if (1 == true)
		printf("1 == true\n");
	if (0 == false)
		printf("0 == false\n");
	if (-1 == false)
		printf("-1 == false\n");
}
