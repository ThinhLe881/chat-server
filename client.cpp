#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <list>
#include <pthread.h>
#include <signal.h>

#include "RobustIO.h"

// TODO: No duplicate usernames
// TODO: Fix input stream interrupted by server responses

int sock;
pthread_t tid;

// Update chat log with new messages
void *update_log(void *data)
{
    while (true)
    {
        // Set cancellation point for pthread_cancel()
        pthread_testcancel();

        std::string res = RobustIO::read_string(sock);

        system("clear"); // clear old chat log, replace with new one
        printf("\n--------Welcome to the chatroom!--------\n");
        printf("---Enter 'exit' to exit the chatroom---\n");
        printf("\n");

        printf("%s", res.c_str());
        printf("\n>> ");

        if (fflush(stdout) != 0)
        {
            printf("Can't flush stdout\n");
            exit(1);
        }
    }
}

// Chat closing process
void close_chat()
{
    // Send "exit" msg to server to close the connection in the server side
    RobustIO::write_string(sock, "exit");

    // Close reading thread
    if (pthread_cancel(tid) != 0)
    {
        printf("Can't terminate reading thread\n");
        exit(1);
    }
    if (pthread_join(tid, NULL) != 0)
    {
        printf("Can't close reading thread\n");
        exit(1);
    }

    close(sock);

    printf("\n---Closing the chat---\n");
    printf("\n");

    exit(0);
}

// Catch CTRL+C signal then close the chat
void sighandler(int signum)
{
    close_chat();
}

int main(int argc, char **argv)
{
    struct addrinfo hints;
    struct addrinfo *addr;
    struct sockaddr_in *addrinfo;
    int rc;
    char buffer[512];
    int len;

    // Clear the data structure to hold address parameters
    memset(&hints, 0, sizeof(hints));

    // TCP socket, IPv4/IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_ADDRCONFIG;

    // Get address info for local host
    rc = getaddrinfo("localhost", NULL, &hints, &addr);
    if (rc != 0)
    {
        // Note how we grab errors here
        printf("Hostname lookup failed: %s\n", gai_strerror(rc));
        exit(1);
    }

    // Copy the address info, use it to create a socket
    addrinfo = (struct sockaddr_in *)addr->ai_addr;

    sock = socket(addrinfo->sin_family, addr->ai_socktype, addr->ai_protocol);
    if (sock < 0)
    {
        printf("Can't connect to server\n");
        exit(1);
    }

    // Make sure port is in network order
    addrinfo->sin_port = htons(4321);

    // Connect to the server using the socket and address we resolved
    rc = connect(sock, (struct sockaddr *)addrinfo, addr->ai_addrlen);
    if (rc != 0)
    {
        printf("Connection failed\n");
        exit(1);
    }

    // Clear the address struct
    freeaddrinfo(addr);

    // Start client
    std::string username;

    if (argv[1] != NULL)
        username = argv[1];

    RobustIO::write_string(sock, username);

    std::string msg;
    char buf[1000];

    // Close client when input == CTRL+C
    signal(SIGINT, sighandler);

    // Create reading thread for listening to server responses
    if (pthread_create(&tid, NULL, update_log, NULL) != 0)
    {
        printf("Can't create reading thread\n");
        exit(1);
    }

    bool flag;

    while (true)
    {
        flag = false;

        // Re-prompt if input == empty
        do
        {
            printf(">> ");
            // Close client when input == CTRL+D
            if (fgets(buf, sizeof buf, stdin) == NULL)
            {
                flag = true;
                break;
            }
        } while (buf[0] == '\n');

        if (flag)
            break;

        // Remove trailing newline character from fgets() input
        buf[strcspn(buf, "\n")] = 0;

        // Close client when input == "exit"
        if (strcmp(buf, "exit") == 0)
            break;

        msg = buf;
        RobustIO::write_string(sock, msg);
    }

    // Close client
    close_chat();
}