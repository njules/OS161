#include <unistd.h>
#include <stdio.h>

#define WRITE_BUF 100  // TODO: try to play with this buffer, gets even weirder

int
main()
{
	const int fd_stdout = 1;
	const int fd_stdoutnew = 3;
	int err;

	err = write(fd_stdout, "Can print this to stdout.\n", WRITE_BUF);
	if (err<=0) {
		printf("Couldn't write to stdout: %d.\n", err);
		return 1;
	}
	
	err = dup2(fd_stdout, fd_stdoutnew);
	if (err) {
		printf("Couldn't dup2 stdout: %d.\n", err);
		return 1;
	}

	err = write(fd_stdoutnew, "This is now also stdout.\n", WRITE_BUF);
	if (err<=0) {
		printf("Couldn't write to new stdout: %d.\n", err);
		return 1;
	}

	err = close(fd_stdout);
	if (err) {
		printf("Couldn't close old stdout: %d.\n", err);
		return 1;
	}

	err = write(fd_stdout, "!Shouldn't be able write to old stdout anymore!\n", WRITE_BUF);
	if (err>=0) {
		return 1;
	}

	err = write(fd_stdoutnew, "Can still write to new stdout.\n", WRITE_BUF);
	if (err<=0) {
		printf("Couldn't write to new stdout: %d.\n", err);
		return 1;
	}
}
