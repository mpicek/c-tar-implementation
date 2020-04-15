#include <stdio.h>
#include <err.h>
#include <string.h>

void listFiles(char *fileName){
	char name[100];
	int i = 0;

	FILE *f = fopen(fileName, "r");
	if(f == NULL)
		err(1, "ERROR: ");

	fgets(name, 100, f);
	fclose(f);

	printf("%s\n", name);
}

void HandleOptions(int argc, char *argv[]){
	if(argc == 1)
		errx(1, "Tar needs arguments");

	char fileName[100];
	int list = 0;

	for(int i = 1; i < argc; i++){

		if(!strcmp(argv[i], "-f")){                      //FILENAME 
			i++; //next argument should be fileName 
			if(i == argc){
				errx(1, "Missing filename");
			}
			strcpy(fileName, argv[i]);
			printf("filename: %s\n", fileName);
		}

		else if(!strcmp(argv[i], "-t"))                   //LIST
			list = 1;
		else
			errx(1, "Unknown option: %s", argv[i]);
	}
	if(list)
		listFiles(fileName);
}

int main(int argc, char *argv[]){

	HandleOptions(argc, argv);

	return 0;
}
