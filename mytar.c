/*
Author Martin Picek

Notes:
        Blocking size is ignored - it is set to 20 by default
        https://www.gnu.org/software/tar/manual/html_node/Blocking-Factor.html
        end should be ended with two zero blocks - if not, it is silently
ignored, but if only one is missing, error occurs numbers in tar header are
octal numbers!! (8^n) everything padded to the multiple of 512 (except the end -
blocksize)
*/

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// NAME_MAX ... max length of component in a path (i.e. filenames, dir names)
// PATH_MAX ... max length of the whole path to a filename (all components)
#include <limits.h>

#define NAME_LENGTH 100 // max length of filename 99, because terminated with \0
#define NAME_LOCATION 0 // for consistency
#define SIZE_LENGTH 12
#define SIZE_LOCATION 124
#define TYPEFLAG_LOCATION 156
#define TYPEFLAG_LENGTH 1
#define MULTIPLE 512
#define REGULAR_FILE_FLAG 0

struct files_to_print {
  int number; //number of files to be listed
  int defined;
  int argc;
  char **filenames;
};

void malloc_unsuccessful(){
  errx(2, "Internal error - malloc unsuccessful");
}

void unexp_EOF_err() {
  warnx("Unexpected EOF in archive");
  errx(2, "Error is not recoverable: exiting now");
}

void ftell_unsuccessful(){
  errx(2, "Internal error - ftell unsuccessful");
}

void fseek_unsuccessful(){
  errx(2, "Internal error - fseek unsuccessful");
}

void fclose_unsuccessful(){
  errx(2, "Internal error - fclose unsuccessful");
}

void check_printed_files(struct files_to_print ftprint) {
  if (ftprint.defined) {
    bool found_all = true;
    for (int i = 0; i < ftprint.number; ++i) {
      if (ftprint.filenames[i][0] != '\0') {
        found_all = false;
        warnx("%s: Not found in archive", ftprint.filenames[i]);
      }
    }
    if (!found_all) {
      errx(2, "Exiting with failure status due to previous errors");
    }
  }
}

void init_files_to_print(struct files_to_print *ftprint, int argc) {
  ftprint->filenames =
      malloc(argc * sizeof(char *)); // allocate memory for an array of pointers
                                     // pointing at filenames
  if(ftprint->filenames == NULL){ //check success of malloc()
    malloc_unsuccessful();
  }
  ftprint->number = 0;
  ftprint->defined = 0;
  ftprint->argc = argc;
}

void deallocate_files_to_print(struct files_to_print *ftprint) {
  for(int i = 0; i < ftprint->number; i++){
    free(ftprint->filenames[i]); //free memory for each filename
  }
  free(ftprint->filenames); //free memory for array of pointers for filenames
}

long long oct_to_dec(char *str) {
  long long result = 0;
  int i = 0;
  while (str[i])
    result = 8 * (result + str[i++] - '0');

  return result / 8;
}

///safe seeking in file
void safe_seek(FILE *f, long long offset, int relative_point, long long file_length) {
  if (relative_point == SEEK_CUR) {
    long long current_offset = ftell(f);
    if (current_offset == -1){
      ftell_unsuccessful();
    }
    if (current_offset + offset > file_length) {
      unexp_EOF_err();
    } else {
      //successful fseek returns 0:
      if (fseek(f, offset, relative_point) != 0) { 
        unexp_EOF_err();
      }
    }
  }
}

/// reads string from file and returns to the original position
void read_string(FILE *f, long long location, char str[], long long str_length,
                 long long file_length) {

  long long positionThen = ftell(f); // position at the beginning of the process
  if(positionThen == -1){
    ftell_unsuccessful();
  }

  safe_seek(f, location, SEEK_CUR, file_length); // seek to location

  if (fgets(str, str_length, f) == NULL) // read string
    unexp_EOF_err();

  long long positionNow = ftell(f); // get current position
  if(positionNow == -1){
    ftell_unsuccessful();
  }

  safe_seek(f, (positionThen - positionNow), SEEK_CUR, file_length); // seek back
}

void print_file(char name[], struct files_to_print ftprint) {
  if (ftprint.defined) {
    for (int i = 0; i < ftprint.number; ++i) {
      if (!strcmp(ftprint.filenames[i], name)) {
        printf("%s\n", name);
        fflush(stdout);
        ftprint.filenames[i][0] = '\0'; // it means the file is printed
        break;
      }
    }
  } else {
    printf("%s\n", name);
    fflush(stdout);
  }
}

/// returns 1 when reads file, 0 when there is no file
bool list_file_and_jump(FILE *f, struct files_to_print ftprint,
                       long long file_length) {
  char name[NAME_LENGTH];
  char sizeStr[SIZE_LENGTH];
  char typeflag[TYPEFLAG_LENGTH]; // typeflag and and of string (\0)

  read_string(f, NAME_LOCATION, name, NAME_LENGTH, file_length); // read NAME
  read_string(f, TYPEFLAG_LOCATION, typeflag, 2, file_length); // read TYPEFLAG

  // when it is not regular file but there is file (name[0] != 0)
  if ((typeflag[0] - '0' != REGULAR_FILE_FLAG) && (name[0] != 0)) {
    errx(2, "Unsupported header type: %d", typeflag[0]);
  }

  if (name[0] == 0)
    return false; // when this is an empty block at the end of the .tar file

  print_file(name, ftprint);

  read_string(f, SIZE_LOCATION, sizeStr, SIZE_LENGTH, file_length); // read SIZE

  long long size = oct_to_dec(sizeStr);

  long long padding = 0;
  if (size % MULTIPLE)
    padding = 1; // file ends somewhere in the middle of a block
  // 1 stands for header (512B = MULTIPLE) and the rest makes the record aligned
  // to 512B
  long long jump = MULTIPLE * (1 + size / MULTIPLE + padding);
  safe_seek(f, jump, SEEK_CUR, file_length); // jump to the next record (if possible)
  // if not possible, than don't jump (implemented in seek) and in next
  // iteration of this function the end of file will be detected

  return true;
}

long long get_file_length(FILE *f) {
  if(fseek(f, 0, SEEK_END) != 0){
    fseek_unsuccessful();
  }
  long long file_length = ftell(f);
  if(file_length == -1){
    ftell_unsuccessful();
  }
  if(fseek(f, 0, SEEK_SET) != 0){
    fseek_unsuccessful();
  }
  return file_length;
}

void list_files(char *fileName, struct files_to_print ftprint) {

  if (fileName[0] == 0)
    errx(2, "tar: Refusing to read archive contents from terminal (missing -f "
            "option?)\ntar: Error is not recoverable: exiting now");

  FILE *f = fopen(fileName, "r");
  if (f == NULL)
    errx(2,
         "%s: Cannot open: No such file or directory\n Error is not "
         "recoverable: exiting now",
         fileName);
  long long file_length = get_file_length(f);


  bool fileRead = true;
  while (fileRead) {
    fileRead = list_file_and_jump(f, ftprint, file_length);
    long long current_offset = ftell(f);
    
    if (current_offset == -1){
      ftell_unsuccessful();
    }
    else if (current_offset == file_length) { // when we are at the end of the file, it
                                   // means that two blocks are missing
      deallocate_files_to_print(&ftprint);
      exit(0);
    }
  }
  long long current_offset = ftell(f);
  if (current_offset == -1){
    ftell_unsuccessful();
  }

  // if there is only one block at the end. When there are two, it is ok
  if (current_offset + MULTIPLE <= file_length &&
      current_offset + 2 * MULTIPLE > file_length) {
    warnx("A lone zero block at %lld", (current_offset+MULTIPLE)/MULTIPLE);
  }

  if(fclose(f) == EOF){
    fclose_unsuccessful();
  }
}

void option_t(int file, char fileName[], struct files_to_print ftprint) {
  if (file) {                      // if user defined .tar file
    list_files(fileName, ftprint); // list the files
    fflush(stdout);
    check_printed_files(
        ftprint); // if all files specified (if specified) have been printed
  } else {
    errx(2, "tar: Refusing to read archive contents from terminal (missing -f "
            "option?)\ntar: Error is not recoverable: exiting now");
  }
}

void strcpy_unsuccessful(){
  errx(2, "Internal error - can't use strcpy - destination is too small");
}

void safe_strcpy(char* dest, long long dest_len, char* source, long long source_len){
  if(dest_len >= source_len)
    strcpy(dest, source);
  else
    strcpy_unsuccessful();
}

void handle_options(int argc, char *argv[]) {
  char fileName[PATH_MAX];
  bool list = false; // indicates whether -t option was used
  bool file = false; // indicates whether a user specified the file to operate on
  bool action_defined = false; // whether there is any action defined

  // when -t is defined and user wants to list only certain files, here they are
  // stored
  struct files_to_print ftprint;
  init_files_to_print(&ftprint, argc);

  if (argc == 1)
    errx(2, "Tar needs arguments");

  // iterate over argv and find options so that they can be later processed
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-f")) { // FILENAME
      file = true;
      i++; // next argument should be fileName
      if (i == argc)
        errx(2, "tar: option requires an argument -- 'f'\nTry 'tar --help' or "
                "'tar --usage' for more information.");

      safe_strcpy(fileName, sizeof(fileName), argv[i], sizeof(argv[i]));

    } else if (!strcmp(argv[i], "-t")) { // LIST
      list = true;
      action_defined = true;
    } else {      // OTHER OPTION OR FILENAME
      if (list) { // if list is defined, than the next option is the file to be
                  // listed
        ftprint.defined = 1;
        ftprint.filenames[ftprint.number] =
            malloc(PATH_MAX * sizeof(char)); // allocate memory
        if(ftprint.filenames[ftprint.number] == NULL){
          malloc_unsuccessful();
        }
        safe_strcpy(ftprint.filenames[ftprint.number], sizeof(ftprint.filenames[ftprint.number]), argv[i], sizeof(argv[i]));
        ftprint.number++;
      } else
        //errx(2, "Unknown option.");
        exit(2); // UNKNOWN OPTION
    }
  }

  if (list) {
    option_t(file, fileName, ftprint);
  }
  if (!action_defined) {
    errx(2, "tar: You must specify one of the '-Acdtrux', '--delete' or "
            "'--test-label' options\nTry 'tar --help' or 'tar --usage' for "
            "more information.");
  }

  deallocate_files_to_print(&ftprint);

}

int main(int argc, char *argv[]) {
  handle_options(argc, argv);
  return 0;
}
