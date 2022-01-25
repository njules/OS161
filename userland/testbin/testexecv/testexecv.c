#include <unistd.h>
#include <stdio.h>

int
main()
{
	char arg1[] = "foo";
	char arg2[] = "bar";
	char* args[] = {arg1, arg2, NULL};

	printf("Should print foo bar now.\n");
	execv("testbin/argtest", args);
}
