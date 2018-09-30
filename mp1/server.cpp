/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include <vector>
#include <iostream>
#include <string>
#include <fstream>

#define PORT "80"  // the port users will be connecting to

#define BACKLOG 10	 // how many pending connections queue will hold
#define MAXDATASIZE 1024 // max number of bytes we can get at once

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

//return type, host, dir and HTTP version
std::string parse_request(std::string& request) {
	std::string dir = "";

	int len = request.length();
	int i=0;
	
	while(i < len){
		if(request[i] == '/'){
			request = request.substr(i + 1);
			int j = 0;
			while(request[j] != ' '){
				j++;
			}
			dir = request.substr(0,j);
			break;
		}
		i++;
	}

	i = 0;
	len = dir.length();
	while(i < len) {
		if(dir[i] == '/') {
			dir = dir.substr(i + 1, len - 1);
			return dir;
		}
		i++;
	}
	return dir;
}

void send_file(std::string dir, int sockfd) {
	char buff[MAXDATASIZE];
    FILE *fp = fopen(dir.c_str(), "rb");
    if(!fp) {
    	std::string header = "HTTP/1.1 404 Not Found\r\n\r\n";
	  	if (send(sockfd, header.c_str(), header.length(), 0) == -1)
			perror("send 404");
		return;
    }

    std::string header = "HTTP/1.1 200 OK\r\n\r\n";
  	if (send(sockfd, header.c_str(), header.length(), 0) == -1)
		perror("send 200");

    while(!feof(fp)) {
    	size_t ret_code = fread(buff, sizeof(char), MAXDATASIZE, fp); // reads an array of doubles
    	if(ret_code <= 0) {
        	printf("finish reading.\n");
        	break;
    	} else if(ret_code == MAXDATASIZE){ // error handling
       		printf("reading.\n");
       		if (send(sockfd, buff, MAXDATASIZE, 0) == -1)
       			perror("send");
   		} else {
   			printf("reading.\n");
       		if (send(sockfd, buff, (int)ret_code, 0) == -1)
       			perror("send");
   		}
  	}
 
   	fclose(fp);
 	std::cout << "finish reading" << std::endl;
  	exit(0);
}

int main(void)
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

		if (!fork()) { // this is the child process
			close(sockfd); // child doesn't need the listener
			int numbytes;
			char buf[MAXDATASIZE];
			std::string request = "";
			std::string dir = "";
			while(true) {
				numbytes = recv(new_fd, buf, MAXDATASIZE, 0);
				if(numbytes <=  0) {
					break;
				}

				request = std::string(buf);
				dir = parse_request(request);
				std::cout << "Request: " << request << std::endl;
				std::cout << "Dir: " << dir << std::endl;
				//char name[] = {"test.bin"};
				send_file(dir, new_fd);
			}

			close(new_fd);
			std::cout << "Child process finished, socket closed" << std::endl;
			exit(0);
		}
		close(new_fd);  // parent doesn't need this
	}

	return 0;
}

