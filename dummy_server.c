#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include "color_print.h"

#define MSGSIZE 2048

#define NIPQUAD(addr) ((unsigned char *)&addr)[0], ((unsigned char *)&addr)[1], ((unsigned char *)&addr)[2], ((unsigned char *)&addr)[3]

struct function
{
    char name[MSGSIZE];
    char description[MSGSIZE];
    int total_param[4]; // 0 - char; 1 - int; 2 - float, double; 3 - total
    int *param_type;    // 0 - char; 1 - int; 2 - float, double
    int result_type;    // 0 - char; 1 - int; 2 - float, double; 11 - int array
    int access;
} *func;

struct server
{
    char addr[50];
    int port, sockfd, total_functions;
    struct hostent *server;
    struct function *func;
    struct server *next;
};

struct server *head = NULL;
struct server *curr = NULL;
struct server *prev = NULL;

int my_sockfd, in_sockfd, i;
socklen_t addr_len = sizeof(struct sockaddr);
struct sockaddr_in server_addr, in_addr, my_addr;

int total_servers=0, total_functions=0, total_functions_accessable=0, PORT, GRUMPY=1;
char ADDR[INET_ADDRSTRLEN];

void usage(char *);
void parse_arg(int, char**);
void start_server();
void parse_info(struct server **, char *);
void handle_request_client(int, char *);
void removeSubstring(char *, const char *);
 
int main(int argc, char *argv[])
{
    signal(SIGCHLD, SIG_IGN);
    parse_arg(argc, argv);      // parse the input
    
    start_server(); // start the server

    return 1;
}

void parse_arg(int argc, char **argv)
{
    int i=1;
    if ( !(argc-1) ) usage("Error no arguments passed. Program exiting.");
    while ( i < argc )
    {
        if ( !strcasecmp(argv[i], "-p") )
        { // store the port
            if ( ++i >= argc ) usage("No port passed.");
            PORT = atoi(argv[i]);
            ++i;
        } else usage("Invalid argument.");
    }    
}

void usage(char *s)
{ // return usage of the server code
    if ( strlen(s) ) printf(SET_RED("%s\n"), s);
    printf(SET_BLUE("USAGE:\n"));
    printf(SET_BLUE("\t-p/P <PORT number>\n"));
    printf(SET_BLUE("\t-f/F <functions to run on the server>\n"));
    printf(SET_BLUE("\t-n/N <list of neighbor server address:port>\n"));
    printf(SET_BLUE("For more read the report!\n"));
    exit(1);
}

void start_server()
{
    if ( (my_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
    {
        perror(SET_RED("Socket Error"));
        exit(1);
    } else {
        server_addr.sin_family       = AF_INET;
        server_addr.sin_addr.s_addr  = htons(INADDR_ANY);
        server_addr.sin_port         = htons(PORT);

        if ( bind(my_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 )
        {
            perror(SET_RED("Binding Error"));
            exit(1);
        } else {
            if ( listen(my_sockfd, 20) < 0 )
            {
                perror(SET_RED("Listening Error"));
                exit(1);
            } else {
                while ( 1 )
                { // wait for incoming connections
                    printf(SET_GREEN("Waiting for incoming connection...\n"));
                    addr_len = sizeof(in_addr);
                    if ( (in_sockfd = accept(my_sockfd, (struct sockaddr *)&in_addr, &addr_len)) < 0 )
                    {
                        perror(SET_RED("Accepting Connection Error"));
                        exit(1);
                    } else {
                        getsockname(in_sockfd, (struct sockaddr *)&my_addr, &addr_len);
                        sprintf(ADDR, "%d.%d.%d.%d", NIPQUAD(my_addr.sin_addr.s_addr));

                        char msg[MSGSIZE];
                        bzero(msg, MSGSIZE);
                        read(in_sockfd, msg, MSGSIZE);
                        printf("recv1:%ld\n", strlen(msg));

                        char str[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &(in_addr.sin_addr), str, INET_ADDRSTRLEN);
                        if ( !strcasecmp(msg, "CLIENT") )
                        { // if client, fork and handle the requests
                            printf(SET_BLUE("Connection established with Client: %s\n"), str);
                            if ( !fork() )
                            {
                                handle_request_client(in_sockfd, str);
                                exit(0);
                            }
                        } else { // if not server or client, disconnect
                            printf(SET_RED("Unknown connection type. Closing connection to: %s\n"), str);
                            close(in_sockfd);
                        }
                    }                       
                }
                printf(SET_GREEN("Server Exiting.\n"));
            }
        }
    }
}

void handle_request_client(int sockfd, char *addr)
{ // handle client requests
    char msg[MSGSIZE];

    while ( 1 )
    {
        bzero(msg, MSGSIZE);
        printf(SET_BLUE("Waiting for incoming msg from Client at %s...\n"), addr);
        if ( !read(sockfd, msg, MSGSIZE) ) break;
        printf("recv2:%ld\n", strlen(msg));
        if ( strstr(msg, "EXIT") || strstr(msg, "exit") )
        {
            bzero(msg, MSGSIZE);
            sprintf(msg, "Connection terminated with server %s", addr);
            write(sockfd, msg, strlen(msg));
            printf("send1:%ld\n", strlen(msg));
            break;
        } else if ( strstr(msg, "FILE") ) {
            char s[MSGSIZE];
            int blocks=0;
            write(sockfd, msg, strlen(msg));
            printf("send2:%ld\n", strlen(msg));
            strcpy(s, strtok(msg, " "));        // FILE
            strcpy(s, strtok(NULL, " "));       // ext
            strcpy(s, strtok(NULL, " \n"));     // file name
            blocks = atoi(strtok(NULL, " "));   // blocks
            int nbr=0, nbs=0;
            while ( blocks )
            {
                memset(msg, '\0', MSGSIZE);
                //recvfrom(sockfd, msg, MSGSIZE, 0, (struct sockaddr *)&in_addr, (socklen_t *)sizeof(in_addr));
                while ( (nbr = recv(sockfd, msg, 1448, 0)) != 1448 && (blocks-1) ) printf(SET_MAGENTA("%d - %s\n"), blocks, msg);      //read(sockfd, msg, MSGSIZE);
                printf("\t%d - recv3:%ld - %d | ", blocks, strlen(msg), nbr);
                //sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *)&in_addr, sizeof(in_addr));
                nbs = send(sockfd, msg, nbr, MSG_DONTROUTE);  //write(sockfd, msg, strlen(msg));
                printf("send3:%ld - %d\n", strlen(msg), nbs);
                //bzero(msg, MSGSIZE);
                blocks = blocks - 1;
            }
            printf(SET_CYAN("%s\n"), msg);
        }
    }    
    close(sockfd);
    printf(SET_CYAN("Connection terminated with Client %s\n"), addr);
}

void removeSubstring(char *s, const char *toremove)
{ // remove substring from the string
    while( (s = strstr(s, toremove)) ) memmove(s, (s + strlen(toremove)), (1 + strlen(s + strlen(toremove))));
}
