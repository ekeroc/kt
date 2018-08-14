/*
gcc mock_example_2.c -o mock_example_2 && \
./mock_example_2
*/
#define TEST
#include <stdio.h>
#include "mock_example_2.h"


int foo(int x);

/* Orignal C Code */
int MOCK(foo)(int x)
{
	int result = x;
	/* Do something complex to calculate the result */
	
	return result;
}


/* Testing Code */
DECLARE_MOCK(foo);

int mock1(int x){
	return 2;
}

int mock2(int x){
	return 1;
}

void main(void)
{
	test_foo = mock1; /* foo__ = mock1 */
    printf("mock1: %d\n", foo(999));
	test_foo = mock2;
	printf("mock2: %d\n", foo(999));
	test_foo = NULL;
    printf("%d\n", foo(999));
}
