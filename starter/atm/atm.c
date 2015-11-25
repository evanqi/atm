#include "atm.h"
#include "ports.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

static const char BEGIN[] = "begin-session";
static const char WITHDRAW[] = "withdraw";
static const char BALANCE[] = "balance";
static const char END[] = "end-session";
static const int ATM_ACTION_MAX = 14;
static const int USERNAME_ACTION_MAX = 250;


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
    atm->cur_user = (char *)malloc(USERNAME_ACTION_MAX*sizeof(char));
    return atm;
}

void atm_free(ATM *atm)
{
    if(atm != NULL)
    {
        close(atm->sockfd);
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

void atm_process_command(ATM *atm, char *command)
{
  char inbuf[USERNAME_ACTION_MAX], username[USERNAME_ACTION_MAX], action[ATM_ACTION_MAX];
  char input_pin[5];
  int pin = 0, amt;

  //TODO: check length of action and username in sscanf
  sscanf(command, "%s %s", action, inbuf);

  if(strcmp(action, BEGIN) == 0) {
    if(atm->session) {
      printf("A user is already logged in\n");
      return;
    }
    strcpy(username, inbuf);
    if(!is_valid_username(username)) {
      printf("Usage: begin-session <user-name>\n");
      return;
    }

    //TODO: validate username with bank
    /* char recvline[10000]; */
    /* int n; */

    /* atm_send(atm, command, strlen(command)); */
    /* n = atm_recv(atm,recvline,10000); */
    /* recvline[n]=0; */
    /* fputs(recvline,stdout); */


    //TODO: better pin input validation
    printf("PIN? ");
    scanf("%c%c%c%c", &input_pin[0], &input_pin[1], &input_pin[2], &input_pin[3]);
    input_pin[4] = '\0';
    pin = atoi(input_pin);
    if(pin < 0 || pin > 9999) {
      printf("Not authorized\n");
      return;
    }

    //TODO: validate pin with bank
    
    int flag = 1;
    if(flag) {
      printf("Authorized\n");
      atm->session = 1;
      atm->cur_user = (char *)malloc((strlen(username)+1)*sizeof(char));
      strcpy(atm->cur_user, username);
    }
    else {
      printf("No such user\n");
    }
  }
  else if(strcmp(action, WITHDRAW) == 0) {
    if(!session) {
      printf("No user logged in\n");
      return;
    }
    amt = atoi(inbuf);
    if(amt < 0) {
      printf("Usage: withdraw <amt>\n");
      return;
    }

    //TODO: ask bank to withdraw money
    
    int flag = 1;
    if(flag) {
      printf("$%d dispensed\n", amt);
    } else {
      printf("Insufficient funds\n");
    }
  }
  else if(strcmp(action, BALANCE) == 0) {
    if(!session) {
      printf("No user logged in\n");
      return;
    }

    if(inbuf[0] != 0) {
      printf("Usage: balance\n");
      return;
    }

    //TODO: ask bank for balance
    int balance = 0;
    printf("$%d: balance\n", balance);
  }
  else if(strcmp(action, END) == 0) {
    if(!session) {
      printf("No user logged in\n");
      return;
    }

    session = 0;
    free(atm->cur_user);
    atm->cur_user = NULL;
    printf("User logged out\n");
  }
  else if(strcmp(action, "\n") == 0) {
    return;
  }
  else {
    printf("Invalid command\n");
    return;
  }

    // TODO: Implement the ATM's side of the ATM-bank protocol

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
