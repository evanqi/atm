#include "atm.h"
#include "ports.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>

static const char BEGIN[] = "begin-session";
static const char WITHDRAW[] = "withdraw";
static const char BALANCE[] = "balance";
static const char END[] = "end-session";
static const int ATM_ACTION_MAX = 14;
static const int USERNAME_MAX = 250;


ATM* atm_create()
{
    ATM *atm = (ATM*) malloc(sizeof(ATM));
    if(atm == NULL)
    {
        perror("Could not allocate ATM");
        exit(1);
    }

    // Set up the network state
    atm->sockfd=socket(AF_INET,SOCK_DGRAM,0);

    bzero(&atm->rtr_addr,sizeof(atm->rtr_addr));
    atm->rtr_addr.sin_family = AF_INET;
    atm->rtr_addr.sin_addr.s_addr=inet_addr("127.0.0.1");
    atm->rtr_addr.sin_port=htons(ROUTER_PORT);

    bzero(&atm->atm_addr, sizeof(atm->atm_addr));
    atm->atm_addr.sin_family = AF_INET;
    atm->atm_addr.sin_addr.s_addr=inet_addr("127.0.0.1");
    atm->atm_addr.sin_port = htons(ATM_PORT);
    bind(atm->sockfd,(struct sockaddr *)&atm->atm_addr,sizeof(atm->atm_addr));

    // Set up the protocol state
    // TODO set up more, as needed
    atm->session = 0;
    atm->cur_user = (char *)calloc(USERNAME_MAX+1, sizeof(char));
    return atm;
}

void atm_free(ATM *atm)
{
    if(atm != NULL)
    {
        close(atm->sockfd);
        if(atm->cur_user != NULL)
          free(atm->cur_user);
        free(atm);
    }
}

ssize_t atm_send(ATM *atm, char *data, size_t data_len)
{
    // Returns the number of bytes sent; negative on error
    return sendto(atm->sockfd, data, data_len, 0,
                  (struct sockaddr*) &atm->rtr_addr, sizeof(atm->rtr_addr));
}

ssize_t atm_recv(ATM *atm, char *data, size_t max_data_len)
{
    // Returns the number of bytes received; negative on error
    return recvfrom(atm->sockfd, data, max_data_len, 0, NULL, NULL);
}

int is_valid_username(char *username) {
  int i;

  for(i=0; i<strlen(username); i++) {
    if(!((username[i] <= 'Z' && username[i] >= 'A')
          || (username[i] <= 'z' && username[i] >= 'a'))) {
        return 0;
    }
  }

  return 1;
}

int is_valid_pin(char *input_pin) {
  int i;

  for(i=0; i<strlen(input_pin); i++) {
    if(!(input_pin[i] <= '9' && input_pin[i] >= '0')) {
      return 0;
    }
  }

  return 1;
}

int is_valid_amount(char *amt) {
  int i;

  if(strlen(amt) > 10) {
    return 0;
  }

  for(i=0; i<strlen(amt); i++) {
    if(!(amt[i] <= '9' && amt[i] >= '0')) {
      return 0;
    }
  }

  return 1;
}


void atm_process_command(ATM *atm, char *command)
{
  char recvline[10000], inbuf[301], username[USERNAME_MAX+1], action[101];
  char input_pin[7];
  char *sendbuffer;
  int n = 0, i = 0;
  long amt = 0;

  //initialize buffers
  for(i=0; i<301; i++) {
    inbuf[i] = 0;
  }
  for(i=0; i<101; i++) {
    action[i] = 0;
  }
  for(i=0; i<10000; i++) {
    recvline[i] = 0;
  }
  for(i=0; i<6; i++) {
    input_pin[i] = 0;
  }

  //TODO: check for extraneous input after "begin-session <username>"
  sscanf(command, "%100s %300s", action, inbuf);

  if(strcmp(action, BEGIN) == 0) {
    if(atm->session) {
      printf("A user is already logged in\n");
      return;
    }
    if(strlen(inbuf) > USERNAME_MAX) {
      printf("Usage: begin-session <user-name>\n");
      return;
    }
    strcpy(username, inbuf);
    if(!is_valid_username(username)) {
      printf("Usage: begin-session <user-name>\n");
      return;
    }

    //Send "u <username>" to bank and expect "yes" if user exists
    sendbuffer = (char *)calloc(strlen(username)+3, sizeof(char));
    *sendbuffer ='u';
    *(sendbuffer+1)=' ';
    strcat(sendbuffer+2, username);
    *(sendbuffer+strlen(username) + 2) = 0;
    atm_send(atm, sendbuffer, strlen(sendbuffer)+1);
    free(sendbuffer);
    sendbuffer = NULL;
    n = atm_recv(atm,recvline,10000);
    recvline[n]=0;
    if(strcmp(recvline, "yes") != 0) {
      printf("No such user\n");
      return;
    }

    printf("PIN? ");
    fgets(input_pin, 6, stdin);
    if(!(strlen(input_pin) == 5 && input_pin[4] == '\n')) {
      printf("Not authorized\n");
      return;
    }
    input_pin[4] = 0;
    if(!is_valid_pin(input_pin)) {
      printf("Not authorized\n");
      return;
    }

    //send "p <username> <pin>" to bank and expect "yes" if pin is valid
    sendbuffer = (char *)calloc(strlen(username)+strlen(input_pin)+4, sizeof(char));
    *sendbuffer ='p';
    *(sendbuffer+1)= ' ';
    strcat(sendbuffer, username);
    *(sendbuffer+strlen(username)+2) = ' ';
    strcat(sendbuffer, input_pin);
    *(sendbuffer+strlen(username)+strlen(input_pin)+3) = 0;
    atm_send(atm, sendbuffer, strlen(sendbuffer)+1);
    free(sendbuffer);
    sendbuffer = NULL;
    n = atm_recv(atm,recvline,10000);
    recvline[n]=0;

printf("%s\n", recvline);
    if(strcmp(recvline, "yes") != 0) {
      printf("Not authorized\n");
      return;
    }

    printf("Authorized\n");
    atm->session = 1;
    atm->cur_user = (char *)calloc(strlen(username)+1, sizeof(char));
    strcpy(atm->cur_user, username);
  }
  else if(strcmp(action, WITHDRAW) == 0) {
    if(!atm->session) {
      printf("No user logged in\n");
      return;
    }
    if(!is_valid_amount(inbuf)) {
      printf("Usage: withdraw <amt>\n");
      return;
    }
    amt = strtol(inbuf, NULL, 10);
    if(amt < 0 || amt > INT_MAX) {
      printf("Usage: withdraw <amt>\n");
      return;
    }

    //send "w <username> 0 <amt> to bank, expect "yes" if sufficient funds
    sendbuffer = (char *)calloc(strlen(atm->cur_user)+strlen(inbuf)+6, sizeof(char));
    *sendbuffer ='w';
    *(sendbuffer+1)= ' ';
    strcat(sendbuffer, username);
    *(sendbuffer+strlen(username)+2) = ' ';
    *(sendbuffer+strlen(username)+3) = '0';
    *(sendbuffer+strlen(username)+4) = ' ';
    strcat(sendbuffer, inbuf);
    *(sendbuffer+strlen(username)+strlen(inbuf)+5) = 0;
    atm_send(atm, sendbuffer, strlen(sendbuffer)+1);
    free(sendbuffer);
    sendbuffer = NULL;
    n = atm_recv(atm,recvline,10000);
    recvline[n]=0;

    if(strcmp(recvline, "yes") == 0) {
      printf("$%d dispensed\n", (int)amt);
    } else {
      printf("Insufficient funds\n");
    }
  }
  else if(strcmp(action, BALANCE) == 0) {
    if(!atm->session) {
      printf("No user logged in\n");
      return;
    }

    if(inbuf[0] != 0) {
      printf("Usage: balance\n");
      return;
    }

    //send "b <username>" to bank, expect <amt>
    sendbuffer = (char *)calloc(strlen(username)+3, sizeof(char));
    *sendbuffer ='b';
    *(sendbuffer+1)=' ';
    strcat(sendbuffer+2, username);
    *(sendbuffer+strlen(username) + 2) = 0;
    atm_send(atm, sendbuffer, strlen(sendbuffer)+1);
    free(sendbuffer);
    sendbuffer = NULL;
    n = atm_recv(atm,recvline,10000);
    recvline[n]=0;

    int balance = atoi(recvline);
    printf("balance: $%d\n", balance);
  }
  else if(strcmp(action, END) == 0) {
    if(!atm->session) {
      printf("No user logged in\n");
      return;
    }

    atm->session = 0;
    free(atm->cur_user);
    atm->cur_user = NULL;
    printf("User logged out\n");
  }
  else {
    printf("Invalid command\n");
    return;
  }

	/*
	 * The following is a toy example that simply sends the
	 * user's command to the bank, receives a message from the
	 * bank, and then prints it to stdout.
	 */

	/*
    char recvline[10000];
    int n;

    atm_send(atm, command, strlen(command));
    n = atm_recv(atm,recvline,10000);
    recvline[n]=0;
    fputs(recvline,stdout);
	*/


}
