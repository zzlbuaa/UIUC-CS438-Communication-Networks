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
#include <cmath>

#include <iostream>
#include <fstream>

#define MAX_BUFFER 5000
#define MSS 1024


typedef struct Segment{
    short seq_num;
    char datagram[MSS];//MSS+1?
}segment;

typedef struct Ack {
    int seq_num;
}ack;

struct sockaddr_in si_me, si_other;
int s;
socklen_t slen;
int expect_seq = 0;
int total_packets;
char buffer[MAX_BUFFER][MSS];
bool isReceived[MAX_BUFFER];//need to be initialized with false
unsigned short int senderPort;

char sender[INET6_ADDRSTRLEN];//only used for printf?
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
    printf("ack successful");
}

void writeFile(char* filename, unsigned long long int numBytes) {
    //write all segments in buffer to output file
    FILE *fp;
    fp = fopen(filename, "w");
    total_packets = ceil(numBytes / MSS);
    for (int i = 0; i < total_packets; i++) {
        fwrite(buffer[i], MSS, 1, fp);
    }
    fclose(fp);
    printf("write file finished.\n");
    return;
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
        printf("waiting for UDP on port: %d\n", myUDPport);
        segment seg;
        recvlen = recvfrom(s, &seg, sizeof(seg), 0, (struct sockaddr *)&si_other, &slen);
        
        inet_ntop(si_other.sin_family, get_in_addr((struct sockaddr *)&si_other), sender, sizeof sender);
        printf("receiver: got udp packet from %s\n", sender);
        if (recvlen < 0) {
            perror("recvfrom() error");
            exit(1);
        }
        if (seg.seq_num == -1) {
            //maybe using the first packet to make sure the size is better?
            finished = true;
            continue;
        }
        if (seg.seq_num == expect_seq) {
            ack a;
            a.seq_num = expect_seq;
            for (int i = 0; i < MSS; i++) {
                buffer[seg.seq_num][i] = seg.datagram[i];
                //if (seg.datagram[i] == '\0') {
                //    finished = true;
                //    break;
                //}
            }
            isReceived[seg.seq_num] = true;
            response(senderPort, a);
            expect_seq++;
            while (isReceived[expect_seq]) {
                expect_seq++;
            }
        } else if (seg.seq_num < expect_seq) {
            ack a;
            a.seq_num = seg.seq_num;
            response(senderPort, a);
        } else {
            isReceived[seg.seq_num] = true;
            ack a;
            a.seq_num = expect_seq - 1;
            for (int i = 0; i < MSS; i++) {
                buffer[seg.seq_num][i] = seg.datagram[i];
                //if (seg.datagram[i] == '\0') {
                //    finished = true;
                //    break;
                //}
            }
            //buffer[seg.seq_num] = seg.datagram;
            response(senderPort, a);
        }

    }
    int numbytes = 0;
    //writefile should specify numbytes again
    writeFile(destinationFile, numbytes);
    close(s);
    printf("%s received.", destinationFile);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;
    senderPort = udpPort + 1;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    //initialize global variables
    for (int i = 0; i < MAX_BUFFER; i++) {
        isReceived[i] = false;
    }

    udpPort = (unsigned short int) atoi(argv[1]);


    reliablyReceive(udpPort, argv[2]);
}

