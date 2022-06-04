#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char func1(int a) {
	return 'a';
}

int func2(int a) {
	return 0;
}

float* func3(int a) {
	float *x = malloc(sizeof(float));
	return x;
}

float func31(int a) {
	return 3.0f;
}

typedef float* (*func_type_a)(int);
func_type_a funca () {
	return &func3;
}

float (*func_type_b())(int) {
	return &func31;
}

//typedef float *(*func_type_4(char)) (int);
//func_type_4 func4(char z) {
//	return &funca;
//}

float *(*func5(void)) (int) {
	return &func3;
}

int main() {
	// char (*a[10]) (int) = {func1};
	perror("ER_WRONG_VALUE_FOR_VAR");
	return 0;
}
