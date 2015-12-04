#include "bank.h"
#include "ports.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include <openssl/evp.h>

#define MAX_COMMAND_SIZE 12
#define MAX_NAME_SIZE 251
#define MAX_MISC_SIZE 12
#define MAX_LINE_SIZE 1001
#define MAX_PIN_SIZE 5
#define MAX_AMT_SIZE 11
#define BLOCK_SIZE 16

Bank* bank_create()
{
    Bank *bank = (Bank*) malloc(sizeof(Bank));
    if(bank == NULL)
    {
        perror("Could not allocate Bank");
        exit(1);
    }

    // Set up the network state
    bank->sockfd=socket(AF_INET,SOCK_DGRAM,0);

    bzero(&bank->rtr_addr,sizeof(bank->rtr_addr));
    bank->rtr_addr.sin_family = AF_INET;
    bank->rtr_addr.sin_addr.s_addr=inet_addr("127.0.0.1");
    bank->rtr_addr.sin_port=htons(ROUTER_PORT);

    bzero(&bank->bank_addr, sizeof(bank->bank_addr));
    bank->bank_addr.sin_family = AF_INET;
    bank->bank_addr.sin_addr.s_addr=inet_addr("127.0.0.1");
    bank->bank_addr.sin_port = htons(BANK_PORT);
    bind(bank->sockfd,(struct sockaddr *)&bank->bank_addr,sizeof(bank->bank_addr));

    // Set up the protocol state
    // TODO set up more, as needed

    bank->user_pin = hash_table_create(100);
    bank->user_bal = hash_table_create(100);
    bank->key = (unsigned char *)calloc(BLOCK_SIZE, sizeof(unsigned char));
    bank->iv = (unsigned char *)calloc(BLOCK_SIZE, sizeof(unsigned char));

    return bank;
}

void bank_free(Bank *bank)
{
    if(bank != NULL)
    {
        fclose(bank->init);
        hash_table_free(bank->user_pin);
        hash_table_free(bank->user_bal);
	free(bank->key);
	free(bank->iv);
        close(bank->sockfd);
        free(bank);
    }
}

ssize_t bank_send(Bank *bank, char *data, size_t data_len)
{
    // Returns the number of bytes sent; negative on error
    return sendto(bank->sockfd, data, data_len, 0,
                  (struct sockaddr*) &bank->rtr_addr, sizeof(bank->rtr_addr));
}

ssize_t bank_recv(Bank *bank, char *data, size_t max_data_len)
{
    // Returns the number of bytes received; negative on error
    return recvfrom(bank->sockfd, data, max_data_len, 0, NULL, NULL);
}
/*
  unsigned char *out = (unsigned char *)calloc(10000, sizeof(char));
  unsigned char *back = (unsigned char *)calloc(10000, sizeof(char));

  int len = do_crypt(atm, (unsigned char *)command, out, 1, strlen(command));
  do_crypt(atm, out, back, 0, len);
  free(out); free(back);*/

int do_crypt(Bank *bank, unsigned char *inbuf, unsigned char *res, int do_encrypt, int inlen)
        {
        unsigned char outbuf[10000 + EVP_MAX_BLOCK_LENGTH];
        int outlen, len;
        EVP_CIPHER_CTX ctx;

        EVP_CIPHER_CTX_init(&ctx);
        EVP_CipherInit_ex(&ctx, EVP_aes_128_cbc(), NULL, NULL, NULL,
                do_encrypt);
        OPENSSL_assert(EVP_CIPHER_CTX_key_length(&ctx) == 16);
        OPENSSL_assert(EVP_CIPHER_CTX_iv_length(&ctx) == 16);

        EVP_CipherInit_ex(&ctx, NULL, NULL, bank->key, bank->iv, do_encrypt);

	if(!EVP_CipherUpdate(&ctx, outbuf, &outlen, inbuf, inlen))
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

void bank_process_local_command(Bank *bank, char *command, size_t len)
{



    char *comm = (char *)calloc(MAX_COMMAND_SIZE, sizeof(char));
    char *name = (char *)calloc(MAX_COMMAND_SIZE, sizeof(char));
    char *misc = (char *)calloc(MAX_COMMAND_SIZE, sizeof(char));
    char *misc_two = (char *)calloc(MAX_COMMAND_SIZE, sizeof(char));
    /*unsigned char *encrypted = (unsigned char*) calloc (1024 + EVP_MAX_BLOCK_LENGTH, sizeof(unsigned char));
    unsigned char *decrypted = (unsigned char*) calloc (1024 + EVP_MAX_BLOCK_LENGTH, sizeof(unsigned char));*/

    char temp_comm[MAX_LINE_SIZE];
    char temp_name[MAX_LINE_SIZE];
    char temp_misc[MAX_LINE_SIZE];
    char temp_misc_two[MAX_LINE_SIZE];

    if(strlen(command) >= MAX_LINE_SIZE)
    {
        printf("Invalid command\n");
        return;
    }

    sscanf(command, "%s %s %s %s", temp_comm,temp_name,temp_misc,temp_misc_two);
    
    if(strlen(temp_comm) < 1)
    {
        printf("Invalid command\n");
        return;
    }

    if(strlen(temp_comm) >= MAX_COMMAND_SIZE)
    {
        printf("Invalid command\n");
        return;
    }

    strncpy(comm, temp_comm, strlen(temp_comm) + 1);


    //TESTING ENCRYPTION AND DECRYPTION
   //encrypt(bank->init, comm, encrypted);
   //decrypt(bank->init, encrypted, decrypted);


    if(strcmp(comm, "create-user") == 0)
    {
        if(strlen(temp_name) < 1 || strlen(temp_misc) < 1 || strlen(temp_misc_two) < 1)
        {
            printf("Usage: create-user <user-name> <pin> <balance>\n");
            return;
        }

        if(strlen(temp_name) >= MAX_NAME_SIZE)
        {
            printf("Usage: create-user <user-name> <pin> <balance>\n");
            return;
        }

        strncpy(name, temp_name, strlen(temp_name) + 1);

        if(check_username(name) == 0)
        {
            printf("Usage: create-user <user-name> <pin> <balance>\n");
            return;
        }

        if(hash_table_find(bank->user_bal, name) != NULL)
        {
            printf("%s already exists\n", name);
            return;
        }

        if(strlen(temp_misc) != 4)
        {
            printf("Usage: create-user <user-name> <pin> <balance>\n");
            return;
        }

        strncpy(misc, temp_misc, strlen(temp_misc) + 1);
        if(check_pin(misc) == 0)
        {
            printf("Usage: create-user <user-name> <pin> <balance>\n");
            return;
        }

        if(strlen(temp_misc_two) > 10) 
        {
            printf("Usage: create-user <user-name> <pin> <balance>\n");
            return;
        }

        strncpy(misc_two, temp_misc_two, strlen(temp_misc_two) + 1);
        if(check_bal(misc_two) == 0)
        {
            printf("Usage: create-user <user-name> <pin> <balance>\n");
            return;
        }

         // create card
         int ufilelen = strlen(name) + 6;
         char userfile[ufilelen];
         memset(userfile, 0x00, ufilelen);
         strncpy(userfile, name, strlen(name));
         strncat(userfile, ".card", 5);
         FILE *card = fopen(userfile, "w");
         if(card == NULL){
             printf("Error creating card file for user %s\n", name);
             remove(userfile);
             return;
        }
	fprintf(card, "%s", misc);
        fclose(card);

        hash_table_add(bank->user_bal, name, misc_two); //username balance
        hash_table_add(bank->user_pin, name, misc);	//username pin


        printf("Created user %s\n", name);
        return;
    } 
    else if(strcmp(comm, "deposit") == 0)
    {
        if(strlen(temp_name) < 1 || strlen(temp_misc) < 1)
        {
            printf("Usage: deposit <user-name> <amt>\n");
            return;
        }

        if(strlen(temp_name) >= MAX_NAME_SIZE)
        {
            printf("Usage: deposit <user-name> <amt>\n");
            return;
        }

        strncpy(name, temp_name, strlen(temp_name) + 1);
        if(check_username(name) != 1)
        {
            printf("Usage: deposit <user-name> <amt>\n");
            return;

        }

        if(hash_table_find(bank->user_bal, name) == NULL)
        {
            printf("No such user\n");
            return;
        }

        if(strlen(temp_misc) > 10)
        {
            printf("Usage: deposit <user-name> <amt>\n");
            return;
        }

        strncpy(misc, temp_misc, strlen(temp_misc) + 1);
        if(check_bal(misc) == 0)
        {
            printf("Usage: deposit <user-name> <amt>\n");
            return;
        }
       
        char* curr_bal = (char*) hash_table_find(bank->user_bal, name);
        
        unsigned int curr_bal_int = strtol(curr_bal, NULL, 10);
        unsigned int amt = strtol(misc, NULL, 10);
        unsigned int new_bal = amt + curr_bal_int;  

        //checks if new_bal was capped
        if( new_bal > INT_MAX || ((new_bal == INT_MAX) && ((new_bal - amt) != curr_bal_int))){
            printf("Too rich for this program\n");
            return;   
        }       

        char *new_bal_char = (char *)calloc(MAX_MISC_SIZE, sizeof(char));

        sprintf(new_bal_char, "%u", new_bal);

        hash_table_del(bank->user_bal, name);
        hash_table_add(bank->user_bal, name, new_bal_char);

        printf("$%s added to %s's account\n", misc, name);
        return;

    }
    else if(strcmp(comm, "balance") == 0)
    {
        if(strlen(temp_name) < 1)
        {
            printf("Usage: balance <user-name>\n");
            return;
        }

        if(strlen(temp_name) >= MAX_NAME_SIZE)
        {
            printf("Usage: balance <user-name>\n");
            return;
        }

        strncpy(name, temp_name, strlen(temp_name) + 1);
        if(check_username(name) != 1)
        {
            printf("Usage: balance <user-name>\n");
            return;
        }

        char *curr_bal = (char *) hash_table_find(bank->user_bal, name);
        if(curr_bal == NULL)
        {
            printf("No such user\n");
            return;
        }

        printf("$%s\n", curr_bal);
        return;

    }
    else
    {
        printf("Invalid command\n");
        return;
    }
}

void bank_process_remote_command(Bank *bank, char *command, size_t len)
{
    // TODO: Implement the bank side of the ATM-bank protocol

	/*
	 * The following is a toy example that simply receives a
	 * string from the ATM, prepends "Bank got: " and echoes 
	 * it back to the ATM before printing it to stdout.
	 */

	/*
    char sendline[1000];
    command[len]=0;
    sprintf(sendline, "Bank got: %s", command);
    bank_send(bank, sendline, strlen(sendline));
    printf("Received the following:\n");
    fputs(command, stdout);
	*/

    // ASSUME everything valid (checked in atm)
    char *comm = calloc(2, sizeof(char)); // w = withdrawal, u = user exists?, b = balance, p = user pin
    char *name = calloc(MAX_NAME_SIZE, sizeof(char));
    char *pin = calloc(MAX_PIN_SIZE, sizeof(char));
    char *amt = calloc(MAX_AMT_SIZE, sizeof(char));

    sscanf(command, "%s %s %s %s", comm, name, pin, amt);

    if(strcmp(comm, "u") == 0)
    {
	if(hash_table_find(bank->user_bal,name) == NULL)
	{
	    send_no(bank);
	    return;
	}
	else
	{
	    send_yes(bank);
	    return;
	}
    }
    else if(strcmp(comm, "b") == 0)
    {
	if(hash_table_find(bank->user_bal, name) == NULL)
	{
	    send_no_user(bank);
	    return;
	}
	char* balance = (char *)hash_table_find(bank->user_bal, name);
	send_balance(bank, balance);
	return;
    }
    else if(strcmp(comm, "p") == 0)
    {
	if(hash_table_find(bank->user_bal, name) == NULL)
	{
	    send_no_user(bank);
	    return;
	}

	char* pin_from_hash = (char *) hash_table_find(bank->user_pin, name);
	if(pin_from_hash == NULL)
	{
	    send_no_pin(bank);
	    return;
	}

	if(strcmp(pin_from_hash, pin) == 0)
	{
	    send_yes(bank);
	    return;
	}
	else
	{
	    send_no(bank);
	    return;
	}
    }
    else if(strcmp(comm, "w") == 0)
    {
	if(hash_table_find(bank->user_bal, name) == NULL)
	{
	    send_no_user(bank);
	    return;
	}

	unsigned int amt_w = strtol(amt, NULL, 10);
	unsigned int curr_amt = strtol((char *)hash_table_find(bank->user_bal, name), NULL, 10);

	if(curr_amt < amt_w)
	{
	    send_no_fund(bank);
	    return;
	}
	unsigned int new_amt = curr_amt - amt_w;

	char *new_amt_char = calloc(MAX_AMT_SIZE, sizeof(char));
	sprintf(new_amt_char, "%u", new_amt);

	hash_table_del(bank->user_bal, name);
	hash_table_add(bank->user_bal, name,new_amt_char);
	send_yes(bank);
	return;
    }    
}

// 122 = z
// 65 = A
// 90 = Z
// 97 = a
// 91 ~ 96 = not alpha
int check_username(char *username) 
{
    if(username == NULL)
        return 0;
    int u, s = strlen(username);
    for(u = 0; u < s; u++)
    {
	if(!isalpha(username[u]))
	    return 0;
    }
    return 1;
}

int check_pin(char *pin)
{
    int b;
    for(b = 0; b < 4; b++)
    {
        if(!isdigit(pin[b]))
            return 0;
    }

    long p = strtol(pin, NULL, 10);
    if (p < 0){
        return 0;
    }
    return 1;
}

int check_bal(char *bal)
{
    int b, s = strlen(bal);
    for(b = 0; b < s; b++)
    {
        if(!isdigit(bal[b]))
            return 0;
    }

    long balance = strtol(bal, NULL, 10);
    if(balance < 0 || balance > INT_MAX)
        return 0;
    return 1;
}

void send_no_fund(Bank *bank)
{
    bank_send(bank, "nofund", sizeof("nofund"));
}

void send_no(Bank *bank)
{
    bank_send(bank, "no", sizeof("no"));
}

void send_yes(Bank *bank)
{
    bank_send(bank, "yes", sizeof("yes"));
}

void send_no_user(Bank *bank)
{
    bank_send(bank, "nouser", sizeof("nouser"));
}

void send_no_pin(Bank *bank)
{
    bank_send(bank, "nopin", sizeof("nopin"));
}

void send_balance(Bank *bank, char *bal)
{
    bank_send(bank, bal, strlen(bal)+1);
}

/*void encrypt(FILE *init, char *plain, unsigned char *encrypted)
{

    unsigned char key[17] = {0};
    unsigned char iv[17] = {0};

    fgets(key, 17, init);
    fgets(iv, 17, init);

    //printf("key: %s\n", key);
    //printf("iv: %u\n", iv);

    EVP_CIPHER_CTX ctx;
    EVP_CIPHER_CTX_init(&ctx);
    int outlen;
    EVP_EncryptInit_ex(&ctx, EVP_aes_128_cbc(), NULL, NULL, NULL);

    OPENSSL_assert(EVP_CIPHER_CTX_key_length(&ctx) == 16);
    OPENSSL_assert(EVP_CIPHER_CTX_iv_length(&ctx) == 16);

    EVP_EncryptInit_ex(&ctx, EVP_aes_128_cbc(), NULL, key, iv);

    EVP_EncryptUpdate(&ctx, encrypted, &outlen, plain, strlen(plain));
    EVP_EncryptFinal_ex(&ctx, encrypted, &outlen);

    EVP_CIPHER_CTX_cleanup(&ctx);

    rewind(init);
}

void decrypt(FILE *init, unsigned char * encrypted, unsigned char * decrypted)
{
    unsigned char key[17] = {0};
    unsigned char iv[17] = {0};

    fgets(key, 17, init);
    fgets(iv, 17, init);


    EVP_CIPHER_CTX ctx;
    EVP_CIPHER_CTX_init(&ctx);
    int outlen;

    EVP_DecryptInit_ex(&ctx, EVP_aes_128_cbc(), NULL, key, iv);

    EVP_DecryptUpdate(&ctx, decrypted, &outlen, encrypted, strlen(encrypted));
    EVP_DecryptFinal_ex(&ctx, decrypted, &outlen);
    
    printf("decrypted: %s\n", decrypted);

    rewind(init);

}*/




