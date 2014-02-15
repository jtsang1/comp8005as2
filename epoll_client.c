	/******************************************************************
File: 		epoll_client.c

Usage:		./epoll_client
			-h <server_address>
			-p <int_port>
			-c <int_connections>
			-d <string_data>
			-i <int_iterations>
			
Authors:	Jeremy Tsang
			Kevin Eng
			
Date:		February 10, 2014

Purpose:	COMP 8005 Assignment 2 - Comparing Scalable Servers - 
			threads/select()/epoll()
*******************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <ctype.h>
#include <sys/types.h>

#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <string.h>
#include <sys/wait.h>

#define BUFLEN 80
#define EPOLL_QUEUE_LEN 256 // Must be > 0. Only used for backward compatibility.


static void SystemFatal(const char* message);

struct custom_data{
	int total;		// Total number of messages to send and receive
	int sent;		// Number of messages sent
	int received;	// Number of messages received
};

int main (int argc, char ** argv)
{
	// Remove the need to flush printf with "\n" everytime...
	setbuf(stdout, NULL);

	/**********************************************************
	Parse input parameters
	**********************************************************/
	
	char * host, * data;
	int port, connections, iterations, c;

	int num_params = 0;
	while((c = getopt(argc, argv, "h:p:c:d:i:")) != -1)
	{
		switch(c)
		{
			case 'h':
			host = optarg;
			break;
			case 'p':
			port = atoi(optarg);
			break;
			case 'c':
			connections = atoi(optarg);
			break;
			case 'd':
			data = optarg;
			break;
			case 'i':
			iterations = atoi(
			optarg);
			break;
		}
		num_params++;
	}
	
	if(num_params < 4)
	{
		printf("\n\
Usage: ./epoll_client\n\
-h <address>\t\tSpecify host.\n\
-p <port>\t\tOptionally specify port.\n\
-c <connections>\tNumber of connections to use.\n\
-d <data>\t\tData to send.\n\
-i <iterations>\t\tNumber of iterations to use.\n\n");

		SystemFatal("params");
	}
	else
		printf("You entered -h %s -p %d -c %d -d %s -i %d\n",host, port, connections, data, iterations);
	
	/**********************************************************
	Epoll init. Create all sockets and add to epoll event loop
	**********************************************************/
	
	int i, * sd, arg = 1, epoll_fd;
	sd = malloc(sizeof(int) * connections);
	static struct epoll_event events[EPOLL_QUEUE_LEN], event;
	struct custom_data * cdata = malloc(sizeof(struct custom_data) * connections);
	
	// Initialize server's sockaddr_in and hostent
	struct sockaddr_in server;
	struct hostent * hp;
	memset(&server, 0, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	if((hp = gethostbyname(host)) == NULL)
		SystemFatal("gethostbyname");
	bcopy(hp->h_addr, (char *)&server.sin_addr, hp->h_length);
	
	// Create epoll file descriptor
	if((epoll_fd = epoll_create(EPOLL_QUEUE_LEN)) == -1);
		SystemFatal("epoll_create");
		
	// Create all sockets
	for(i = 0; i < connections; i++){
	
		if((sd[i] = socket(AF_INET, SOCK_STREAM, 0)) == -1)
			SystemFatal("socket");
		
		// Set SO_REUSEADDR so port can be reused immediately
		if(setsockopt(sd[i], SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(arg)) == -1)
			SystemFatal("setsockopt");
		
		// Make server socket non-blocking
		if(fcntl(sd[i], F_SETFL, O_NONBLOCK | fcntl(sd[i], F_GETFL, 0)) == -1)
			SystemFatal("fcntl");
		
		// Create data struct for each epoll descriptor
		cdata[i].total = iterations;
		cdata[i].sent = 0;
		cdata[i].received = 0;
		
		// Add all sockets to epoll event loop
		event.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET;
		event.data.fd = sd[i];
		event.data.ptr = (void *)&cdata[i];
		if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sd[i], &event) == -1)
			SystemFatal("epoll_ctl");
	}
	
	/**********************************************************
	Enter epoll event loop
	**********************************************************/
	int num_fds,f,fin = 0;
	
	char rbuf[BUFLEN], sbuf[BUFLEN], * bp;
	int bytes_to_read, n;
	bp = rbuf;
	bytes_to_read = BUFLEN;
	strcpy(sbuf, data);	
	
	while(1){
		fprintf(stdout,"Finished sockets: %d\n", fin);
		num_fds = epoll_wait(epoll_fd, events, EPOLL_QUEUE_LEN, -1);
		if(num_fds < 0)
			SystemFatal("epoll_wait");
			
		for(f = 0;f < num_fds;f++){
		
			// EPOLLHUP
			if(events[f].events & EPOLLHUP){
				fprintf(stdout,"EPOLLHUP - closing fd: %d\n", events[f].data.fd);
				close(events[f].data.fd);
				continue;
			}
			
			// EPOLLERR
			if(events[f].events & EPOLLERR){
				fprintf(stdout,"EPOLLERR - closing fd: %d\n", events[f].data.fd);
				close(events[f].data.fd);
				continue;
			}
			
			// EPOLLIN
			if(events[f].events & EPOLLIN){
				fprintf(stdout,"EPOLLIN - fd: %d\n", events[f].data.fd);
				
				bp = rbuf;
				bytes_to_read = BUFLEN;

				//make repeated calls to recv until there is no more data
				n = 0;
				while((n = recv(events[f].data.fd,bp,bytes_to_read,0)) < BUFLEN){
					bp += n;
					bytes_to_read -= n;
				}
				
				// Retrieve cdata
				struct custom_data ptr = *(struct custom_data *)events[f].data.ptr;
				
				// Increment receive counter
				ptr.received++;
				printf("(%d) Transmitted and Received: %s\n",getpid(), rbuf);
				
				// All messages received, close socket
				if(ptr.received == ptr.total){
					close(events[f].data.fd);
					fin++;
				}
			}
			
			// EPOLLIN
			if(events[f].events & EPOLLOUT){
				fprintf(stdout,"EPOLLOUT - fd: %d\n", events[f].data.fd);
				
				// Retrieve cdata
				struct custom_data ptr = *(struct custom_data *)events[f].data.ptr;
				
				// Connect the socket if haven't already done so
				if(ptr.sent == 0){
				
					//char ** pptr, str[16];
	
					if(connect(events[f].data.fd, (struct sockaddr *)&server, sizeof(server)) == -1)
						SystemFatal("connect");
		
					printf("Connected: Server: %s\n", hp->h_name);
					//pptr = hp->h_addr_list;
					//printf("IP Address: %s\n", inet_ntop(hp->h_addrtype, *pptr, str, sizeof(str)));
					
				}
				
				// Send one message and increase counter	
				if(ptr.sent < ptr.total){
					send(events[f].data.fd, sbuf, BUFLEN, 0);
					ptr.sent++;
				}
			}
		}
	}
	
	
	
	
	/**********************************************************
	Send and receive data
	**********************************************************/
	
	/*char rbuf[BUFLEN], sbuf[BUFLEN], * bp;
	int bytes_to_read, n;
	
	strcpy(sbuf, data);	
	//sbuf = data;
	//sbuf = "abcde";
	int i;
	
	//Send data <iterations> number of times 
	for(i = 0;i < iterations;i++)
	{
		//printf("Transmit: %s\t", data);
		
		send(sd, sbuf, BUFLEN, 0);
	
		//printf("Receive: ");
		bp = rbuf;
		bytes_to_read = BUFLEN;
	
		//make repeated calls to recv until there is no more data
		n = 0;
		while((n = recv(sd,bp,bytes_to_read,0)) < BUFLEN)
		{
	
			bp += n;
			bytes_to_read -= n;
	
		}
	
		printf("(%d) Transmitted and Received: %s\n",getpid(), rbuf);
	}
	fflush(stdout);
	
	//shutdown(sd, SHUT_RDWR);
	close(sd);*/
	
	/**********************************************************
	End epoll?
	**********************************************************/

	/*for(i = 0; i < connections; i++){
		free(sd[i]);
		free(cdata[i];
	}*/
		
	return 0;
}

static void SystemFatal(const char* message)
{
	perror(message);
	exit(EXIT_FAILURE);
}

