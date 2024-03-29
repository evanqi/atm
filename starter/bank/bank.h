/*
 * The Bank takes commands from stdin as well as from the ATM.  
 *
 * Commands from stdin be handled by bank_process_local_command.
 *
 * Remote commands from the ATM should be handled by
 * bank_process_remote_command.
 *
 * The Bank can read both .card files AND .pin files.
 *
 * Feel free to update the struct and the processing as you desire
 * (though you probably won't need/want to change send/recv).
 */

#ifndef __BANK_H__
#define __BANK_H__

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <openssl/evp.h>
#include <stdio.h>
#include "util/hash_table.h"
#include "util/list.h"


typedef struct _Bank
{
    // Networking state
    int sockfd;
    struct sockaddr_in rtr_addr;
    struct sockaddr_in bank_addr;

    // Protocol state
    // TODO add more, as needed

    HashTable *user_bal;
    HashTable *user_pin;
    FILE *init;
    unsigned char *key;
    unsigned char *iv;
} Bank;

Bank* bank_create();
void bank_free(Bank *bank);
ssize_t bank_send(Bank *bank, char *data, size_t data_len);
ssize_t bank_recv(Bank *bank, char *data, size_t max_data_len);
void bank_process_local_command(Bank *bank, char *command, size_t len);
void bank_process_remote_command(Bank *bank, char *command, size_t len);
int check_username(char *username);
int check_pin(char *pin);
int check_bal(char *bal);
void send_no_fund(Bank *bank);
void send_no(Bank *bank);
void send_yes(Bank *bank);
void send_no_user(Bank *bank);
void send_no_pin(Bank *bank);
void send_balance(Bank *bank, char *bal);
void encrypt(FILE *init, char *plain,unsigned char * encrypted);
void decrypt(FILE *init,unsigned char * encrypted, unsigned char * decypted);
int do_crypt(Bank *bank, unsigned char *inbuf, unsigned char *res, int do_encrypt);

#endif

