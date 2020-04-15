/*
Author Martin Picek

Notes:
	Blocking size is ignored - it is set to 20 by default
	https://www.gnu.org/software/tar/manual/html_node/Blocking-Factor.html
	numbers in tar header are octal numbers!! (8^n)
	everything padded to the multiple of 512 (except the end - blocksize)
*/

#include <stdio.h>
#include <err.h>
#include <string.h>

// NAME_MAX ... max length of component in a path (i.e. filenames, dir names)
// PATH_MAX ... max length of the whole path to a filename (all components)
#include <limits.h> 

#define NAME_LENGTH 100 //max length of filename 99, because terminated with \0
#define SIZE_LENGTH 12
#define SIZE_LOCATION 124
#define MULTIPLE 512

int octToDec(char* str){
	int result = 0;
	int i = 0;
	while(str[i])
		result = 8*(result+str[i++]-'0');

	return result/8;
}


int listFileAndJump(FILE* f){
	/* returns 1 when reads file, 0 when there is no file*/
	char name[NAME_LENGTH];
	char sizeStr[SIZE_LENGTH];
	long positionThen = ftell(f);

	//NAME
	fgets(name, NAME_LENGTH, f);
	long positionNow = ftell(f);
	fseek(f, positionThen-positionNow, SEEK_CUR);
	fseek(f, SIZE_LOCATION, SEEK_CUR);
	if(name[0] == 0) return 0;
	printf("%s\n", name);

	//SIZE
	fgets(sizeStr, SIZE_LENGTH, f);
	fseek(f,-SIZE_LOCATION - SIZE_LENGTH + 1, SEEK_CUR);

	int size = octToDec(sizeStr);
	//printf("%d\n", size);

	int padding = 0;
	if(size % MULTIPLE) padding = 1;
	int jump = MULTIPLE*(1 + size/MULTIPLE + padding);
	fseek(f, jump, SEEK_CUR);

	return 1;
} 

void listFiles(char *fileName){

	FILE *f = fopen(fileName, "r");

	printf("%s\n", fileName);
	if(f == NULL)
		err(1, "ERROR: ");

	int fileRead = 1;
	while(fileRead)
		fileRead = listFileAndJump(f);	

	fclose(f);

}

void HandleOptions(int argc, char *argv[]){
	char fileName[PATH_MAX];
	int list = 0;

	if(argc == 1)
		errx(1, "Tar needs arguments");


	for(int i = 1; i < argc; i++){
		if(!strcmp(argv[i], "-f")){                      //FILENAME 
			i++; //next argument should be fileName 
			if(i == argc)
				errx(1, "Missing filename");

			strcpy(fileName, argv[i]);
		//	printf("filename: %s\n", fileName);
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
