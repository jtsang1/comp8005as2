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

#include <ctype.h>
#include <sys/types.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <strings.h>

#define BUFLEN 80

static void SystemFatal(const char* message);

int main (int argc, char ** argv)
{
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
	Create socket
	**********************************************************/
	
	struct sockaddr_in server;
	struct hostent * hp;
	int sd;
	
	if((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		SystemFatal("socket");
	
	bzero((char *)&server, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	if((hp = gethostbyname(host)) == NULL)
		SystemFatal("gethostbyname");
		
	bcopy(hp->h_addr, (char *)&server.sin_addr, hp->h_length);
	
	/**********************************************************
	Connect to server
	**********************************************************/
	
	char ** pptr, str[16];
	
	if(connect(sd, (struct sockaddr *)&server, sizeof(server)) == -1)
		SystemFatal("connect");
		
	printf("Connected: Server: %s\n", hp->h_name);
	pptr = hp->h_addr_list;
	printf("IP Address: %s\n", inet_ntop(hp->h_addrtype, *pptr, str, sizeof(str)));
	printf("Transmit: %s\n", data);
	
	/**********************************************************
	Send and receive data
	**********************************************************/
	
	char rbuf[BUFLEN], sbuf[BUFLEN] = "abcde", * bp;
	int bytes_to_read, n;
	
	//sbuf = data;
	//sbuf = "abcde";
	send(sd, sbuf, BUFLEN, 0);
	
	printf("Receive:\n");
	bp = rbuf;
	bytes_to_read = BUFLEN;
	
	//make repeated calls to recv until there is no more data
	n = 0;
	while((n = recv(sd,bp,bytes_to_read,0)) < BUFLEN)
	{
	
		bp += n;
		bytes_to_read = BUFLEN;
	
	}
	
	printf("%s\n", rbuf);
	fflush(stdout);
	close(sd);
		
	return 0;
}

static void SystemFatal(const char* message)
{
	perror(message);
	exit(EXIT_FAILURE);
}

