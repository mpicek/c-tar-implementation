#include <stdio.h>


int main(int argc, char *argv[]){

	FILE *f;
	if((f= fopen("neco.tar", "r")) == NULL) printf("chyba\n");
	char name[100];
	
	for(int i; i < 100; i++) name[i] = 0;

	//fscanf(f, "%[^\0]", name);
	fgets(name, 100, f);
	fclose(f);

	printf("%s\n", name);
	return 0;
}
