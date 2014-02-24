/*---------------------------------------------------------------------------------------
--	SOURCE FILE:	epoll_svr.c -   A simple echo server using the epoll API
--
--	PROGRAM:		epolls
--					gcc -Wall -ggdb -o epolls epoll_svr.c  
--
--	FUNCTIONS:		Berkeley Socket API
--
--	DATE:			February 2, 2008
--
--	REVISIONS:		(Date and Description)
--
--	DESIGNERS:		Design based on various code snippets found on C10K links
--					Modified and improved: Aman Abdulla - February 2008
--
--	PROGRAMMERS:	Aman Abdulla
--
--	NOTES:
--	The program will accept TCP connections from client machines.
-- 	The program will read data from the client socket and simply echo it back.
--	Design is a simple, single-threaded server using non-blocking, edge-triggered
--	I/O to handle simultaneous inbound connections. 
--	Test with accompanying client application: epoll_clnt.c
---------------------------------------------------------------------------------------*/

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>

#define TRUE 				1
#define FALSE 				0
#define EPOLL_QUEUE_LEN		256
#define BUFLEN				800
#define SERVER_PORT			7000

// Statistics for client
typedef struct{
	char ip_address[32];// Client IP address in decimals/dots notation
	int port;			// Client port	
	int	total_conn;		// Total connections made
	int conn;			// Current connections
	long total_msg;		// Total received messages from this client
	long msg;			// Current messages
	long total_data;	// Total data received from this client
	long data;			// Current data
	long send_errors;	// Server send errors
	long recv_errors;	// Server recv errors
}stats;

// Socket Data
typedef struct{
	char ip_address[32];// Socket peer address
	int fd;				// Socket descriptor
}cinfo;

//Globals
int fd_server;
stats * server_stats;
int server_stat_len = 0;
pthread_t t1;
int print_debug = 0; 	// Print debug messages
struct timeval start, end;

// Check if client exists in server_stats
int client_exists(char * address){

	int c;
	for(c = 0;c < server_stat_len;c++){
		if(print_debug == 2)
			printf("Comparing [%s] with [%s]\n",address, server_stats[c].ip_address);
			
		if((strcmp(address, server_stats[c].ip_address)) == 0){
			if(print_debug == 1)
				printf("found!\n");
			return c;
		}
	}	
	if(print_debug == 1)
		printf("not exist!\n");
	return -1;
}

// Get pointer to client stats
stats * get_client_stats(char * ip_address){
	if(print_debug == 2)
		printf("get_client_stats:%s!\n",ip_address);
	// Get client's sockaddr_in
	/*struct sockaddr addr;
	socklen_t size = sizeof(struct sockaddr);
	if((getpeername(socket, &addr, &size)) == -1){
		perror("getpeername");
		return NULL;
	}
	
	struct sockaddr_in * sin;
	if(addr.sa_family == AF_INET){
		sin = (struct sockaddr_in *)&addr;
	}
	else
		return NULL;
	*/
	
	// Check if client exists. If so return it, otherwise
	// create a new client and return it.
	
	int c;
	//if((c = client_exists(inet_ntoa(sin->sin_addr))) != -1){
	if((c = client_exists(ip_address)) != -1){
		
		if(print_debug == 2)
			printf("found2:%s!\n",ip_address);
		return &server_stats[c];
		
	}
	else{
		
		if((server_stats = realloc((void *)server_stats, sizeof(stats) * (server_stat_len + 1))) != NULL){
			server_stat_len++;
			
			if(print_debug == 2)
				printf("Increased server_stat_len:%d\n", server_stat_len);
			
			//Initialize server_stats
			memset (server_stats[server_stat_len - 1].ip_address, 0, 32);
			// Copy address to server_stats
			strcpy(server_stats[server_stat_len - 1].ip_address,ip_address);
			server_stats[server_stat_len - 1].total_conn = 0;
			server_stats[server_stat_len - 1].conn = 0;
			server_stats[server_stat_len - 1].total_msg = 0;
			server_stats[server_stat_len - 1].msg = 0;
			server_stats[server_stat_len - 1].total_data = 0;
			server_stats[server_stat_len - 1].data = 0;
			server_stats[server_stat_len - 1].send_errors = 0;
			server_stats[server_stat_len - 1].recv_errors = 0;
			
			return &server_stats[server_stat_len - 1];
		}
	}
	return NULL;
}

// Print live performance statistics while server is running
// This is called in a loop
void * print_loop(){
	int c = 0;
	int p1 = 0,p2 = 0;
	long p3 = 0,p4 = 0,p5 = 0,p6 = 0, p7 = 0,p8 = 0;
	
	int t1 = 0, t2 = 0;
	long t3 = 0,t4 = 0,t5 = 0,t6 = 0,t7 = 0,t8 = 0;
	
	char line[108];
	for(c = 0;c < 107;c++)
		line[c] = '-';
	line[c] = '\0';
	
	while(1){
	
		gettimeofday (&end, NULL);	
		float total_time = (float)(end.tv_sec - start.tv_sec) + ((float)(end.tv_usec - start.tv_usec)/1000000);
		printf("\nElapsed Time: %.3fs\n",total_time);
		printf("%-14s%-14s%-14s%-14s%-14s%-14s%-14s%-5s%-5s\n",\
		"Clients",\
		"TotalConn",\
		"ActiveConn",\
		"RecvMsg",\
		"Msg/s",\
		"RecvByte",\
		"Byte/s",\
		"RxEr",\
		"TxEr");
		printf("%s\n",line);
		
		if(print_debug == 2)
			printf("server_stat_len:%d\n",server_stat_len);
		
		for(c = 0;c < server_stat_len;c++){
			// Pre stats
			t1 += p1 = server_stats[c].total_conn;
			t2 += p2 = server_stats[c].conn;
			
			t4 += p4 = server_stats[c].msg;
			t3 += p3 = server_stats[c].total_msg;
			
			t6 += p6 = server_stats[c].data;
			t5 += p5 = server_stats[c].total_data;
			
			t7 += p7 = server_stats[c].recv_errors;
			t8 += p8 = server_stats[c].send_errors;
			
			// Print stats
			printf("%-14s%-14d%-14d%-14ld%-14ld%-14ld%-14ld%-5ld%-5ld\n",\
			server_stats[c].ip_address,\
			p1,\
			p2,\
			p3,\
			p4,\
			p5,\
			p6,\
			p7,\
			p8);
			
			// Post stats
			server_stats[c].msg = 0;
			server_stats[c].data = 0;
		}
		
		// Print totals
		printf("%-14s%-14d%-14d%-14ld%-14ld%-14ld%-14ld%-5ld%-5ld\n\n",\
		"Total",\
		t1,\
		t2,\
		t3,\
		t4,\
		t5,\
		t6,\
		t7,\
		t8);

		// Reset totals
		t1 = 0;
		t2 = 0;
		t3 = 0;
		t4 = 0;
		t5 = 0;
		t6 = 0;
		t7 = 0;
		t8 = 0;
		
		
	
		sleep(1);
	}
}

// Function prototypes
static void SystemFatal (const char* message);
static int ClearSocket (int fd, stats * cstat);
void close_server (int);

int main (int argc, char* argv[]) {

	gettimeofday (&start, NULL);
	
	// Start the server stats loop
	pthread_create(&t1, NULL, &print_loop, NULL);

	int i, arg; 
	int num_fds, epoll_fd;
	static struct epoll_event events[EPOLL_QUEUE_LEN], event;
	int port = SERVER_PORT;
	struct sockaddr_in addr;
	struct sigaction act;
	
	// set up the signal handler to close the server socket when CTRL-c is received
    act.sa_handler = close_server;
    act.sa_flags = 0;
    if ((sigemptyset (&act.sa_mask) == -1 || sigaction (SIGINT, &act, NULL) == -1)){
        perror ("Failed to set SIGINT handler");
        exit (EXIT_FAILURE);
    }
	
	// Create the listening socket
	fd_server = socket (AF_INET, SOCK_STREAM, 0);
	if (fd_server == -1) 
	SystemFatal("socket");
	
	// set SO_REUSEADDR so port can be resused imemediately after exit, i.e., after CTRL-c
	arg = 1;
	if (setsockopt (fd_server, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(arg)) == -1) 
	SystemFatal("setsockopt");
	
	// Make the server listening socket non-blocking
	if (fcntl (fd_server, F_SETFL, O_NONBLOCK | fcntl (fd_server, F_GETFL, 0)) == -1) 
	SystemFatal("fcntl");
	
	// Bind to the specified listening port
	memset (&addr, 0, sizeof (struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);
	if (bind (fd_server, (struct sockaddr*) &addr, sizeof(addr)) == -1) 
	SystemFatal("bind");
	
	// Listen for fd_news; SOMAXCONN is 128 by default
	if (listen (fd_server, SOMAXCONN) == -1) 
	SystemFatal("listen");
	
	// Create the epoll file descriptor
	epoll_fd = epoll_create(EPOLL_QUEUE_LEN);
	if (epoll_fd == -1) 
	SystemFatal("epoll_create");
	
	// Add the server socket to the epoll event loop
	event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET;
	
	cinfo server_cinfo;
	server_cinfo.fd = fd_server;
	event.data.ptr = (void *)&server_cinfo;
	
	if (epoll_ctl (epoll_fd, EPOLL_CTL_ADD, fd_server, &event) == -1) 
	SystemFatal("epoll_ctl");
    
    stats * cstat;
    
	// Execute the epoll event loop
	while (TRUE){
		//struct epoll_event events[MAX_EVENTS];
		num_fds = epoll_wait (epoll_fd, events, EPOLL_QUEUE_LEN, -1);
		if (num_fds < 0)
			SystemFatal ("epoll_wait");

		for (i = 0; i < num_fds; i++){
			
    		// EPOLLHUP
    		if (events[i].events & EPOLLHUP){
    			// Get socket cinfo
    			cinfo * c_ptr = (cinfo *)events[i].data.ptr;
    		
				if(print_debug == 1)
					fprintf(stdout,"EPOLLHUP - closing fd: %d\n", c_ptr->fd);
				
				// Get client stats
				if((cstat = get_client_stats(c_ptr->ip_address)) == NULL)
					SystemFatal("get_client_stats");
				
				close(c_ptr->fd);
				//free(c_ptr);
				
				// Update stats
				cstat->conn--;
				
				continue;
			}
			
			// EPOLLERR
			if (events[i].events & EPOLLERR){
				// Get socket cinfo
    			cinfo * c_ptr = (cinfo *)events[i].data.ptr;
			
				if(print_debug == 1)
					fprintf(stdout,"EPOLLERR - closing fd: %d\n", c_ptr->fd);
				
				// Get client stats
				if((cstat = get_client_stats(c_ptr->ip_address)) == NULL)
					SystemFatal("get_client_stats");
				
				close(c_ptr->fd);
				//free(c_ptr);
				
				// Update stats
				cstat->conn--;
				
				continue;
			}
			
    		assert (events[i].events & EPOLLIN);
    						
    		// EPOLLIN
    		if (events[i].events & EPOLLIN){
    		
    			// Get socket cinfo
    			cinfo * c_ptr = (cinfo *)events[i].data.ptr;
    			
    			// Server is receiving one or more incoming connection requests
				if (c_ptr->fd == fd_server){
					
					if(print_debug == 1)
						printf("EPOLLIN-connect fd:%d\n",c_ptr->fd);
				
					while(1){
								
						struct sockaddr_in in_addr;
						socklen_t in_len;
						int fd_new = 0;
						
						memset (&in_addr, 1, sizeof (struct sockaddr_in));
						fd_new = accept(fd_server, (struct sockaddr *)&in_addr, &in_len);
						if (fd_new == -1){
							// If error in accept call
							if (errno != EAGAIN && errno != EWOULDBLOCK)
								perror("accept");
							// All connections have been processed
							break;
						}
						char * ip_address = inet_ntoa(in_addr.sin_addr);
						if(print_debug == 2)
							printf("CONNECTED TO: %s\n",ip_address);
						
						// Get new client stats
						if((cstat = get_client_stats(ip_address)) == NULL)
							SystemFatal("get_client_stats");
					
						// Update stats
						cstat->conn++;
						cstat->total_conn++;
						
						if(print_debug == 1)
							printf("total_conn:%d\n",cstat->total_conn);
					
						if(print_debug == 1)
							fprintf(stdout,"ACCEPTED NEW fd: %d\n", fd_new);

						// Make the fd_new non-blocking
						if (fcntl (fd_new, F_SETFL, O_NONBLOCK | fcntl(fd_new, F_GETFL, 0)) == -1) 
							SystemFatal("fcntl");
				
						// Add the new socket descriptor to the epoll loop
						if(print_debug == 1)
							fprintf(stdout,"ADDING TO EPOLL fd: %d\n", fd_new);
						
						//struct epoll_event * client_event = malloc(sizeof(struct epoll_event));
						event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET;
						
						cinfo * client_info = malloc(sizeof(cinfo));
						client_info->fd = fd_new;
						strcpy(client_info->ip_address,ip_address);
						event.data.ptr = (void *)client_info;
						
						if (epoll_ctl (epoll_fd, EPOLL_CTL_ADD, fd_new, &event) == -1) 
							SystemFatal ("epoll_ctl");
			
						if(print_debug == 2)
							printf(" Remote Address:  %s fd:%d\n", ip_address, ((cinfo*)event.data.ptr)->fd);
						continue;
					}
				}
				// Else one of the sockets has read data
				else{
				
					if(print_debug == 1)
						printf("EPOLLIN-read fd:%d\n",c_ptr->fd);
					
					// Get client stats
					if((cstat = get_client_stats(c_ptr->ip_address)) == NULL)
						SystemFatal("get_client_stats2");
					
					if (!ClearSocket(c_ptr->fd, cstat)){
				
						// epoll will remove the fd from its set
						// automatically when the fd is closed
						if(print_debug == 1)
							fprintf(stdout,"CLOSING3 fd: %d\n", c_ptr->fd);
					
						close(c_ptr->fd);
						//free(c_ptr);
					
						// Update stats
						cstat->conn--;
					}
				}
			}
		}
	}
	
	close(fd_server);
	exit (EXIT_SUCCESS);
}

static int ClearSocket (int fd, stats * cstat) {
	int	n,l, bytes_to_read, m = 0;
	char	*bp, buf[BUFLEN];
		
	bp = buf;
	bytes_to_read = BUFLEN;
	
	// Edge-triggered event will only notify once, so we must
	// read everything in the buffer
	while(1){
		n = recv (fd, bp, bytes_to_read, 0);
	
		// Read fixed size message and echo back
		if(n == BUFLEN){
			m++;
			if(print_debug == 1)
				printf ("sending:%s\n", buf);
			l = send(fd, buf, BUFLEN, 0);
			if(l == -1){
				cstat->send_errors++;
			}
			
			// Update stats
			cstat->msg++;
			cstat->total_msg++;
			cstat->data += n;
			cstat->total_data += n;
		}
		// No more messages or read error
		else if(n == -1){
			if(errno != EAGAIN && errno != EWOULDBLOCK){
				perror("recv");
				// Update stats
				cstat->recv_errors++;
			}
			
			break;
		}
		// Wrong message size or zero-length message
		// Stream socket peer has performed an orderly shutdown
		else{
			break;
		}
	}
	
	if(print_debug == 1)
		printf ("sending m:%d\n", m);
	
	if(m == 0)
		return FALSE;
	else
		return TRUE;
	/*
	while ((n = recv (fd, bp, bytes_to_read, 0)) > 0)
	{
		bp += n;
		bytes_to_read -= n;
		m++;
	}
	
	if(m == 0)
		return FALSE;
	
	//printf ("sending:%s\tloops:%d\n", buf, m);
	printf ("sending:%s\n", buf);

	send (fd, buf, BUFLEN, 0);
	//close (fd);
	return TRUE;*/
}

// Prints the error stored in errno and aborts the program.
static void SystemFatal(const char* message) {
    perror (message);
    exit (EXIT_FAILURE);
}

// Server closing function, signalled by CTRL-C
void close_server (int signo){
	if(print_debug == 1)
		printf("\n\nDone here\n\n");
	
	// Close down thread, server socket
	pthread_kill(t1,0);
    close(fd_server);
    
	exit (EXIT_SUCCESS);
}

