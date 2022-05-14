#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <string>
#include <list>
#include <pthread.h>

#include "RobustIO.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int log_size = 12;
bool flag = false;

std::list<std::string> chatlog;

// Save new msg to the chatlog
void save_msg(std::string msg)
{
	pthread_mutex_lock(&mutex);

	// Only keep a history of 12 msgs
	if (chatlog.size() > log_size)
	{
		chatlog.pop_front();
	}

	chatlog.push_back(msg);

	// New chat log exists, needs to send to all clients
	flag = true;

	pthread_mutex_unlock(&mutex);
}

// Update new chat log to client side
void *broadcast(void *data)
{
	int fd = *(int *)data;

	while (true)
	{
		// Set cancellation point for pthread_cancel()
		pthread_testcancel();

		// Send new chat log to client side only if there is one
		if (flag)
		{
			pthread_mutex_lock(&mutex);

			std::string prev_msgs = "";
			for (auto msg = chatlog.begin(); msg != chatlog.end(); msg++)
			{
				prev_msgs += (*msg);
			}
			RobustIO::write_string(fd, prev_msgs);

			flag = false;

			pthread_mutex_unlock(&mutex);
		}
	}
}

// Create new client
void *create_client(void *data)
{
	int fd = *(int *)data;
	pthread_t write_tid;

	// Create a thread for updating chatlog to client
	if (pthread_create(&write_tid, NULL, broadcast, &fd) != 0)
	{
		printf("Can't create writing thread\n");
		exit(1);
	}

	std::string username = RobustIO::read_string(fd);
	std::string res = "---" + username + " joined the chat---\n";
	printf("%s", res.c_str());

	save_msg(res);

	std::string msg = "";

	while (true)
	{
		msg = RobustIO::read_string(fd);

		if (strcmp(msg.c_str(), "exit") == 0)
		{
			// Close writing thread
			if (pthread_cancel(write_tid) != 0)
			{
				printf("Can't terminate writing thread\n");
				exit(1);
			}
			if (pthread_join(write_tid, NULL) != 0)
			{
				printf("Can't close writing thread\n");
				exit(1);
			}

			close(fd);

			res = "---" + username + " left the chat---\n";
			printf("%s", res.c_str());

			save_msg(res);

			break;
		}

		res = username + ": " + msg + "\n";
		printf("%s", res.c_str());

		save_msg(res);
	}

	pthread_exit(NULL);
}

int main(int argc, char **argv)
{
	int sock, conn;
	int i;
	int rc;
	struct sockaddr address;
	socklen_t addrLength = sizeof(address);
	struct addrinfo hints;
	struct addrinfo *addr;
	char buffer[512];
	int len;

	// Clear the address hints structure
	memset(&hints, 0, sizeof(hints));

	hints.ai_socktype = SOCK_STREAM;			 // TCP
	hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; // IPv4/6, socket is for binding
	// Get address info for local host
	if ((rc = getaddrinfo(NULL, "4321", &hints, &addr)))
	{
		printf("host name lookup failed: %s\n", gai_strerror(rc));
		exit(1);
	}

	// Create a socket
	sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
	if (sock < 0)
	{
		printf("Can't create socket\n");
		exit(1);
	}

	// Set the socket for address reuse, so it doesn't complain about
	// other servers on the machine.
	i = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));

	// Bind the socket
	rc = bind(sock, addr->ai_addr, addr->ai_addrlen);
	if (rc < 0)
	{
		printf("Can't bind socket\n");
		exit(1);
	}

	// Clear up the address data
	freeaddrinfo(addr);

	// Listen for new connections, wait on up to five of them
	rc = listen(sock, 5);
	if (rc < 0)
	{
		printf("listen failed\n");
		exit(1);
	}

	system("clear");
	printf("\n---Server is running---\n");
	printf("\n");

	// Client connected
	while ((conn = accept(sock, (struct sockaddr *)&address, &addrLength)) >= 0)
	{
		pthread_t read_tid;

		// Create a thread for reading input from client
		if (pthread_create(&read_tid, NULL, create_client, &conn) != 0)
		{
			printf("Can't create reading thread\n");
			exit(1);
		}

		if (pthread_detach(read_tid) != 0)
		{
			printf("Can't detach reading thread\n");
			exit(1);
		}
	}
}