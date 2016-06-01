#include <stdio.h>
#include <unistd.h>

int main() {
	while(1) {
		printf("Hola mundo\n");
		fflush(stdout);
		sleep(5);
	}
}