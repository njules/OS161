#include <unistd.h>
#include <stdio.h>

#define PATH_MAX 128

int
main()
{
	char cwd[PATH_MAX+1];
	char testfolder[] = "..";
	
	int err;
	
	
	printf("Current working directory: \"%s\"\n", getcwd(cwd, PATH_MAX+1));

	err = chdir(testfolder);
	if (err) {
		printf("Couldn't change working directory because %d\n", err);
	} else {
		getcwd(cwd, PATH_MAX+1);
		
		if(cwd !=NULL){
			printf("Changed working directory to: \"%s\"\n", cwd);
		} else {
			printf("getcwd() error");
		}
	}
	return 0;
}
