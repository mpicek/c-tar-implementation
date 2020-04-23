/*
Author Martin Picek

Notes:
	Blocking size is ignored - it is set to 20 by default
	https://www.gnu.org/software/tar/manual/html_node/Blocking-Factor.html
	end should be ended with two zero blocks - if not, it is silently ignored,
		but if only one is missing, error occurs
	numbers in tar header are octal numbers!! (8^n)
	everything padded to the multiple of 512 (except the end - blocksize)
*/

#include <stdio.h>
#include <err.h>
#include <string.h>
#include <stdlib.h>

// NAME_MAX ... max length of component in a path (i.e. filenames, dir names)
// PATH_MAX ... max length of the whole path to a filename (all components)
#include <limits.h> 

#define NAME_LENGTH 100 //max length of filename 99, because terminated with \0
#define SIZE_LENGTH 12
#define SIZE_LOCATION 124
#define TYPEFLAG_LOCATION 156
#define MULTIPLE 512

struct files_to_print{
    int number;
    int defined;
    int argc;
    int deleted;
    char **filenames;
};

int octToDec(char* str){
	int result = 0;
	int i = 0;
	while(str[i])
		result = 8*(result+str[i++]-'0');

	return result/8;
}


int listFileAndJump(FILE* f, struct files_to_print ftprint){
	/* returns 1 when reads file, 0 when there is no file*/
	char name[NAME_LENGTH];
	char sizeStr[SIZE_LENGTH];
	char typeflag;
	long positionThen = ftell(f);

	//NAME
	if(fgets(name, NAME_LENGTH, f) == NULL) //NAME is on offset 0
        err(2, "Unexpected End Of File\n");
	long positionNow = ftell(f);
	if(fseek(f, positionThen-positionNow, SEEK_CUR) != 0){ //back
	    err(2, "Unexpected End Of File\n");
	}

    if(fseek(f, TYPEFLAG_LOCATION, SEEK_CUR) != 0){ //back
        err(2, "Unexpected End Of File\n");
    }
    if((typeflag = fgetc(f)) == EOF)
        err(2, "Unexpected End Of File\n");

    if(fseek(f, -TYPEFLAG_LOCATION-1, SEEK_CUR) != 0){ //back
        err(2, "Unexpected End Of File\n");
    }

    //printf("%d\n", typeflag-'0');
    if(typeflag-'0' && name[0] != 0){ //typeflag for regular file is 0
        errx(2, "Unsupported header type: %d", typeflag);
    }

	if(name[0] == 0) return 0;
	if(ftprint.defined){
        for (int i = 0; i < ftprint.number; ++i) {
            //printf("VIDIM: %s ... HLEDAM: %s \n", name, ftprint.filenames[i]);
            if(!strcmp(ftprint.filenames[i], name)){
                printf("%s\n", name);
                ftprint.filenames[i][0] = '\0';
                ftprint.deleted++;
                break;
            }
        }
	}
	else{
        printf("%s\n", name);
	}



	//SIZE
    if(fseek(f, SIZE_LOCATION, SEEK_CUR) != 0){
        err(2, "Unexpected End Of File\n");
    }
    if(fgets(sizeStr, SIZE_LENGTH, f) == NULL)
        err(2, "Unexpected End Of File\n");
    if(fseek(f,-SIZE_LOCATION - SIZE_LENGTH + 1, SEEK_CUR != 0)){ //back
        err(2, "Unexpected End Of File\n");
    }

	int size = octToDec(sizeStr);
	//printf("%d\n", size);

	int padding = 0;
	if(size % MULTIPLE) padding = 1;
	int jump = MULTIPLE*(1 + size/MULTIPLE + padding);
    if(fseek(f, jump, SEEK_CUR) != 0){
        err(2, "Unexpected End Of File\n");
    }

	return 1;
} 

void listFiles(char *fileName, struct files_to_print ftprint){

	if(fileName[0] == 0)
		errx(2, "tar: Refusing to read archive contents from terminal (missing -f option?)\ntar: Error is not recoverable: exiting now");

	FILE *f = fopen(fileName, "r");

	//printf("%s\n", fileName);
	if(f == NULL)
		err(2, "%s: Cannot open: No such file or directory\n Error is not recoverable: exiting now", fileName);

	int fileRead = 1;
	while(fileRead)
		fileRead = listFileAndJump(f, ftprint);

	fclose(f);

}

void HandleOptions(int argc, char *argv[]){
	char fileName[PATH_MAX];
	int list = 0;
	int file = 0;
	int something = 0;
	char last_option[30];
	last_option[0] = '\0';
	//char files_to_print[argc][PATH_MAX];//too much memory given - optimize later (TODO)
	//TODO uvolnit pamet!!
    struct files_to_print ftprint;
    ftprint.filenames = malloc(argc*sizeof(char*)); //allocate memory for an array of pointers pointing at filenames
    ftprint.number = 0;
    ftprint.deleted = 0;
    ftprint.defined = 0;
    ftprint.argc = argc;

	if(argc == 1)
		errx(2, "Tar needs arguments");


	for(int i = 1; i < argc; i++){
		if(!strcmp(argv[i], "-f")){                      //FILENAME 
			strcpy(last_option, argv[i]);
			file=1;
			i++; //next argument should be fileName 
			if(i == argc)
				errx(2, "tar: option requires an argument -- 'f'\nTry 'tar --help' or 'tar --usage' for more information.");

			strcpy(fileName, argv[i]);
		//	printf("filename: %s\n", fileName);
		}

		else if(!strcmp(argv[i], "-t")){                   //LIST
			strcpy(last_option, argv[i]);
			list = 1;
			something = 1;
		}
		else{
			//if(!strcmp(last_option, "-t")){
			if(list){
			    ftprint.defined = 1;
			    ftprint.filenames[ftprint.number] = malloc(PATH_MAX*sizeof(char)); //allocate memory
			    strcpy(ftprint.filenames[ftprint.number], argv[i]);
			    ftprint.number++;
			}
			else errx(2, "Unknown option: %s", argv[i]); // UNKNOWN OPTION
		}
	}
	if(list){
		if(file) {
            listFiles(fileName, ftprint);
            fflush(stdout);
            if(ftprint.defined){ //kontrola, aby bylo vse vypsano, potom jeste dat do novy funkce
                int found_all = 1;
                for (int i = 0; i < ftprint.number; ++i) {
                    if(ftprint.filenames[i][0] != '\0'){
                        found_all = 0;
                        warnx("%s: Not found in archive", ftprint.filenames[i]);
                    }
                }
                if(!found_all){
                    errx(2, "Exiting with failure status due to previous errors");
                }
            }
        }
		else {
            errx(2, "tar: Refusing to read archive contents from terminal (missing -f option?)\ntar: Error is not recoverable: exiting now");
        }
	}
	if(!something)
		errx(2, "tar: You must specify one of the '-Acdtrux', '--delete' or '--test-label' options\nTry 'tar --help' or 'tar --usage' for more information.");
}

int main(int argc, char *argv[]){

	HandleOptions(argc, argv);
	return 0;
}
