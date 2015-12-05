/* 
 * The main program for the ATM.
 *
 * You are free to change this as necessary.
 */

#include "atm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util/hash_table.h"
#include "util/list.h"

static const char BLOCK_SIZE = 16;
static const char prompt[] = "ATM: ";

int main(int argc, char *argv[])
{
    char user_input[1000];
    FILE *atm_file;

    if(argc != 2) {
      printf("Usage: atm <filename>\n");
      exit(62);
    }

    if(strncmp(argv[1]+strlen(argv[1])-4, ".atm", 4) != 0) {
      printf("Error opening ATM initialization file\n");
      exit(64);
    }

    atm_file = fopen(argv[1], "r");
    if(!atm_file) {
      printf("Error opening ATM initialization file\n");
      exit(64);
    }

    ATM *atm = atm_create();
    fread(atm->key, sizeof(char), BLOCK_SIZE, atm_file);
    fread(atm->iv, sizeof(char), BLOCK_SIZE, atm_file);

    printf("%s", prompt);
    fflush(stdout);

    while (fgets(user_input, 10000,stdin) != NULL)
    {
        if(strlen(user_input) > 0 && strcmp(user_input, "\n") == 0) continue;
        atm_process_command(atm, user_input);
        if(atm->session) {
          printf("ATM (<%s>): ", atm->cur_user);
        } else {
          printf("%s", prompt);
        }
        fflush(stdout);
    }
    atm_free(atm);

    return EXIT_SUCCESS;
}
