#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>

void createrf()
{
	FILE* f = fopen("/root/archivo", "w+");
	if (f == NULL)
		{
			printf ("NO PUEDO\n");
			exit(-1);
		}
	fprintf(f, "HOLA MUNDO!\n");
	fclose(f);
}

int main()
{
	printf ("Soy: %d\n", getuid());
	createrf();
	return 0;
}
