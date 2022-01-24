#include <unistd.h>
#include <stdio.h>

int main (void) {
	char* args[] = {"foo", "bar"};
	printf("Should print foo bar now.");
	execv("argtest", args);
}
