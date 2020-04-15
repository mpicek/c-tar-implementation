/*
Author Martin Picek

Notes:
	Blocking size is ignored - it is set to 20 by default
	https://www.gnu.org/software/tar/manual/html_node/Blocking-Factor.html
*/

#include <stdio.h>
#include <err.h>
#include <string.h>

//int fseek(FILE *stream, long int offset, int whence)

FILE* listFileAndJump(FILE* f){
	FILE *fcp = f;
	char name[100];
	char size[13];
	long positionThen = ftell(f);
	fgets(name, 100, f);
	long positionNow = ftell(f);
	printf("%s\n", name);

	fseek(f, positionThen-positionNow, SEEK_CUR);
	printf("%ld\n", ftell(f)); 
	fseek(f, 124, SEEK_CUR);
	fgets(size, 12, f);
	size[12] = 0;
	printf("%s\n", size);
	return NULL;
}

void listFiles(char *fileName){
	char name[100];

	FILE *f = fopen(fileName, "r");
	if(f == NULL)
		err(1, "ERROR: ");

	//fgets(name, 100, f);
	
	//while(f != NULL)
		listFileAndJump(f);	

	fclose(f);

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
