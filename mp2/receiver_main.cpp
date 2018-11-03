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
#include <vector>   
#include <iostream>
#include <unordered_map>
#include <time.h>

using namespace std;

#define MSS 1450

#define _DEBUG

typedef struct Segment{
    long seq_num;
    short len;
    char datagram[MSS];
}segment;

unordered_map<int,segment*> buf;

struct sockaddr_in si_me, si_other;

int s; 
socklen_t slen;
FILE *fp;

void diep(char *s) {
    perror(s);
    exit(1);
}

long total = 0;

void closeConnection(){
    #ifdef _DEBUG
    cout << "Prepare for closing..." << endl;
    #endif
    while(true){
        struct timeval tp;
        tp.tv_sec = 0;
        tp.tv_usec = 2000;
        int fin_sig = -1;
        if(sendto(s, &fin_sig, sizeof(fin_sig), 0, (struct sockaddr *)&si_other, slen) == -1){
            diep("FIN sending error");
        }
        int ret = setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tp, sizeof(tp));
        segment package;
        int numbytes = recvfrom(s, &package, sizeof(package), 0, (struct sockaddr*)&si_other, &slen);
        if(numbytes == -1){
            if(errno == EAGAIN){
                break;
            }
            diep("Fin recv error");
        }
    }
}

void writeFile(int seq){
    //append to existing file every time
    segment *seg = buf[seq];

    size_t ret_code = fwrite(seg->datagram, sizeof(char), seg->len, fp);
    if ((int)ret_code <= 0){
        return;
    }
    total += ret_code;
    return;
}

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    
    remove(destinationFile);
    fp = fopen(destinationFile, "ab");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

    slen = sizeof (si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep("bind");

    int expected_ack = 0;
    int highest_ack = 0;
    bool FIN = false;

    /* Now receive data and send acknowledgements */    
    while(true){
        segment *package = new segment;

        if(FIN && expected_ack > highest_ack){
            break;
        }

        int numbytes = recvfrom(s, package, sizeof(*package), 0, (struct sockaddr*)&si_other, &slen);
        #ifdef _DEBUG
        // cout << "Received Bytes: " << numbytes << endl;
        #endif
        if(numbytes == -1){
            diep("recv error");
        }

        // add curr received package to buf
        int seq_num = package->seq_num;
        #ifdef _DEBUG
        cout << "Received: " << seq_num << endl;
        #endif
        if(seq_num == -1){
            FIN = true;
            if(expected_ack > highest_ack){
                break;
            }
        }else{
            highest_ack = max(seq_num, highest_ack);
            buf[seq_num] = package;
        }

        if(seq_num == expected_ack){
            writeFile(expected_ack);
            int i = expected_ack+1;
            while(buf.count(i) > 0){
                // find next empty slot 
                writeFile(i);
                i++;
            }
            

            expected_ack = i;
            int reply_ack = expected_ack-1;
            // reply with ack
            if(sendto(s, &reply_ack, sizeof(reply_ack), 0, (struct sockaddr *)&si_other, slen) == -1){
                diep("send error");
            }
        }else{
            int reply_ack = expected_ack-1;
            // reply with dup Acks
            if(sendto(s, &reply_ack, sizeof(reply_ack), 0, (struct sockaddr *)&si_other, slen) == -1){
                diep("send error");
            }
        }
        #ifdef _DEBUG
        cout << "expected_ack: " << expected_ack << endl;
        cout << "highest_ack: " << highest_ack << endl;
        cout << endl;
        #endif
    }

    closeConnection();
    #ifdef _DEBUG
    cout << "Connection close" << endl;
    #endif
    
    cout << "Total bytes write: " << total << endl;

    close(s);
    fclose(fp);
    // printf("%s received.", destinationFile);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {
    // clock_t start = clock();
    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);
    reliablyReceive(udpPort, argv[2]);
    // clock_t end = clock();
    // double cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    // printf("for loop took %f seconds to execute \n", cpu_time_used);
    return 0;
}

