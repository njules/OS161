#include <unistd.h>
#include <stdio.h>

int
main()
{
	char arg1[] = "foo";
//	char arg2[] = "bar";
	char arg3[] = "os161";
	char arg4[] = "execv";
	char* args[] = {arg1, arg3, arg4, NULL};

	printf("Should print foo bar now.\n");
	execv("testbin/argtest", args);
}
