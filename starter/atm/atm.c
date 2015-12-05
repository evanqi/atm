#include "atm.h"
#include "ports.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include "util/hash_table.h"
#include "util/list.h"

static const char BEGIN[] = "begin-session";
static const char WITHDRAW[] = "withdraw";
static const char BALANCE[] = "balance";
static const char END[] = "end-session";
static const int ATM_ACTION_MAX = 14;
static const int USERNAME_MAX = 250;
static const int BLOCK_SIZE = 16;

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
    atm->key = (unsigned char *)calloc(BLOCK_SIZE+1, sizeof(unsigned char));
    atm->iv = (unsigned char *)calloc(BLOCK_SIZE+1, sizeof(unsigned char));
    atm->attempts = hash_table_create(100);
    return atm;
}

void atm_free(ATM *atm)
{
    if(atm != NULL)
    {
        close(atm->sockfd);
        if(atm->cur_user != NULL)
          free(atm->cur_user);
	free(atm->key);
        hash_table_free(atm->attempts);
	free(atm->iv);
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

/*
  unsigned char *out = (unsigned char *)calloc(10000, sizeof(char));
  unsigned char *back = (unsigned char *)calloc(10000, sizeof(char));

  int len = do_crypt(atm, (unsigned char *)command, out, 1, strlen(command));
  do_crypt(atm, out, back, 0, len);
  free(out); free(back);*/

int do_crypt(ATM *atm, unsigned char *inbuf, unsigned char *res, int do_encrypt)
        {
        unsigned char outbuf[10000 + EVP_MAX_BLOCK_LENGTH];
        int outlen, len;
        EVP_CIPHER_CTX ctx;

        EVP_CIPHER_CTX_init(&ctx);
        EVP_CipherInit_ex(&ctx, EVP_aes_128_cbc(), NULL, NULL, NULL,
                do_encrypt);
        OPENSSL_assert(EVP_CIPHER_CTX_key_length(&ctx) == 16);
        OPENSSL_assert(EVP_CIPHER_CTX_iv_length(&ctx) == 16);

        EVP_CipherInit_ex(&ctx, NULL, NULL, atm->key, atm->iv, do_encrypt);

	if(!EVP_CipherUpdate(&ctx, outbuf, &outlen, inbuf, strlen((char *)inbuf)))
	{
		EVP_CIPHER_CTX_cleanup(&ctx);
		return 0;
	}
	memcpy(res, outbuf, outlen);
	len = outlen;
	if(!EVP_CipherFinal_ex(&ctx, outbuf, &outlen))
	{
		EVP_CIPHER_CTX_cleanup(&ctx);
		return 0;
	}
	memcpy(res+len, outbuf, outlen);
	len += outlen;
        EVP_CIPHER_CTX_cleanup(&ctx);
        return len;
	}

void atm_process_command(ATM *atm, char *command)
{
  char recvline[10000], inbuf[301], username[USERNAME_MAX+1], action[101];
  char input_pin[7];
  char *sendbuffer;
  int n = 0, i = 0;
  unsigned int amt;

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
  fflush(stdout);

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

	//USERNAME for the first time
    if(hash_table_find(atm->attempts, username) == NULL)
    {
	hash_table_add(atm->attempts, username, "0");
    } else {
	char* try = (char*) hash_table_find(atm->attempts, username);
	int try_int = atoi(try);
	try_int++;
	char *new_try =(char *)calloc(5, sizeof(char));
        sprintf(new_try, "%d", try_int);
	hash_table_del(atm->attempts, username);
	hash_table_add(atm->attempts, username, new_try);
	if(try_int > 2)
	{
	printf("LOCKED OUT\n");
	return;
	}
    }

    char *tmp_buffer = (char *)calloc(strlen(username)+6, sizeof(char));
    strcpy(tmp_buffer, username);
    strcat(tmp_buffer, ".card");
    atm->card = fopen(tmp_buffer, "r");
    if(atm->card == NULL) {
      printf("Unable to access %s's card\n", username);
      return;
    }
    free(tmp_buffer);

    //Send "u <username>" to bank and expect "yes" if user exists
    sendbuffer = (char *)calloc(strlen(username)+3, sizeof(char));
    *sendbuffer ='u';
    *(sendbuffer+1)=' ';
    strcat(sendbuffer+2, username);
    *(sendbuffer+strlen(username) + 2) = 0;
    unsigned char *out = (unsigned char *)calloc(10000, sizeof(unsigned char));
    do_crypt(atm, (unsigned char *)sendbuffer, out, 1);
    atm_send(atm, (char *)out, strlen((char *)out));
    free(sendbuffer);
    free(out);
    sendbuffer = NULL;
    out = NULL;
    n = atm_recv(atm,recvline,10000);
    recvline[n]=0;
    
    out = (unsigned char *)calloc(10000, sizeof(unsigned char));
    do_crypt(atm, recvline, out, 0);

    if(strcmp((char *)out, "yes") != 0) {
      printf("No such user\n");
      return;
    }
    free(out);
    out = NULL;

    printf("PIN? ");
    fgets(input_pin, 6, stdin);
    fflush(stdout);
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
    out = (unsigned char *)calloc(10000, sizeof(unsigned char));
    do_crypt(atm, (unsigned char *)sendbuffer, out, 1);
    atm_send(atm, (char *)out, strlen((char *)out));
    free(sendbuffer);
    free(out);
    sendbuffer = NULL;
    out = NULL;
    n = atm_recv(atm,recvline,10000);
    recvline[n]=0;
    
    out = (unsigned char *)calloc(10000, sizeof(unsigned char));
    do_crypt(atm, recvline, out, 0);

    if(strcmp((char *)out, "yes") != 0) {
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
    char *p;
    amt = (unsigned int)strtoul(inbuf, &p, 10);
    if(amt < 0 || amt > UINT_MAX) {
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
    unsigned char *out = (unsigned char *)calloc(10000, sizeof(unsigned char));
    do_crypt(atm, (unsigned char *)sendbuffer, out, 1);
    atm_send(atm, (char *)out, strlen((char *)out));
    free(sendbuffer);
    free(out);
    sendbuffer = NULL;
    out = NULL;
    n = atm_recv(atm,recvline,10000);
    recvline[n]=0;
    
    out = (unsigned char *)calloc(10000, sizeof(unsigned char));
    do_crypt(atm, recvline, out, 0);

    if(!strcmp((char *)out, "yes") != 0) {
      printf("$%u dispensed\n", amt);
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
    unsigned char *out = (unsigned char *)calloc(10000, sizeof(unsigned char));
    do_crypt(atm, (unsigned char *)sendbuffer, out, 1);
    atm_send(atm, (char *)out, strlen((char *)out));
    free(sendbuffer);
    free(out);
    sendbuffer = NULL;
    out = NULL;
    n = atm_recv(atm,recvline,10000);
    recvline[n]=0;
    
    out = (unsigned char *)calloc(10000, sizeof(unsigned char));
    do_crypt(atm, recvline, out, 0);
    char *ptr;
    unsigned int balance = (unsigned int)strtoul((char *)out, &ptr, 10);
    printf("balance: $%u\n", balance);
  }
  else if(strcmp(action, END) == 0) {
    if(!atm->session) {
      printf("No user logged in\n");
      return;
    }

    atm->session = 0;
    free(atm->cur_user);
    atm->cur_user = NULL;
    fclose(atm->card);
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
