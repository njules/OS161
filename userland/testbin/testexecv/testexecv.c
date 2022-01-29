#include <unistd.h>
#include <stdio.h>

int
main()
{
	char arg1[] = "foo";
	char arg2[] = "os161";
	char arg3[] = "bar";
	char* args[] = {arg1, arg2, arg3, NULL};

	printf("Should print foo os161 bar now.\n");
	execv("testbin/argtest", args);
}
