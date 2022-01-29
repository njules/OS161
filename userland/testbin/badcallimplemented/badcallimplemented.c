#include <unistd.h>
#include <stdio.h>

int
main()
{
	char badexecv[] = "a";
	char badwaitpid[] = "b";
	char badopen[] = "c";
	char badread[] = "d";
	char badwrite[] = "e";
	char badclose[] = "f";
	char badlseek[] = "j";
	char badchdir[] = "s";
	char baddup2[] = "w";
	char bad__getcwd[] = "z";
	char* badcalls[] = {badexecv, NULL};
	execv("testbin/badcall", badcalls);
}
