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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// NAME_MAX ... max length of component in a path (i.e. filenames, dir names)
// PATH_MAX ... max length of the whole path to a filename (all components)
#include <limits.h>

#define NAME_LENGTH 100 // max length of filename 99, because terminated with \0
#define NAME_LOCATION 0 // for consistency
#define SIZE_LENGTH 12
#define SIZE_LOCATION 124
#define TYPEFLAG_LOCATION 156
#define TYPEFLAG_LENGTH 2
#define MAGIC_LOCATION 257
#define MAGIC_LENGTH 6
#define PROPER_MAGIC "ustar"
#define MULTIPLE 512
#define REGULAR_FILE_FLAG 0
#define DIRECTORY_FILE_FLAG 5

struct files_to_process {
  int number; // number of files to be listed
  int defined;
  int argc;
  char **filenames;
};

void malloc_unsuccessful() { errx(2, "Internal error - malloc unsuccessful"); }

void unexp_EOF_err() {
  warnx("Unexpected EOF in archive");
  errx(2, "Error is not recoverable: exiting now");
}

void more_than_one_action_argument_err() {
  errx(2, "You may not specify more than one \'-Acdtrux\', \'--delete\' or "
  "\'--test-label\' option\nTry \'tar --help\' or \'tar --usage\' "
  "for more information.");
}

void ftell_unsuccessful() { errx(2, "Internal error - ftell unsuccessful"); }

void fseek_unsuccessful() { errx(2, "Internal error - fseek unsuccessful"); }

void fputc_unsuccessful() { errx(2, "Internal error - fputc unsuccessful"); }

void fopen_unsuccessful(char fileName[]) {
    errx(2,
         "%s: Cannot open: No such file or directory\n Error is not "
         "recoverable: exiting now",
         fileName);
}

void magic_err() {
  warnx("This does not look like a tar archive");
  warnx("Skipping to next header");
  errx(2, "Exiting with failure status due to previous errors");
}

void fclose_unsuccessful() { errx(2, "Internal error - fclose unsuccessful"); }

struct action_info{
  char fileName[PATH_MAX]; // name of tar file
  bool file;    // if user specified tar file
  bool action_defined; // if action is even defined
  bool list;    // if -t is defined
  bool extract; // if -x is defined
  bool verbose; // if -v is defined
};

void init_action_info(struct action_info *action){
  action->file = false;
  action->list = false;
  action->extract = false;
  action->verbose = false;
  action->action_defined = false;
}


void check_processed_files(struct files_to_process ftprocess) {
  if (ftprocess.defined) {
    bool found_all = true;
    for (int i = 0; i < ftprocess.number; ++i) {
      if (ftprocess.filenames[i][0] != '\0') {
        found_all = false;
        warnx("%s: Not found in archive", ftprocess.filenames[i]);
      }
    }
    if (!found_all) {
      errx(2, "Exiting with failure status due to previous errors");
    }
  }
}

void init_files_to_process(struct files_to_process *ftprocess, int argc) {
  ftprocess->filenames =
      malloc(argc * sizeof(char *)); // allocate memory for an array of pointers
                                     // pointing at filenames
  if (ftprocess->filenames == NULL) {  // check success of malloc()
    malloc_unsuccessful();
  }
  ftprocess->number = 0;
  ftprocess->defined = 0;
  ftprocess->argc = argc;
}

void deallocate_files_to_process(struct files_to_process *ftprocess) {
  for (int i = 0; i < ftprocess->number; i++) {
    free(ftprocess->filenames[i]); // free memory for each filename
  }
  free(ftprocess->filenames); // free memory for array of pointers for filenames
}

long long oct_to_dec(char *str) {
  long long result = 0;
  int i = 0;
  while (str[i])
    result = 8 * (result + str[i++] - '0');

  return result / 8; }

/// safe seeking in file
void safe_seek(FILE *f, long long offset, int relative_point,long long file_length) {
  if (relative_point == SEEK_CUR) {
    long long current_offset = ftell(f);
    if (current_offset == -1) {
      ftell_unsuccessful();
    }
    if (current_offset + offset > file_length) {
      unexp_EOF_err();
    } else {
      // successful fseek returns 0:
      if (fseek(f, offset, relative_point) != 0) {
        unexp_EOF_err();
      }
    }
  }
}

/// reads string from file and returns to the original position
void read_string(FILE *f, long long location, char str[], long long str_length, long long file_length) {

  long long positionThen = ftell(f); // position at the beginning of the process
  if (positionThen == -1) {
    ftell_unsuccessful();
  }

  safe_seek(f, location, SEEK_CUR, file_length); // seek to location

  if (fgets(str, str_length, f) == NULL) // read string
    unexp_EOF_err();

  long long positionNow = ftell(f); // get current position
  if (positionNow == -1) {
    ftell_unsuccessful();
  }

  safe_seek(f, (positionThen - positionNow), SEEK_CUR,
            file_length); // seek back
}

// saves name of the current file to name[]
void get_name_of_file(FILE *f, char name[], long long file_length){
  read_string(f, NAME_LOCATION, name, NAME_LENGTH, file_length); // read NAME
}

void get_flag_of_file(FILE *f, char typeflag[], long long file_length){
  read_string(f, TYPEFLAG_LOCATION, typeflag, 
        TYPEFLAG_LENGTH, file_length); // read TYPEFLAG
} 

void get_size_of_file(FILE *f, char sizeStr[], long long file_length){
  read_string(f, SIZE_LOCATION, sizeStr, SIZE_LENGTH, file_length); // read SIZE
}

void check_magic(FILE *f, long long file_length){
  char magic[6];
  read_string(f, MAGIC_LOCATION, magic, MAGIC_LENGTH, file_length);
  if(strcmp(magic, PROPER_MAGIC) != 0){
    magic_err();
  }
}

void print_file(char name[], struct files_to_process *ftprocess, bool delete_from_files_to_process) {
  if (ftprocess->defined) {
    for (int i = 0; i < ftprocess->number; ++i) {
      if (strcmp(ftprocess->filenames[i], name) == 0) {
        printf("%s\n", name);
        fflush(stdout);
        if(delete_from_files_to_process)
          ftprocess->filenames[i][0] = '\0'; // it means the file is printed
        break;
      }
    }
  } else {
    printf("%s\n", name);
    fflush(stdout);
  }
}

// returns size of the WHOLE tar file
long long get_tar_file_length(FILE *f) {
  if (fseek(f, 0, SEEK_END) != 0) {
    fseek_unsuccessful();
  }
  long long file_length = ftell(f);
  if (file_length == -1) {
    ftell_unsuccessful();
  }
  if (fseek(f, 0, SEEK_SET) != 0) {
    fseek_unsuccessful();
  }
  return file_length;
}

void create_and_extract_to_file(FILE *f, char name[], long long size, 
              long long file_length, struct files_to_process ftprocess){

  long long positionThen = ftell(f); // position at the beginning of the process
  if (positionThen == -1) {
    ftell_unsuccessful();
  }


  bool process_file = false;

  if(ftprocess.defined){
    for (int i = 0; i < ftprocess.number; ++i) {
      if (!strcmp(ftprocess.filenames[i], name)) {
        process_file = true;
        ftprocess.filenames[i][0] = '\0'; // it means the file is processed
        break;
      }
    }
  }
  else{
    process_file = true;
  }

  if(process_file){
    //create new file
    FILE *extracted_file;
    extracted_file = fopen(name, "w");
    if(extracted_file == NULL)
      fopen_unsuccessful(name);

    safe_seek(f, MULTIPLE, SEEK_CUR, file_length); // skip header

    for(int i = 0; i < size; i++){
      int one_byte;
      one_byte = fgetc(f);
      if(one_byte == EOF)
        unexp_EOF_err();
      int is_error = fputc((char)one_byte, extracted_file);
      if(is_error == EOF)
        fputc_unsuccessful();
    }

    if (fclose(extracted_file) == EOF) {
      fclose_unsuccessful();
    }

    long long positionNow = ftell(f); // get current position
    if (positionNow == -1) {
      ftell_unsuccessful();
    }
    safe_seek(f, (positionThen - positionNow), SEEK_CUR,
            file_length); // seek back
  }
}

/// returns 1 when reads file, 0 when there is no file
bool process_one_file_and_jump(FILE *f, struct action_info action, struct files_to_process ftprocess, long long file_length){

  char name[NAME_LENGTH];
  char sizeStr[SIZE_LENGTH];
  char typeflag[TYPEFLAG_LENGTH]; // typeflag and end of string (\0)

  get_name_of_file(f, name, file_length);
  get_flag_of_file(f, typeflag, file_length);

  if (name[0] == 0)
    return false; // when this is an empty block at the end of the .tar file

  //read_string(f, SIZE_LOCATION, sizeStr, SIZE_LENGTH, file_length); // read SIZE
  get_size_of_file(f, sizeStr, file_length);
  long long size = oct_to_dec(sizeStr);

  if(action.list){
  // when it is not regular file but there is file (name[0] != 0)
    if ((typeflag[0] - '0' != REGULAR_FILE_FLAG) && (name[0] != 0)) {
      errx(2, "Unsupported header type: %d", typeflag[0]);
    }
    print_file(name, &ftprocess, true);
  }
  else if(action.extract){
    // when it is not regular file but there is file (name[0] != 0)
    if ((typeflag[0] - '0' != REGULAR_FILE_FLAG && 
         typeflag[0] - '0' != DIRECTORY_FILE_FLAG) && (name[0] != 0)) {
      errx(2, "Unsupported header type: %d", typeflag[0]);
    }
    if(action.verbose)
      print_file(name, &ftprocess, false);
    if(strcmp(name, "./") != 0)
      create_and_extract_to_file(f, name, size, file_length, ftprocess);
  }

  long long padding = 0;
  if (size % MULTIPLE)
    padding = 1; // file ends somewhere in the middle of a block
  // 1 stands for header (512B = MULTIPLE) and the rest makes the record aligned
  // to 512B
  long long jump = MULTIPLE * (1 + size / MULTIPLE + padding);
  safe_seek(f, jump, SEEK_CUR,
            file_length); // jump to the next record (if possible)
  // if not possible, than don't jump (implemented in seek) and in next
  // iteration of this function the end of file will be detected

  return true;
}

void process_action(struct action_info action, struct files_to_process ftprocess){
  if (action.fileName[0] == 0)
    errx(2, "tar: Refusing to read archive contents from terminal (missing -f "
            "option?)\ntar: Error is not recoverable: exiting now");
  FILE *f = fopen(action.fileName, "r");
  if (f == NULL)
    fopen_unsuccessful(action.fileName);
  
  long long file_length = get_tar_file_length(f);

  check_magic(f, file_length);

  bool fileRead = true;
  while (fileRead) {
    //PROCESS FILE
    fileRead = process_one_file_and_jump(f, action, ftprocess, file_length);
    long long current_offset = ftell(f);

    if (current_offset == -1) {
      ftell_unsuccessful();
    } else if (current_offset ==
               file_length) { // when we are at the end of the file, it
                              // means that two blocks are missing
      deallocate_files_to_process(&ftprocess);
      exit(0);
    }
  }

  check_processed_files(ftprocess);

  long long current_offset = ftell(f);
  if (current_offset == -1) {
    ftell_unsuccessful();
  }

  // if there is only one block at the end. When there are two, it is ok
  if (current_offset + MULTIPLE <= file_length &&
      current_offset + 2 * MULTIPLE > file_length) {
    warnx("A lone zero block at %lld", (current_offset + MULTIPLE) / MULTIPLE);
  }

  if (fclose(f) == EOF) {
    fclose_unsuccessful();
  }

}

void process_tar_archive(struct action_info action, struct files_to_process ftprocess){
  if(action.list && action.extract){
    more_than_one_action_argument_err();
  } else{
    if(action.action_defined){
      if(action.file){
        //TODO action
        process_action(action, ftprocess);
        fflush(stdout);
      } else{
        errx(2, "tar: Refusing to read archive contents from terminal (missing -f "
                "option?)\ntar: Error is not recoverable: exiting now");
      }
    } else{
      errx(2, "tar: You must specify one of the '-Acdtrux', '--delete' or "
              "'--test-label' options\nTry 'tar --help' or 'tar --usage' for "
              "more information.");
    }
  }
}

void strcpy_unsuccessful() {
  errx(2, "Internal error - can't use strcpy - destination is too small");
}

void safe_strcpy(char *dest, long long dest_len, char *source, long long source_len) {
  if (dest_len >= source_len)
    strcpy(dest, source);
  else
    strcpy_unsuccessful();
}

void program(int argc, char *argv[]) {

  struct action_info action;
  init_action_info(&action);

  // here are stored explicitely defined files by user
  struct files_to_process ftprocess;
  init_files_to_process(&ftprocess, argc);

  if (argc == 1)
    errx(2, "Tar needs arguments");

  // iterate over argv and find options so that they can be later processed
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-f")) { // FILENAME
      action.file = true;
      i++; // next argument should be fileName
      if (i == argc){
        errx(2, "tar: option requires an argument -- 'f'\nTry 'tar --help' or "
                "'tar --usage' for more information.");
      }

      safe_strcpy(action.fileName, sizeof(action.fileName), argv[i], sizeof(argv[i]));

    } else if (!strcmp(argv[i], "-t")) { // LIST
      action.list = true;
      action.action_defined = true;
    } else if (!strcmp(argv[i], "-x")){ // EXTRACT
      action.extract = true;
      action.action_defined = true;
    } else if (!strcmp(argv[i], "-v")){ // VERBOSE
      action.verbose = true;
      action.action_defined = true;
    } else {      // OTHER OPTION OR FILENAME
      if (action.list || action.extract) { // if list or extract defined
                  //than the next option is the file to be listed/extracted
        ftprocess.defined = 1;
        ftprocess.filenames[ftprocess.number] =
            malloc(PATH_MAX * sizeof(char)); // allocate memory
        if (ftprocess.filenames[ftprocess.number] == NULL) {
          malloc_unsuccessful();
        }
        safe_strcpy(ftprocess.filenames[ftprocess.number],
                    sizeof(ftprocess.filenames[ftprocess.number]), argv[i],
                    sizeof(argv[i]));
        ftprocess.number++;
      } else{
        // commented to pass the tests:
        // errx(2, "Unknown option.");
        exit(2); // UNKNOWN OPTION
        }
    }
  }
  
  process_tar_archive(action, ftprocess);

  deallocate_files_to_process(&ftprocess);
}

int main(int argc, char *argv[]) {
  program(argc, argv);
  return 0;
}
