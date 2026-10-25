#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <errno.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>

volatile sig_atomic_t caught_signal = 0;
char * aesd_file = "/var/tmp/aesdsocketdata";

void signal_handler(int signal)
{
	caught_signal = signal;
}

int main(int argc, char *argv[])
{
	bool daemon_mode = false;

	if( argc ==2 &&(strcmp(argv[1], "-d") == 0 ))
	{
		daemon_mode = true;
	}


	openlog("aesd_socket", LOG_PERROR | LOG_PID, LOG_USER);
	
	struct sigaction sa = {0};
        sa.sa_handler = signal_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
	if (sigaction(SIGINT, &sa, NULL) == -1) 
	{
        	syslog(LOG_ERR, "Failed to register SIGINT handler");
    		return -1;
     	}

	if (sigaction(SIGTERM, &sa, NULL) == -1) 
	{
    		syslog(LOG_ERR, "Failed to register SIGTERM handler");
    		return -1;
	}
	
	struct addrinfo hints;
	struct addrinfo *res;
	struct sockaddr client;

	memset(&client, 0, sizeof(client));
	memset(&hints, 0, sizeof(hints));
    
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

        
	int getaddr_ret = getaddrinfo(NULL, "9000", &hints, &res);
	if(getaddr_ret != 0)
	{
		syslog(LOG_ERR, "getaddrinfo call failed %s",gai_strerror(getaddr_ret));
                closelog();
                return -1;
	}
	
	int sockfd = socket(res->ai_family, res->ai_socktype, 0);

	if(sockfd == -1)
	{
		syslog(LOG_ERR, "Dynamic memory allocation failed for res %s", strerror(errno));
                closelog();
		freeaddrinfo(res);
		return -1;
	}

        int reuse = 1;

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) 
        {
                syslog(LOG_ERR, "setsockopt failed: %s", strerror(errno));
                close(sockfd);
                freeaddrinfo(res);
                closelog();
                return -1;
        }


	if (-1 == bind(sockfd, res->ai_addr, res->ai_addrlen))
	{
		syslog(LOG_ERR, "Socket binding failed %s", strerror(errno));
                closelog();
		freeaddrinfo(res);
		close(sockfd);
		return -1;
	}
    
 	freeaddrinfo(res);

	if( daemon_mode )
        {
                pid_t pid = fork();
                
                if( pid < 0)
                {
                        syslog(LOG_ERR, "fork failed %s", strerror(errno));
                        close(sockfd);
                        closelog();
                        return -1;
                }
                if( pid > 0)
                {
                        close(sockfd);
			closelog();
                        return 0;
                }
        }


	if(-1 == listen(sockfd, 5))
	{
		syslog(LOG_ERR, "listen failed %s", strerror(errno));
                closelog();
                close(sockfd);
                return -1;
	}

	int fd = open(aesd_file, O_CREAT | O_APPEND | O_RDWR, 0644);
	if( fd == -1)
	{
	        syslog(LOG_ERR, "aesdsocketdata file open failed %s", strerror(errno));        
                closelog();       
                close(sockfd);
		return -1;
	}

	syslog(LOG_INFO, "socket created successfully and listening at port 9000");

	size_t capacity = 1024;

	char buffer[1024];

	while(!caught_signal)
	{
	
		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);
		int clientfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);


		if (clientfd == -1)
		{
			if(caught_signal)
			{
				break;
			}
			continue;
		}

		char ip[INET_ADDRSTRLEN];

                if (inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip)) == NULL)
                {
                        syslog(LOG_ERR, "inet_ntop failed: %s\n", strerror(errno));
                        close(clientfd);
                        continue;
                }

		syslog(LOG_INFO, "Accepted connection from %s\n", ip);

		char * packet = NULL;
		int packet_len = 0;
		bool discard_packet = false;

		while(!caught_signal)
		{
			int conn = recv(clientfd, buffer, capacity, 0);

			if(conn == -1)
			{
				if(caught_signal)
					break;
				syslog(LOG_ERR, "recv failed: %s", strerror(errno));
				break;
			}

			if (conn == 0)
			{
				syslog(LOG_INFO, "Closed connection from %s\n", ip);
				break;
			}

			if (conn > 0)
			{
				int start = 0;
				for( int i = 0 ; i<conn ; i++)
				{
					if( buffer[i] == '\n')
					{
						if(discard_packet)
						{
							discard_packet = false;
							continue;
						}

						size_t len = i - start + 1;
						char * tmp = realloc(packet, packet_len + len); // continuation from prev recv

						if( tmp == NULL)
						{
							syslog(LOG_ERR, "realloc failed discarding packet");
							free(packet); 
							packet = NULL;
							packet_len = 0;
							start = i + 1;
							continue;
						}

						packet = tmp;
						memcpy(packet + packet_len, buffer + start , len);
						packet_len += len;
						

						write(fd,packet,packet_len);
						char file_buffer[capacity];
						size_t bytes_read;

						lseek(fd,0,SEEK_SET);

						while( (bytes_read = read(fd, file_buffer, capacity)) > 0 )
						{
							send(clientfd, file_buffer, bytes_read, 0);
						}

						free(packet);
						packet = NULL;
						packet_len = 0;

						start = i + 1;
					}

				}

				if(start < conn )
				{
					size_t len = conn - start;

					char * tmp = realloc(packet, packet_len + len);

					if(tmp == NULL)
					{
						syslog(LOG_ERR, "realloc failed discarding packet");
						free(packet);
						packet = NULL;
					        packet_len = 0;
						discard_packet = true;
						continue;
					}

					packet = tmp;
					memcpy(packet + packet_len, buffer + start, len);
					packet_len += len;
				}
			}
		}

		free(packet);
		close(clientfd);
	}
	if(caught_signal)
	{
		syslog(LOG_INFO,"Caught signal, exiting");
	}
	close(sockfd);
	close(fd);
        if (unlink(aesd_file) == -1)
	{
		syslog(LOG_ERR, "unlink failed: %s", strerror(errno));
	}
	closelog();
	return 0;
}

