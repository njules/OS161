#include <unistd.h>
#include <stdio.h>

int
main()
{
	char arg1[] = "foo";
	char arg2[] = "bar";
	char* args[] = {arg1, arg2};
	printf("Should print foo bar now.");
	execv("argtest", args);
}
