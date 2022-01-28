#include <unistd.h>
#include <stdio.h>

int
main()
{
	char arg1[] = "foo";
	char arg2[] = "bar";
	char arg3[] = "1";
	char* args[] = {arg1, arg2, arg3, NULL};

	printf("Should print \"testbin/argtest foo bar 1\" now.\n");
	execv("testbin/argtest", args);
}
