#include "bank.h"
#include "ports.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>


#define MAX_COMMAND_SIZE 12
#define MAX_NAME_SIZE 251
#define MAX_MISC_SIZE 12
#define MAX_LINE_SIZE 1001

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

    return bank;
}

void bank_free(Bank *bank)
{
    if(bank != NULL)
    {
        fclose(bank->init);
        free(bank->user_pin);
        free(bank->user_bal);
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

void bank_process_local_command(Bank *bank, char *command, size_t len)
{
    char comm[MAX_COMMAND_SIZE];
    char name[MAX_NAME_SIZE];
    char misc[MAX_MISC_SIZE];
    char misc_two[MAX_MISC_SIZE];

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
        fclose(card);

        printf("pin: %s, balance: %s\n", misc, misc_two);
        hash_table_add(bank->user_bal, name, misc_two);
        hash_table_add(bank->user_pin, name, misc);

        printf("user's balance -> %s\n", (char*) hash_table_find(bank->user_bal, name));
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
        
        int curr_bal_int = strtol(curr_bal, NULL, 10);
        int amt = strtol(misc, NULL, 10);
        long new_bal = amt + curr_bal_int;  

        printf("curr: %d\n", curr_bal_int);

        //checks if new_bal was capped
        if( new_bal > INT_MAX || ((new_bal == INT_MAX) && ((new_bal - amt) != curr_bal_int))){
            printf("Too rich for this program\n");
            return;   
        }       

        char new_bal_char[MAX_MISC_SIZE];

        sprintf(new_bal_char, "%ld", new_bal);
        //update balance
        // BELOW GIVES MALLOC ERRORS
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
}

int check_username(char *username) 
{
    if(username == NULL)
        return 0;
    int u, s = strlen(username);
    for(u = 0; u < s; u++)
    {
        if(username[u] > 122 || username[u] < 65)
            return 0;
        else if(username[u] > 90 && username[u] < 97)
            return 0;
    }
    return 1;
}

int check_pin(char *pin)
{
    printf("pin: %s\n", pin);
    int b;
    for(b = 0; b < 4; b++)
    {
        if(!isdigit(pin[b]))
            return 0;
    }

    long p = strtol(pin, NULL, 10);
    if (p > 9999 || p < 0){
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
    if(balance > INT_MAX || balance < 0)
        return 0;
    return 1;
}


