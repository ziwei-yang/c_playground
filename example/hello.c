#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#define SWAP(a, b) do { typeof(a) t; t = a; a = b; b = t; } while(0)
#define EPRINT(msg) fprintf (stderr, "%s:%d %s(): %s\n", __FILE__, __LINE__, __FUNCTION__,  msg);

int f_test() {
	size_t bufsize = 1000;
	char *buffer = (char *)malloc(bufsize * sizeof(char));
	if( buffer == NULL) {
		perror("Unable to allocate buffer");
		exit(1);
	}

	void *fp = fopen("./hello.c", "r");
	while(1) {
		ssize_t l_len = getline(&buffer, &bufsize, fp);
		printf("l_len: %zd \n", l_len);
		printf("bufsize: %zd \n", bufsize);
		printf("line : '%s'\n", buffer);
		printf("len  : %lu\n\n", strlen(buffer));
		if (l_len <= 0) break;
		break;
	}
	return 0;
}

int type_test() {
	unsigned long ct = 1;
	for (;ct>=1;ct*=2) {
		printf("hello %24lu\n", ct);
	}
	printf("hello %24lu\n", ct);
	ct = (unsigned long)(-1);
	printf("max  %24lu\n", ct);
	return 0;
}

int array_test() {
	printf("size of char %lu\n", sizeof(char));
	printf("size of int  %lu\n", sizeof(int));
	int array[10];
	array[0] = 0;
	array[1] = 1;
	array[2] = 2;
	char *cp = (char *)(&array);
	void *vp = (void *)(&array);
	printf("array0 addr %lu\n", (unsigned long)array);
	printf("cp     addr %lu\n", (unsigned long)cp);
	printf("vp     addr %lu\n", (unsigned long)vp);
	printf("array0 val  %d\n", *array);
	printf("array1 addr %lu\n", (unsigned long)(array+1));
	printf("cp+1   addr %lu\n", (unsigned long)(cp + 1));
	printf("vp+1   addr %lu\n", (unsigned long)(vp + 1));
	printf("cp+1   val  %hhd\n", *(cp + 1));
	printf("array1 val  %d\n", *(array + 1));
	return 0;
}

int str_test() {
	char *s = "abcde";
	printf("s    : %s\n", s);
	printf("len  : %lu\n", strlen(s));
	return 0;
}

int for_test() {
	printf("forloop test\n");
	for (int i=0; i<-1; i++)
		printf("for i %d\n", i);
	return 0;
}

int main() {
	f_test();
	str_test();
	for_test();
	// EPRINT("something wrong");
	// array_test();
}
