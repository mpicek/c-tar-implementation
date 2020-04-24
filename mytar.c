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
#define NAME_LOCATION 0 //for consistency
#define SIZE_LENGTH 12
#define SIZE_LOCATION 124
#define TYPEFLAG_LOCATION 156
#define TYPEFLAG_LENGTH 1
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

void unexp_EOF_err(){
    warnx("Unexpected EOF in archive");
    errx(2, "Error is not recoverable: exiting now");
}

void seek(FILE* f, int offset, int relative_point, int file_length){
    if(relative_point == SEEK_CUR){
        if(ftell(f) + offset > file_length){
            unexp_EOF_err();
        }
        else{
            if(fseek(f, offset, relative_point) != 0){ //back
                unexp_EOF_err();
            }
        }
    }
}

//reads string from file and returns to the original position
void read_string(FILE* f, int location, char str[], int str_length, int file_length){

    long positionThen = ftell(f); //position at the beginning of the process

    seek(f, location, SEEK_CUR, file_length); //seek to location

    if(fgets(str, str_length, f) == NULL) //read string
        unexp_EOF_err();

    long positionNow = ftell(f); //get current position

    seek(f, positionThen-positionNow, SEEK_CUR, file_length); //seek back

}

int listFileAndJump(FILE* f, struct files_to_print ftprint, int file_length){
	/* returns 1 when reads file, 0 when there is no file*/

	char name[NAME_LENGTH];
	char sizeStr[SIZE_LENGTH];
	char typeflag[TYPEFLAG_LENGTH]; //typeflag and and of string (\0)

	read_string(f, NAME_LOCATION, name, NAME_LENGTH, file_length); //read NAME
    read_string(f, TYPEFLAG_LOCATION, typeflag, 2, file_length);   //read TYPEFLAG


    if(typeflag[0]-'0' && name[0] != 0){ //typeflag for regular file is 0
        errx(2, "Unsupported header type: %d", typeflag[0]);
    }

	if(name[0] == 0) return 0;
	if(ftprint.defined){
        for (int i = 0; i < ftprint.number; ++i) {
            //printf("VIDIM: %s ... HLEDAM: %s \n", name, ftprint.filenames[i]);
            if(!strcmp(ftprint.filenames[i], name)){
                printf("%s\n", name);
                fflush(stdout);
                ftprint.filenames[i][0] = '\0';
                ftprint.deleted++;
                break;
            }
        }
	}
	else{
        printf("%s\n", name);
        fflush(stdout);
	}


    read_string(f, SIZE_LOCATION, sizeStr, SIZE_LENGTH, file_length); //read SIZE

	int size = octToDec(sizeStr);

	int padding = 0;
	if(size % MULTIPLE) padding = 1;
	int jump = MULTIPLE*(1 + size/MULTIPLE + padding);
    seek(f, jump, SEEK_CUR, file_length);

	return 1;
} 

void listFiles(char *fileName, struct files_to_print ftprint){

	if(fileName[0] == 0)
		errx(2, "tar: Refusing to read archive contents from terminal (missing -f option?)\ntar: Error is not recoverable: exiting now");

	FILE *f = fopen(fileName, "r");
    fseek(f, 0, SEEK_END);
    int file_length = ftell(f);
    fseek(f, 0, SEEK_SET);

	if(f == NULL)
		errx(2, "%s: Cannot open: No such file or directory\n Error is not recoverable: exiting now", fileName);

	int fileRead = 1;
	while(fileRead){
		fileRead = listFileAndJump(f, ftprint, file_length);
		if(ftell(f) == file_length){
		    exit(0); //when two blocks are missing
            fflush(stdout);
		}
    }

    if(ftell(f) + MULTIPLE <= file_length){
        if(ftell(f) + 2*MULTIPLE <= file_length){

        }
        else{
            warnx("A lone zero block at 4");
        }
    }
	fclose(f);

}

void init_files_to_print(struct files_to_print *ftprint, int argc){
    ftprint->filenames = malloc(argc*sizeof(char*)); //allocate memory for an array of pointers pointing at filenames
    ftprint->number = 0;
    ftprint->deleted = 0;
    ftprint->defined = 0;
    ftprint->argc = argc;
}

void HandleOptions(int argc, char *argv[]){
	char fileName[PATH_MAX];
	int list = 0;
	int file = 0;
	int something = 0;
	char last_option[30];
	last_option[0] = '\0';
	//char files_to_print[argc][PATH_MAX];//too much memory given - optimize later (TODO)

    struct files_to_print ftprint;
    init_files_to_print(&ftprint, argc);

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
			else exit(2);//errx(2, "Unknown option: %s", argv[i]); // UNKNOWN OPTION
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
