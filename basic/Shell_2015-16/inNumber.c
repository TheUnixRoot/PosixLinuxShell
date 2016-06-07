#include <stdio.h>

int main(int argc, char const *argv[])
{
	int i = 0;
	scanf("%d", &i);
	printf("%d %s\n", i, argv[1]);
	return 0;
}