#include <unistd.h>
#include <stdio.h>

#define PATH_MAX 128

int
main()
{
	char cwd[PATH_MAX+1];
	char testfolder[] = "testbin/";
	int err;
	
	getcwd(cwd, PATH_MAX+1);
	printf("Current working directory: \"%s\"\n", cwd);

	err = chdir(testfolder);
	if (err) {
		printf("Couldn't change working directory because %d\n", err);
	} else {
		getcwd(cwd, PATH_MAX+1);
		printf("Changed working directory to: \"%s\"\n", cwd);
	}
}
