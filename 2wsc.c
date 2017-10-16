#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <sys/types.h>
#include <pthread.h>
#include <signal.h>
 
pthread_t tranid;
/* this is needed here to allow the read&print thread to let the send thread know
 * that the other end of the connection has closed, since fgets blocks so
 * the send thread won't be able to check for that itself.
 */
 
void usageerr()
{
	fprintf(stderr, "invalid or missing options\n\
usage: snc [-l] [-u] [hostname] port\n");
}


void internerr()
{
	fprintf(stderr, "internal error\n");
}

void closethread(int signum)
{
	pthread_exit(NULL);
}

/* int handleprinting()
 * handles printing of messages
 */
void* handleprinting(void* fd)
{
	signal(SIGUSR1, closethread);
	
	int* sockfd = (int*) fd;
	char buf[1024];
	while(1)
	{
		bzero(buf, 1024);
		int input = recv(*sockfd, buf, 1024, 0);
		if(input == 0) // socket closed
		{
			printf("connection terminated by other end\n");
			pthread_kill(tranid, SIGUSR1);
			break;
		}
        buf[input] = '\0';
        fputs(buf, stdout);
	}
	
    pthread_exit(0);
}

/* int handletransmission()
 * handles the actual transmitting of messages
 */
void* handletransmission(void* fd)
{
	int* sockfd = (int*) fd;
	char buf[1024];
	while(1)
	{
		if(write(*sockfd, "", 0) < 0)
		{
			break;
		}
	    
		char* input = fgets(buf, 1024, stdin);
        
		int n = send(*sockfd, buf, strlen(buf), 0);
        
		if(n < 0)
			break;
		
		if(input == NULL) // ctrl-d was pressed
		{
			 printf("closing connection\n");
			 break;
		}
	}

    pthread_exit(0);
	
}

/* int handlenc(int l, int udp, char* hostname, unsigned int port)
 * handles the connections, then hands off actual message transmission to
 * other functions
 */
int handlenc(int l, int udp, char* hostname, unsigned int port)
{
	if(udp == 0) // create connection using TCP
	{
		if(l == 1)
		{
			
			int sockfd, newfd;
			sockfd = socket(AF_INET, SOCK_STREAM, 0);
			
			struct sockaddr_in servadd;
			bzero(&servadd, sizeof(servadd));
			
			// set up the socket address struct
			servadd.sin_family = AF_INET;
			servadd.sin_port = htons(port);
			servadd.sin_addr.s_addr = INADDR_ANY;
			
			if(bind(sockfd, (struct sockaddr*) &servadd, sizeof(servadd)) != 0)
			{
				close(sockfd);
				internerr();
				return 1;
			}
			else
				printf("socket bound, listening...\n");
			
			listen(sockfd, 3);
			struct sockaddr_in clientadd;
			int clientlen = sizeof(clientadd);
			
			if((newfd = accept(sockfd, (struct sockaddr*) &clientadd,
					&clientlen)) < 0)
			{
				internerr();
				return 1;
			}
			else
				printf("connected!\n");
			
			pthread_create(&tranid, NULL, handletransmission, &newfd);
			pthread_t printid;
			pthread_create(&printid, NULL, handleprinting, &newfd);
			
			pthread_join(tranid, NULL);

			
			close(newfd);
			close(sockfd);
			printf("connection closed, exiting...\n");
			return 0;
		}
		else //running as client
		{
			int sockfd;
			
			struct sockaddr_in servadd;
			struct hostent* serv;
			
			sockfd = socket(AF_INET, SOCK_STREAM, 0);
			
			if((serv = gethostbyname(hostname)) == NULL)
			{
				printf("not a host name\n");
				internerr();
				return 1;
			}
			
			// set up servadd
			servadd.sin_family = AF_INET;
			servadd.sin_port = htons(port);
			bcopy((char*) serv->h_addr, (char*) &servadd.sin_addr.s_addr,
				serv->h_length);
			
			if(connect(sockfd, (struct sockaddr*) &servadd, sizeof(servadd)) < 0)
			{
				printf("couldn't connect\n");
				internerr();
				return 1;
			}
			else
				printf("connected!\n");
			
			pthread_create(&tranid, NULL, handletransmission, &sockfd);
			pthread_t printid;
			pthread_create(&printid, NULL, handleprinting, &sockfd);
			
			
			pthread_join(tranid, NULL);
					
			close(sockfd);
			printf("connection closed, exiting...\n");
			return 0;
		}
	}
	else // create connection using UDP
	{
		if(l == 1)
		{
			int sockfd, newfd;
			sockfd = socket(AF_INET, SOCK_DGRAM, 0);
			struct sockaddr_in servadd;
			struct sockaddr_in newadd;
			socklen_t newaddl;
			bzero(&servadd, sizeof(servadd));
			
			// set up the socket address struct
			servadd.sin_family = AF_INET;
			servadd.sin_port = htons(port);
			servadd.sin_addr.s_addr = INADDR_ANY; //server runs on local always
			
			if(bind(sockfd, (struct sockaddr*) &servadd, sizeof(servadd)) != 0)
			{
				close(sockfd);
				internerr();
				return 1;
			}
			printf("socket bound, listening...\n");
			char buf[1024];
			int n = recvfrom(sockfd, buf, 1024, 0, (struct sockaddr*) &newadd,
						&newaddl);
			// populates newadd sockaddr struct
			buf[n] = '\0';
			fputs(buf, stdout);
			sleep(1);
			
			if(fork() > 0)
			{
				while(1)
				{
					bzero(buf, 1024);
					int n = recvfrom(sockfd, buf, 1024, 0, 0, 0);
					buf[n] = '\0';
					fputs(buf, stdout);
				}
			}
			else
			{
				while(1)
				{
						bzero(buf, 1024);
						char* input = fgets(buf, 1024, stdin);
						
						if(input == 0)
						{
							printf("message transmission ended\n");
							break;
						}
						
						int sentn = sendto(sockfd, buf, strlen(buf), 0, 
							(struct sockaddr*) &newadd, newaddl);

				}
			}
			
		}
		
		else //running as client
		{
			int sockfd;
			sockfd = socket(AF_INET, SOCK_DGRAM, 0);
			struct hostent* serv;
			struct sockaddr_in servadd, cliadd;

			if((serv = gethostbyname(hostname)) == NULL)
			{
				printf("not a host name\n");
				internerr();
				return 1;
			}
			
			// set up servadd
			servadd.sin_family = AF_INET;
			servadd.sin_port = htons(port);
			bcopy((char*) serv->h_addr, (char*) &servadd.sin_addr.s_addr,
				serv->h_length);
			// set up cliadd
			cliadd.sin_family = AF_INET;
			cliadd.sin_port = 0;
			cliadd.sin_addr.s_addr = INADDR_ANY;

			
			if(bind(sockfd, (struct sockaddr*) &cliadd, sizeof(cliadd)) != 0)
			{
				close(sockfd);
				internerr();
				return 1;
			}
			
			printf("connection configured; input message to send to server\n");
			
			if(fork() > 0)
			{
				char buf[1024];
				while(1)
				{
					bzero(buf, 1024);
					int n = recvfrom(sockfd, buf, 1024, 0, 0, 0);
					buf[n] = '\0';
					fputs(buf, stdout);
				}
			}
			else
			{
				char buf[1024];
				while(1)
				{
						bzero(buf, 1024);
						char* input = fgets(buf, 1024, stdin);
						if(input == 0)
						{
							printf("message transmission ended\n");
							break;
						}
						
						int sentn = sendto(sockfd, buf, strlen(buf), 0, 
							(struct sockaddr*) &servadd, sizeof(servadd));

				}
			}
		}
	}
}
 
int main(int argc, char *argv[])
{
	/* handling command line arguments */

	int listen = 0, udp = 0;
	unsigned int port = 0;
	char* hostname;
	
	if(argc > 5) // too many args
	{
		usageerr();
		return 1;
	}
	else if(argc == 1)
	{
		usageerr();
		return 1;
	}
	else if(argc > 2)
	{

		int i;

		for(i = 1; i < (argc-1); i++) // parse arguments
		{
			if(strcmp(argv[i], "-l") == 0)
			{
				listen = 1;
			}
			else if(strcmp(argv[i], "-u") == 0)
			{
				udp = 1;
			}
		}
		
		// check second to last argument for hostname
		if(strcmp(argv[argc-2], "-l") == 0 || strcmp(argv[argc-2], "-u") == 0)
		{
			// is not a hostname
			if(listen == 0)
			{
				usageerr();
				return 1;
			}
		}
		else
		{
			hostname = argv[argc-2];
		}
	}
	
	if(hostname == NULL)
	{
		if(listen == 0)
		{
			usageerr();
			return 1;
		}
	}

	if((port = atoi(argv[argc-1])) == 0) // port is not a number
	{
		usageerr();
		return 1;
	}
	
	

	/* begin handling connection */
	
	printf("listen: %d, udp: %d, hostname: %s, port: %u\n", listen, udp, hostname, port);

	handlenc(listen, udp, hostname, port);

	return 0;
}
