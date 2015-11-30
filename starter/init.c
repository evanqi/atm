#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  char *bank, *atm;
  FILE *bankfile, *atmfile;
  if(argc != 2) {
    printf("Usage: init <filename>\n");
    exit(62);
  }

  bank = (char *)malloc((strlen(argv[1]) + 5)*sizeof(char));
  strcpy(bank, argv[1]);
  strcat(bank, ".bank");
  atm = (char *)malloc((strlen(argv[1]) + 4)*sizeof(char));
  strcpy(atm, argv[1]);
  strcat(atm, ".atm");

  if(access(bank, F_OK) != -1 || access(atm, F_OK) != -1) {
    printf("Error: one of the files already exists\n");
    exit(63);
  }

  bankfile = fopen(bank, "w+");
  atmfile = fopen(atm, "w+");

  if(!bankfile || !atmfile) {
    printf("Error creating initialization files\n");
    exit(64);
  }

  printf("Successfully initialized bank state.\n");

  free(bank);
  free(atm);

  return 0;
}
