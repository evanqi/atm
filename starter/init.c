#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define BLOCK_SIZE 16

int main(int argc, char *argv[]) {
  char *bank, *atm, *key, *iv;
  FILE *bankfile, *atmfile;
  int i;
  if(argc != 2) {
    printf("Usage: init <filename>\n");
    exit(62);
  }

  bank = (char *)calloc((strlen(argv[1]) + 6), sizeof(char));
  strncpy(bank, argv[1], strlen(argv[1]));
  strcat(bank, ".bank");
  atm = (char *)calloc((strlen(argv[1]) + 5), sizeof(char));
  strncpy(atm, argv[1], strlen(argv[1]));
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

  key = (char *)calloc(BLOCK_SIZE, sizeof(char));
  iv = (char *)calloc(BLOCK_SIZE, sizeof(char));

  srand(time(NULL));
  for(i=0; i<BLOCK_SIZE; i++) {
    switch(rand()%4) {
      case 0:
	key[i] = rand()%26 + 'a';
	break;
      case 1:
	key[i] = rand()%26 + 'A';
	break;
      case 2:
	key[i] = rand()%10 + '0';
	break;
      default:
	key[i] = rand()%15 + ' ';
	break;
    }
    switch(rand()%4) {
      case 0:
	iv[i] = rand()%26 + 'a';
	break;
      case 1:
	iv[i] = rand()%26 + 'A';
	break;
      case 2:
	iv[i] = rand()%10 + '0';
	break;
      default:
	iv[i] = rand()%15 + ' ';
	break;
    }
  }

  fputs(key, bankfile);
  fputs(key, atmfile);
  fputs("\n", bankfile);
  fputs("\n", atmfile);
  fputs(iv, bankfile);
  fputs(iv, atmfile);

  printf("Successfully initialized bank state.\n");

  free(bank);
  free(atm);
  free(key);
  free(iv);

  return 0;
}
