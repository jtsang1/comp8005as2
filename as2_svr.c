/******************************************************************
File: 		as2_svr.c

Usage:		./as2_svr
                -m <port> Starts the multi-threaded server.
                -s <port> Starts the select() server.
                -e <port> Starts the epoll() server.
			
Authors:	Jeremy Tsang
                Kevin Eng
			
Date:		February 10, 2014

Purpose:	COMP 8005 Assignment 2 - Comparing Scalable Servers - 
                threads/select()/epoll()
 *******************************************************************/

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
#include <assert.h>
#include <sys/epoll.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>

#define SERVER_TCP_PORT 7000    // Default port
#define BUFLEN    800        //Buffer length
#define EPOLL_QUEUE_LEN    256
#define TRUE    1
#define FALSE    0
#define MAX_CONNECTIONS 50000

// Statistics for client

typedef struct {
    char ip_address[32]; // Client IP address in decimals/dots notation
    int port; // Client port	
    int total_conn; // Total connections made
    int conn; // Current connections
    long total_msg; // Total received messages from this client
    long msg; // Current messages
    long total_data; // Total data received from this client
    long data; // Current data
    long send_errors; // Server send errors
    long recv_errors; // Server recv errors
} stats;

// Socket Data
typedef struct {
    char ip_address[32]; // Socket peer address
    int fd; // Socket descriptor
} cinfo;

// Threading Child arguments struct
typedef struct {
    stats * stats; // Socket peer address
    int * fd; // Socket descriptor
} child_info;

//Globals
int fd_server;
stats * server_stats;
int server_stat_len = 0;
pthread_t t1;
int print_debug = 0; // Print debug messages
struct timeval start, end;

// Check if client exists in server_stats

int client_exists(char * address) {

    int c;
    for (c = 0; c < server_stat_len; c++) {
        if (print_debug == 2)
            printf("Comparing [%s] with [%s]\n", address, server_stats[c].ip_address);

        if ((strcmp(address, server_stats[c].ip_address)) == 0) {
            if (print_debug == 1)
                printf("found!\n");
            return c;
        }
    }
    if (print_debug == 1)
        printf("not exist!\n");
    return -1;
}

// Get pointer to client stats

stats * get_client_stats(char * ip_address) {
    if (print_debug == 2)
        printf("get_client_stats:%s!\n", ip_address);
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
    if ((c = client_exists(ip_address)) != -1) {

        if (print_debug == 2)
            printf("found2:%s!\n", ip_address);
        return &server_stats[c];

    } else {

        if ((server_stats = realloc((void *) server_stats, sizeof (stats) * (server_stat_len + 1))) != NULL) {
            server_stat_len++;

            if (print_debug == 2)
                printf("Increased server_stat_len:%d\n", server_stat_len);

            //Initialize server_stats
            memset(server_stats[server_stat_len - 1].ip_address, 0, 32);
            // Copy address to server_stats
            strcpy(server_stats[server_stat_len - 1].ip_address, ip_address);
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

void * print_loop() {
    int c = 0;
    int p1 = 0, p2 = 0;
    long p3 = 0, p4 = 0, p5 = 0, p6 = 0, p7 = 0, p8 = 0;

    int t1 = 0, t2 = 0;
    long t3 = 0, t4 = 0, t5 = 0, t6 = 0, t7 = 0, t8 = 0;

    char line[108];
    for (c = 0; c < 107; c++)
        line[c] = '-';
    line[c] = '\0';

    while (1) {

        gettimeofday(&end, NULL);
        float total_time = (float) (end.tv_sec - start.tv_sec) + ((float) (end.tv_usec - start.tv_usec) / 1000000);
        printf("\nElapsed Time: %.3fs\n", total_time);
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
        printf("%s\n", line);

        if (print_debug == 2)
            printf("server_stat_len:%d\n", server_stat_len);

        for (c = 0; c < server_stat_len; c++) {
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

//FILE fp*;
int new_sd, it;
int fd_server;
int usePort;
int x = 1;

void* multi_svr(int port);
void* select_svr(int port);
void* epoll_svr(int port);

void* Child(void* arg);
static void SystemFatal(const char* message);
static int ClearSocket(int fd, stats * cstat);
void close_server(int);

int main(int argc, char **argv) {

    int c, numparams = 0;

    // Remove the need to flush printf with "\n" everytime...
    setbuf(stdout, NULL);

    while ((c = getopt(argc, argv, "hm:s:e:")) != -1) {
        switch (c) {
            case 'm':
                printf("running multi\n");
                usePort = atoi(optarg);
                multi_svr(usePort);

                break;
            case 's':
                printf("running select\n");
                usePort = atoi(optarg);
                select_svr(usePort);
                break;
            case 'e':
                printf("running epoll\n");
                usePort = atoi(optarg);
                epoll_svr(usePort);
                break;
            default:
            case 'h':
                fprintf(stdout, "\n Usage: ./as2_svr \n\
\n-h\t\tLists help parameters.\n\
-m <port>\tStarts the multi-threaded server.\n\
-s <port>\tStarts the select() server.\n\
-e <port>\tStarts the epoll() server.\n\n ");
                break;
        }
        numparams++;
    }

    if (numparams <= 2) {
        fprintf(stdout, "\n Usage: ./as2_svr \n\
\n-h\t\tLists help parameters.\n\
-m <port>\tStarts the multi-threaded server.\n\
-s <port>\tStarts the select() server.\n\
-e <port>\tStarts the epoll() server.\n\n ");
        exit(1);
    }

    return 0;
}

void* multi_svr(int userPort) {


    int sd, port;
    struct sockaddr_in server, client;
	
    pthread_t child, t1;
    socklen_t client_len;
	
	// Run stat loop
	gettimeofday (&start, NULL);
	pthread_create(&t1, NULL, &print_loop, NULL);

    if (userPort == 0) {

        port = SERVER_TCP_PORT; // Use the default port

    } else {
        port = userPort; // Use defined port
    }

    // Create a stream socket
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Can't create a socket");
        exit(1);
    }

    // Make the server listening socket non-blocking
    //if (fcntl (sd, F_SETFL, O_NONBLOCK | fcntl (sd, F_GETFL, 0)) == -1) {
    //    perror("fcntl");
    //    exit(1); }

    // set SO_REUSEADDR so port can be resused imemediately after exit, i.e., after CTRL-c
    int arg = TRUE;
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof (arg)) == -1) {
        perror("setsockopt");
        exit(1);
    }

    // Bind an address to the socket
    bzero((char *) &server, sizeof (struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY); // Accept connections from any client

    if (bind(sd, (struct sockaddr *) &server, sizeof (server)) == -1) {
        perror("Can't bind name to socket");
        exit(1);
    }

    // Listen for connections

    // queue up to 5 connect requests
    listen(sd, MAX_CONNECTIONS);

    //printf("Listening...\n");

    client_len = sizeof (client);

	stats * cstat;
	
    while (TRUE) {
        int* new_sd = malloc(sizeof (int));
        if ((*new_sd = accept(sd, (struct sockaddr *) &client, &client_len)) == -1) {
            fprintf(stderr, "Can't accept client\n");
            break;
        }
        
        // Get new client stats
        char * ip_address = inet_ntoa(client.sin_addr);
        //printf("CONNECTED TO: %s\n",ip_address);
		if((cstat = get_client_stats(ip_address)) == NULL)
			SystemFatal("get_client_stats");
		
		child_info * cinfo = malloc(sizeof(child_info));
		cinfo->fd = new_sd;
		cinfo->stats = cstat;
		cstat->total_conn++;
		cstat->conn++;
		
		
        //printf("\nNew Connection ------");
        //printf(" Remote Address:  %s\n", inet_ntoa(client.sin_addr));
		
		
		
        if (pthread_create(&child, NULL, Child, (void*) cinfo) != 0) {
            perror("Thread creation");
            break;
        } else {
            //pthread_join(child, NULL);
        }
    }
    
    
    close(sd);
    return NULL;
}

void* Child(void* arg) {
    int bytes_to_read, datSent;
    //int client = *(int *)arg;
    
    int new_sd = *(((child_info *)arg)->fd);
    stats * stats = ((child_info *)arg)->stats;
    
    char *bp, buf[BUFLEN];
    int n = 0, x = 1, t = 0;
    bp = buf;
    bytes_to_read = BUFLEN;

    while (1) {
        while (1) {
            n = recv(new_sd, bp, bytes_to_read, 0);

            if (n == BUFLEN) {
                t++;
                break;
            } else {

                x = 0;
                break;
            }

        }

        if (x == 0) {
            break;
        }

        //printf("sending:%s\n", buf);
        send(new_sd, buf, BUFLEN, 0);
        stats->msg++;
       	stats->total_msg++;
       	stats->data += n;
		stats->total_data += n;
    }

    datSent = t*BUFLEN;
	datSent++;
    //printf("Socket: %d; Num of client requests: %d\n", new_sd, t);
    //printf("Data sent: %d bytes \n", datSent++);
    
    
    stats->conn--;
    close(new_sd);
	free(((child_info *)arg)->fd);
	
	return NULL;
}

void* select_svr(int userPort) {
    int i, maxi, nready, arg, t;
    int listen_sd, new_sd, sockfd, maxfd, client[FD_SETSIZE];
    struct sockaddr_in server, client_addr;
    char buf[BUFLEN];
    ssize_t n;
    fd_set rset, allset;
    socklen_t client_len;
    int port = userPort;

    if (userPort == 0) {

        port = SERVER_TCP_PORT; // Use the default port

    } else {
        port = userPort; // Use the defined port
    }
    printf("%d", FD_SETSIZE);

    // Create a stream socket
    if ((listen_sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        SystemFatal("Cannot Create Socket!");

    // set SO_REUSEADDR so port can be reused immediately after exit, i.e., after CTRL-c
    arg = 1;
    if (setsockopt(listen_sd, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof (arg)) == -1)
        SystemFatal("setsockopt");

    // Bind an address to the socket
    bzero((char *) &server, sizeof (struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY); // Accept connections from any client

    if (bind(listen_sd, (struct sockaddr *) &server, sizeof (server)) == -1)
        SystemFatal("bind error");

    // Listen for connections
    // queue up to LISTENQ connect requests
    listen(listen_sd, MAX_CONNECTIONS);

    maxfd = listen_sd; // initialize
    maxi = -1; // index into client[] array

    for (i = 0; i < FD_SETSIZE; i++)
        client[i] = -1; // -1 indicates available entry
    FD_ZERO(&allset);
    FD_SET(listen_sd, &allset);


    while (TRUE) {
        rset = allset; // structure assignment
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);

        if (FD_ISSET(listen_sd, &rset)) // new client connection
        {
            client_len = sizeof (client_addr);
            if ((new_sd = accept(listen_sd, (struct sockaddr *) &client_addr, &client_len)) == -1)
                SystemFatal("accept error");

            printf(" Remote Address:  %s\n", inet_ntoa(client_addr.sin_addr));

            for (i = 0; i < FD_SETSIZE; i++)
                if (client[i] < 0) {
                    client[i] = new_sd; // save descriptor
                    break;
                }
            if (i == FD_SETSIZE) {
                printf("Too many clients\n");
                exit(1);
            }

            FD_SET(new_sd, &allset); // add new descriptor to set
            if (new_sd > maxfd)
                maxfd = new_sd; // for select

            if (i > maxi)
                maxi = i; // new max index in client[] array

            if (--nready <= 0)
                continue; // no more readable descriptors
        }

        for (i = 0; i <= maxi; i++) // check all clients for data
        {
            if ((sockfd = client[i]) < 0)
                continue;

            if (FD_ISSET(sockfd, &rset)) {
                n = read(sockfd, buf, BUFLEN);
                if (n == BUFLEN) {
                    write(sockfd, buf, BUFLEN); // echo to client
                    //printf("(sockfd: %d)Sending: %s\n", sockfd, buf);

                }

                if (n == 0) // connection closed by client
                {
                    //printf("(sockfd: %d) Remote Address:  %s closed connection\n", sockfd, inet_ntoa(client_addr.sin_addr));
                    close(sockfd);
                    FD_CLR(sockfd, &allset);
                    client[i] = -1;
                    t++;
                }
                if (n == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        ;
                    } else {
                        perror("read fail");
                    }
                }
                //write statement to file
                //int datSent = i*BUFLEN;
                //printf("Socket: %d; Num of client requests: %d\n", sockfd, t);
                //printf("Data sent: %d bytes \n", datSent);
            }
        }
    }
    //printf("made it here");
}

void* epoll_svr(int userPort) {

    gettimeofday(&start, NULL);

    // Start the server stats loop
    pthread_create(&t1, NULL, &print_loop, NULL);

    int i, arg;
    int num_fds, epoll_fd;
    static struct epoll_event events[EPOLL_QUEUE_LEN], event;
    int port = userPort;
    struct sockaddr_in addr;
    struct sigaction act;

    if (userPort == 0) {

        port = SERVER_TCP_PORT; // Use the default port

    } else {
        port = userPort; // Use the default port
    }

    // set up the signal handler to close the server socket when CTRL-c is received
    act.sa_handler = close_server;
    act.sa_flags = 0;
    if ((sigemptyset(&act.sa_mask) == -1 || sigaction(SIGINT, &act, NULL) == -1)) {
        perror("Failed to set SIGINT handler");
        exit(EXIT_FAILURE);
    }

    // Create the listening socket
    fd_server = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_server == -1)
        SystemFatal("socket");

    // set SO_REUSEADDR so port can be resused imemediately after exit, i.e., after CTRL-c
    arg = 1;
    if (setsockopt(fd_server, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof (arg)) == -1)
        SystemFatal("setsockopt");

    // Make the server listening socket non-blocking
    if (fcntl(fd_server, F_SETFL, O_NONBLOCK | fcntl(fd_server, F_GETFL, 0)) == -1)
        SystemFatal("fcntl");

    // Bind to the specified listening port
    memset(&addr, 0, sizeof (struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(fd_server, (struct sockaddr*) &addr, sizeof (addr)) == -1)
        SystemFatal("bind");

    // Listen for fd_news; SOMAXCONN is 128 by default
    if (listen(fd_server, SOMAXCONN) == -1)
        SystemFatal("listen");

    // Create the epoll file descriptor
    epoll_fd = epoll_create(EPOLL_QUEUE_LEN);
    if (epoll_fd == -1)
        SystemFatal("epoll_create");

    // Add the server socket to the epoll event loop
    event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET;

    cinfo server_cinfo;
    server_cinfo.fd = fd_server;
    event.data.ptr = (void *) &server_cinfo;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd_server, &event) == -1)
        SystemFatal("epoll_ctl");

    stats * cstat;

    // Execute the epoll event loop
    while (TRUE) {
        //struct epoll_event events[MAX_EVENTS];
        num_fds = epoll_wait(epoll_fd, events, EPOLL_QUEUE_LEN, -1);
        if (num_fds < 0)
            SystemFatal("epoll_wait");

        for (i = 0; i < num_fds; i++) {

            // EPOLLHUP
            if (events[i].events & EPOLLHUP) {
                // Get socket cinfo
                cinfo * c_ptr = (cinfo *) events[i].data.ptr;

                if (print_debug == 1)
                    fprintf(stdout, "EPOLLHUP - closing fd: %d\n", c_ptr->fd);

                // Get client stats
                if ((cstat = get_client_stats(c_ptr->ip_address)) == NULL)
                    SystemFatal("get_client_stats");

                close(c_ptr->fd);
                //free(c_ptr);

                // Update stats
                cstat->conn--;

                continue;
            }

            // EPOLLERR
            if (events[i].events & EPOLLERR) {
                // Get socket cinfo
                cinfo * c_ptr = (cinfo *) events[i].data.ptr;

                if (print_debug == 1)
                    fprintf(stdout, "EPOLLERR - closing fd: %d\n", c_ptr->fd);

                // Get client stats
                if ((cstat = get_client_stats(c_ptr->ip_address)) == NULL)
                    SystemFatal("get_client_stats");

                close(c_ptr->fd);
                //free(c_ptr);

                // Update stats
                cstat->conn--;

                continue;
            }

            assert(events[i].events & EPOLLIN);

            // EPOLLIN
            if (events[i].events & EPOLLIN) {

                // Get socket cinfo
                cinfo * c_ptr = (cinfo *) events[i].data.ptr;

                // Server is receiving one or more incoming connection requests
                if (c_ptr->fd == fd_server) {

                    if (print_debug == 1)
                        printf("EPOLLIN-connect fd:%d\n", c_ptr->fd);

                    while (1) {

                        struct sockaddr_in in_addr;
                        socklen_t in_len;
                        int fd_new = 0;

                        memset(&in_addr, 1, sizeof (struct sockaddr_in));
                        fd_new = accept(fd_server, (struct sockaddr *) &in_addr, &in_len);
                        if (fd_new == -1) {
                            // If error in accept call
                            if (errno != EAGAIN && errno != EWOULDBLOCK)
                                perror("accept");
                            // All connections have been processed
                            break;
                        }
                        char * ip_address = inet_ntoa(in_addr.sin_addr);
                        if (print_debug == 2)
                            printf("CONNECTED TO: %s\n", ip_address);

                        // Get new client stats
                        if ((cstat = get_client_stats(ip_address)) == NULL)
                            SystemFatal("get_client_stats");

                        // Update stats
                        cstat->conn++;
                        cstat->total_conn++;

                        if (print_debug == 1)
                            printf("total_conn:%d\n", cstat->total_conn);

                        if (print_debug == 1)
                            fprintf(stdout, "ACCEPTED NEW fd: %d\n", fd_new);

                        // Make the fd_new non-blocking
                        if (fcntl(fd_new, F_SETFL, O_NONBLOCK | fcntl(fd_new, F_GETFL, 0)) == -1)
                            SystemFatal("fcntl");

                        // Add the new socket descriptor to the epoll loop
                        if (print_debug == 1)
                            fprintf(stdout, "ADDING TO EPOLL fd: %d\n", fd_new);

                        //struct epoll_event * client_event = malloc(sizeof(struct epoll_event));
                        event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLET;

                        cinfo * client_info = malloc(sizeof (cinfo));
                        client_info->fd = fd_new;
                        strcpy(client_info->ip_address, ip_address);
                        event.data.ptr = (void *) client_info;

                        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd_new, &event) == -1)
                            SystemFatal("epoll_ctl");

                        if (print_debug == 2)
                            printf(" Remote Address:  %s fd:%d\n", ip_address, ((cinfo*) event.data.ptr)->fd);
                        continue;
                    }
                }// Else one of the sockets has read data
                else {

                    if (print_debug == 1)
                        printf("EPOLLIN-read fd:%d\n", c_ptr->fd);

                    // Get client stats
                    if ((cstat = get_client_stats(c_ptr->ip_address)) == NULL)
                        SystemFatal("get_client_stats2");

                    if (!ClearSocket(c_ptr->fd, cstat)) {

                        // epoll will remove the fd from its set
                        // automatically when the fd is closed
                        if (print_debug == 1)
                            fprintf(stdout, "CLOSING3 fd: %d\n", c_ptr->fd);

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
    exit(EXIT_SUCCESS);
}

static int ClearSocket(int fd, stats * cstat) {
    int n, l, bytes_to_read, m = 0;
    char *bp, buf[BUFLEN];

    bp = buf;
    bytes_to_read = BUFLEN;

    // Edge-triggered event will only notify once, so we must
    // read everything in the buffer
    while (1) {
        n = recv(fd, bp, bytes_to_read, 0);

        // Read fixed size message and echo back
        if (n == BUFLEN) {
            m++;
            if (print_debug == 1)
                printf("sending:%s\n", buf);
            l = send(fd, buf, BUFLEN, 0);
            if (l == -1) {
                cstat->send_errors++;
            }

            // Update stats
            cstat->msg++;
            cstat->total_msg++;
            cstat->data += n;
            cstat->total_data += n;
        }// No more messages or read error
        else if (n == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("recv");
                // Update stats
                cstat->recv_errors++;
            }

            break;
        }// Wrong message size or zero-length message
            // Stream socket peer has performed an orderly shutdown
        else {
            break;
        }
    }

    if (print_debug == 1)
        printf("sending m:%d\n", m);

    if (m == 0)
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
    perror(message);
    exit(EXIT_FAILURE);
}

// Server closing function, signalled by CTRL-C

void close_server(int signo) {
    if (print_debug == 1)
        printf("\n\nDone here\n\n");

    // Close down thread, server socket
    pthread_kill(t1, 0);
    close(fd_server);

    exit(EXIT_SUCCESS);
}

