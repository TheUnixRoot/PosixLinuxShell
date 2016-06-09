#include <stdio.h>

int main(int argc, char const *argv[])
{
	int i = 0;
	scanf("%d", &i);
	i++;
	FILE * fich;
	fich = fopen("salidaLast", "w");
	fprintf(fich, "%d\n", i);
	fclose(fich);
	return 0;
}