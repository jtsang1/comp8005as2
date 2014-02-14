#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <resolv.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/types.h>

#define SERVER_TCP_PORT 7000	// Default port
#define BUFLEN	80		//Buffer length
#define TRUE	1

int new_sd;

void multi_svr();
void select_svr();
void epoll_svr();

void* Child(void* arg);

int main (int argc,char **argv){

int c;

setbuf(stdout, NULL);

if (argc < 2){
   fprintf(stderr, "Usage: %s -option \n", argv[0]);
   abort();
}

  while ((c = getopt (argc, argv, "hmse")) !=-1) {
		switch (c){
			case 'm':
			printf("running multi\n");
			//param = atoi(optarg);
			multi_svr();

			break;
			case 's':
			printf("running select\n");	
			select_svr();
			break;
			case 'e':
			printf("running epoll\n");		
			epoll_svr();
			break;
			default:
			case 'h':
			fprintf(stderr, "Usage: %s \n", argv[0]);
			break;			
		}
	}


return 0;
}

void multi_svr(){

//int	n, bytes_to_read;
int	sd, port;
struct	sockaddr_in server, client;
//char	*bp, buf[BUFLEN];
pthread_t child;
socklen_t client_len;

	/*switch(argc)
	{
		case 1:
			port = SERVER_TCP_PORT;	// Use the default port
		break;
		case 2:
			port = atoi(argv[1]);	// Get user specified port
		break;
		default:
			fprintf(stderr, "Usage: %s [port]\n", argv[0]);
			exit(1);
	} */

	port = SERVER_TCP_PORT;	// Use the default port

	// Create a stream socket
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror ("Can't create a socket");
		exit(1);
	}

	// Make the server listening socket non-blocking
    	//if (fcntl (sd, F_SETFL, O_NONBLOCK | fcntl (sd, F_GETFL, 0)) == -1) {
	//	perror("fcntl");
	//	exit(1); }
	
	// set SO_REUSEADDR so port can be resused imemediately after exit, i.e., after CTRL-c
    	int arg = TRUE;
    	if (setsockopt (sd, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(arg)) == -1) {
		perror("setsockopt");
		exit(1); }

	// Bind an address to the socket
	bzero((char *)&server, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = htonl(INADDR_ANY); // Accept connections from any client

	if (bind(sd, (struct sockaddr *)&server, sizeof(server)) == -1)
	{
		perror("Can't bind name to socket");
		exit(1);
	}

	// Listen for connections

	// queue up to 5 connect requests
	listen(sd, 5);
	
	printf("Listening...\n");

	client_len= sizeof(client);

	while (TRUE)
	{
		int* new_sd = malloc(sizeof(int));
		if ((*new_sd = accept (sd, (struct sockaddr *)&client, &client_len)) == -1)
		{
			fprintf(stderr, "Can't accept client\n");
			exit(1);
		}

		printf(" Remote Address:  %s\n", inet_ntoa(client.sin_addr));
		
		if ( pthread_create(&child, NULL, Child, (void*) new_sd) != 0 ){
    		perror("Thread creation");
    	} else{
			//printf("Creating thread\n");
       		pthread_join(child,NULL);
			
			free(new_sd);
		}
	}
	close(sd); 
}

void* Child(void* arg)
{   //char line[100];
    int bytes_to_read;
    //int client = *(int *)arg;
    int new_sd = *(int*)arg;	
    char *bp, buf[BUFLEN];
    int n = 0, x = 1;
    bp = buf;
    bytes_to_read = BUFLEN;
	
	while(1){
		while(1){
			n = recv(new_sd, bp, bytes_to_read, 0);
			
			if(n == BUFLEN)
				break;
			else{
				//printf("breaking because n:%d\n",n);
				x = 0;
				break;
			}
		}
		
		if(x == 0)
			break;
	
		printf ("sending:%s\n", buf);
		send(new_sd, buf, BUFLEN, 0);
	}
	
    close(new_sd);
	
	//printf("closing client connection\n");
    return arg;
}

void select_svr(){

}

void epoll_svr(){

}

