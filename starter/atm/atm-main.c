/* 
 * The main program for the ATM.
 *
 * You are free to change this as necessary.
 */

#include "atm.h"
#include <stdio.h>
#include <stdlib.h>

static const char prompt[] = "ATM: ";

int main(int argc, char *argv[])
{
    char user_input[1000];
    FILE *atm_file;

    if(argc != 2) {
      printf("Usage: atm <filename>\n");
      exit(62);
    }

    atm_file = fopen(argv[1], "r");
    if(!atm_file) {
      printf("Error opening ATM initialization file\n");
      exit(64);
    }

    ATM *atm = atm_create();

    printf("%s", prompt);
    fflush(stdout);

    while (fgets(user_input, 10000,stdin) != NULL)
    {
        atm_process_command(atm, user_input);
        if(atm->session) {
          printf("ATM (<%s>): ", atm->cur_user);
        } else {
          printf("%s", prompt);
        }
        fflush(stdout);
    }
	return EXIT_SUCCESS;
}
