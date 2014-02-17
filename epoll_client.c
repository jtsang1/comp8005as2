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
#include <errno.h>
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
	int fd;
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
	static struct epoll_event events[EPOLL_QUEUE_LEN];
	struct epoll_event * event = malloc(sizeof(struct epoll_event) * connections);
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
	if((epoll_fd = epoll_create(EPOLL_QUEUE_LEN)) == -1)
		SystemFatal("epoll_create");
		
	// Create all sockets
	for(i = 0; i < connections; i++){
	
		if((sd[i] = socket(AF_INET, SOCK_STREAM, 0)) == -1)
			SystemFatal("socket");
		
		fprintf(stdout,"socket() - sd: %d\n", sd[i]);
		
		// Set SO_REUSEADDR so port can be reused immediately
		if(setsockopt(sd[i], SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(arg)) == -1)
			SystemFatal("setsockopt");
		
		// Make server socket non-blocking
		if(fcntl(sd[i], F_SETFL, O_NONBLOCK | fcntl(sd[i], F_GETFL, 0)) == -1)
			SystemFatal("fcntl");
		
		// Create data struct for each epoll descriptor
		cdata[i].fd = sd[i];
		cdata[i].total = iterations;
		cdata[i].sent = 0;
		cdata[i].received = 0;
		
		// Add all sockets to epoll event loop
		event[i].events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET;
		event[i].data.ptr = (void *)&cdata[i]; // data is a union type
		
		if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sd[i], &event[i]) == -1)
			SystemFatal("epoll_ctl");
	}
	
	/**********************************************************
	Enter epoll event loop
	**********************************************************/
	int num_fds, f, bytes_to_read, timeout = 1000, fin = 0, e_err = 0, e_hup = 0, e_in = 0, e_out = 0;
	
	char rbuf[BUFLEN], sbuf[BUFLEN];
	
	bytes_to_read = BUFLEN;
	strcpy(sbuf, data);
	
	while(1){
		
		// If all sockets are finished break out of while loop
		fprintf(stdout,"fin: %d e_err: %d e_hup: %d e_in: %d e_out: %d\n", fin, e_err,e_hup,e_in,e_out);
		if(fin == connections)
			break;
		
		num_fds = epoll_wait(epoll_fd, events, EPOLL_QUEUE_LEN, timeout);
		if(num_fds < 0)
			SystemFatal("epoll_wait");
		else if(num_fds == 0)
			break;
			
		fprintf(stdout,"EPOLLHUP - num_fds: %d\n", num_fds);
			
		for(f = 0;f < num_fds;f++){
			
			// EPOLLERR - close socket and continue
			if(events[f].events & EPOLLERR){
				e_err++;
				
				// Retrieve cdata
				struct custom_data * ptr = (struct custom_data *)events[f].data.ptr;
				
				fprintf(stdout,"EPOLLERR - closing fd: %d\n", (*ptr).fd);
				close((*ptr).fd);
				fin++;
				continue;
			}
			
			// EPOLLHUP
			if(events[f].events & EPOLLHUP){
				e_hup++;
				
				// Retrieve cdata
				struct custom_data * ptr = (struct custom_data *)events[f].data.ptr;
				
				printf("EPOLLHUP - fd: %d\n", (*ptr).fd);
				
				// Connect the socket if EPOLLHUP was generated by an unconnected socket
				if((*ptr).sent == 0){
					
					if(connect((*ptr).fd, (struct sockaddr *)&server, sizeof(server)) == -1){
						if(errno == EINPROGRESS) // Only connecting on non-blocking socket
							;
						else
							SystemFatal("connect");
					}
						
		
					printf("Connected: Server: %s\n", hp->h_name);
					//pptr = hp->h_addr_list;
					//printf("IP Address: %s\n", inet_ntop(hp->h_addrtype, *pptr, str, sizeof(str)));
					
				}
				else{
					fprintf(stdout,"EPOLLHUP - closing fd: %d\n", (*ptr).fd);
					close((*ptr).fd);
					fin++;
					continue;
				}
			}
			
			// EPOLLIN
			if(events[f].events & EPOLLIN){
				e_in++;
				
				// Retrieve cdata
				struct custom_data * ptr = (struct custom_data *)events[f].data.ptr;
				
				fprintf(stdout,"EPOLLIN - fd: %d\n", (*ptr).fd);

				// Call recv once for specified amount of data
				recv((*ptr).fd, rbuf, bytes_to_read,0);
								
				// Increment receive counter
				(*ptr).received++;
				printf("(%d) Transmitted and Received: %s\n",getpid(), rbuf);
				printf("ptr.received: %d ptr.total: %d\n",(*ptr).received, (*ptr).total);
				// All messages received, close socket
				if((*ptr).received == (*ptr).total){
					close((*ptr).fd);
					fin++;
					continue;
				}
			}
			
			// EPOLLOUT
			if(events[f].events & EPOLLOUT){
				e_out++;
				
				// Retrieve cdata
				struct custom_data * ptr = (struct custom_data *)events[f].data.ptr;
				
				fprintf(stdout,"EPOLLOUT - fd: %d\n", (*ptr).fd);
				
				// Send one message and increase counter	
				if((*ptr).sent == (*ptr).received && (*ptr).sent < (*ptr).total){
					send((*ptr).fd, sbuf, BUFLEN, 0);
					(*ptr).sent++;
				}
				
				// Remove EPOLL_OUT if all messages are sent
				if((*ptr).sent == (*ptr).total){
					events[f].events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET;
		
					if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, (*ptr).fd, &events[f]) == -1)
						SystemFatal("epoll_ctl1");
				}
			}
			
			//sleep(1);
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
		free(event[i]);
		free(cdata[i]);
	}*/
	
	free(sd);
	free(event);
	free(cdata);
		
	return 0;
}

static void SystemFatal(const char* message)
{
	perror(message);
	exit(EXIT_FAILURE);
}

