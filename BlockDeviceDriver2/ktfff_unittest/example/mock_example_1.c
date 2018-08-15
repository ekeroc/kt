/*
gcc mock_example_1.c -o mock_example_1 && \
./mock_example_1
*/
#include <stdio.h>
#define TEST
int foo(int x);


/* Orignal C code */
#ifdef TEST
int (*test_foo)(int x) = NULL;
int foo__(int x)
#else
int foo(int x)
#endif
{
	int result = x;
	/* Do something complex to calculate the result */
	printf("original foo result: ");
	return result;
}





/* Testing Header File */
#ifdef TEST
int foo(int x)
{   
	if (test_foo) return test_foo(x);
	return foo__(x);
}
#endif

/* Testing C Code */
int mock_foo1(int x)
{
	return 0;
}

int mock_foo2(int x)
{
	return 5;
}

void main(void)
{
	test_foo = mock_foo1;
    printf("mock_foo1: %d\n", foo(9));
	test_foo = mock_foo2;
	printf("mock_foo2: %d\n", foo(9));
	test_foo = NULL;
    printf("%d\n", foo(9));
}