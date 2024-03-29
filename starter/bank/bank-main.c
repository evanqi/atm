/* 
 * The main program for the Bank.
 *
 * You are free to change this as necessary.
 */

#include <string.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include "bank.h"
#include "ports.h"

static const int BLOCK_SIZE = 16;
static const char prompt[] = "BANK: ";

int main(int argc, char**argv)
{
   int n;
   //char sendline[10000];
   char *sendline = (char *)calloc(10000, sizeof(char));
   char * recvline = (char *)calloc(10000, sizeof(char));
   Bank *bank = bank_create();
  
   bank->init =fopen(argv[1], "r");
   if(bank->init == NULL) {
    printf("Error opening bank initialization file\n");
    return 64;
   }

    fread(bank->key, sizeof(unsigned char), BLOCK_SIZE, bank->init);
    fread(bank->iv, sizeof(unsigned char), BLOCK_SIZE, bank->init);

   printf("%s", prompt);
   fflush(stdout);

   while(1)
   {
       fd_set fds;
       FD_ZERO(&fds);
       FD_SET(0, &fds);
       FD_SET(bank->sockfd, &fds);
       select(bank->sockfd+1, &fds, NULL, NULL, NULL);

       if(FD_ISSET(0, &fds))
       {
           fgets(sendline, 10000,stdin);
           bank_process_local_command(bank, sendline, strlen(sendline));
           printf("%s", prompt);
           fflush(stdout);
       }
       else if(FD_ISSET(bank->sockfd, &fds))
       {
           n = bank_recv(bank, recvline, 10000);
           bank_process_remote_command(bank, recvline, n);
       }
   }
   free(recvline);
   free(sendline);
   bank_free(bank);
   return EXIT_SUCCESS;
}
