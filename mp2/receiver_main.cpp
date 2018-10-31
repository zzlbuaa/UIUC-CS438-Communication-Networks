/* 
 * File:   receiver_main.c
 * Author: 
 *
 * Created on
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#include <iostream>
#include <fstream>

#define MAX_BUFFER 5000
#define MSS 1450
#define SENDER_PORT 4000


struct packet {
    int seq;
    char datagram[MSS];
};

struct ack {
    int seq;
};

struct sockaddr_in si_me, si_other;
int s, slen;
int expect_seq;
char sender[INET6_ADDRSTRLEN];
char buffer[MAX_BUFFER][MSS];
bool received[MAX_BUFFER];
//char port[80];

void diep(char *s) {
    perror(s);
    exit(1);
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    
    bool finished = false;
    slen = sizeof (si_other);

    int recvlen;


    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep("bind");


	/* Now receive data and send acknowledgements */    
    while (!finished) {
        printf("waiting for UDP on port: %d\n", udpPort);
        packet pkt;
        recvlen = recvfrom(s, &pkt, sizeof(pkt), 0, (struct sockaddr *)&si_other, &slen);
        
        inet_ntop(si_other.ss_family, get_in_addr((struct sockaddr *)&si_other), sender, sizeof sender);
        printf("receiver: got udp packet from %s\n", sender);
        if (recvlen < 0) {
            perror("recvfrom() error");
            exit(1);
        }
        if (pkt.seq == expect_seq) {
            ack a;
            a.seq = expect_seq;
            if (received[pkt.seq]) {
                response(SENDER_PORT, a);
                continue;
            }
            for (int i = 0; i < MSS; i++) {
                //TODO: is there a need to have buffer?
                //TODO: write to file
                buffer[expect_seq][i] = pkt.datagram[i];
                if (pkt.datagram[i] == '\0') {
                    finished = true;
                    break;
                }
            }
            received[expect_seq] = true;
            response(SENDER_PORT, a);
            expect_seq++;
        } else if (pkt.seq < expect_seq) {
            ack a;
            a.seq = pkt.seq;
            response(SENDER_PORT, a);
        } else {
            received[pkt.seq] = true;
            ack a;
            a.seq = expect_seq - 1;
            response(SENDER_PORT, a);
        }

    }


    close(s);
	printf("%s received.", destinationFile);
    return;
}

void response(int s_port, ack a) {
    int fd;
    if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("ack socket");
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(s_port);

    if (sendto(fd, &a, sizeof(a), 0, (struct sockaddr *)&si_other, sizeof(si_other)) < 0) {
        perror("sendto failed");
        exit(3);
    }
    printf("finish ack");
}

void write_file() {

}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);


    reliablyReceive(udpPort, argv[2]);
}

