#include <stdio.h>
#include <err.h>

void HandleOptions(int argc, char *argv[]){
	if(argc == 1)
		errx(1, "Tar needs arguments");
	printf("%d \n", argc);
	for(int i = 0; i < argc; i++){
		printf("%s\n", argv[i]);
	}
}

int main(int argc, char *argv[]){

	HandleOptions(argc, argv);

	char name[100];
	FILE *f = fopen("neco.tar", "r");
	if(f == NULL)
		err(1, "ERROR: ");

	fgets(name, 100, f);
	fclose(f);

	printf("%s\n", name);
	return 0;
}
