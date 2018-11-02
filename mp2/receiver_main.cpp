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

using namespace std;

#define MSS 1024

#define _DEBUG

typedef struct Segment{
    short seq_num;
    short len;
    char datagram[MSS+1];
}segment;

unordered_map<int,segment*> buf;

struct sockaddr_in si_me, si_other;

int s; 
socklen_t slen;

void diep(char *s) {
    perror(s);
    exit(1);
}


void closeConnection(){
    #ifdef _DEBUG
    cout << "prepare for closing..." << endl;
    #endif
    while(true){
        struct timeval tp;
        tp.tv_sec = 0;
        tp.tv_usec = 20*1000;
        int fin_sig = -1;
        if(sendto(s, &fin_sig, sizeof(fin_sig), 0, (struct sockaddr *)&si_other, slen) == -1){
            diep("send error");
        }
        int ret = setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tp, sizeof(tp));
        segment package;
        int numbytes = recvfrom(s, &package, sizeof(package), 0, (struct sockaddr*)&si_other, &slen);
        if(numbytes == -1){
            if(errno == EAGAIN){
                break;
            }
            diep("recv error");
        }
    }
}

// void writeFile(char* filename, int highest_ack){
//     FILE *fp;
//     #ifdef _DEBUG
//     cout << "Writing file..." << endl;
//     #endif
//     fp = fopen(filename, "wb");
//     if (fp == NULL) {
//         printf("Could not open file to send.");
//         exit(1);
//     }
//     for(int i=0; i<=highest_ack; i++){
//         segment *seg = buf[i];
//         if(i < highest_ack){
//             size_t ret_code = fwrite(seg->datagram, sizeof(char), MSS, fp);
//             if ((int)ret_code <= 0){
//                 break;
//             }
//         }else{
//             int j=MSS;
//             while(seg->datagram[j] == '\0'){
//                 j--;
//             }
//             size_t ret_code = fwrite(seg->datagram, sizeof(char), j+1, fp);
//             if ((int)ret_code <= 0){
//                 break;
//             }
//         }
//     }
//     fclose(fp);
//     return;
// }

void writeFile(char* filename, int seq){
    FILE *fp;
    #ifdef _DEBUG
    //cout << "Writing file..." << endl;
    #endif
    //append to existing file every time
    if(seq == 0) {
        fp = fopen(filename, "wb");
    }else{
        fp = fopen(filename, "ab");
    }
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

    segment *seg = buf[seq];

    size_t ret_code = fwrite(seg->datagram, sizeof(char), seg->len, fp);
    if ((int)ret_code <= 0){
        return;
    }

    // if(!isLastPckt){
    //     size_t ret_code = fwrite(seg->datagram, sizeof(char), MSS, fp);
    //     if ((int)ret_code <= 0){
    //         return;
    //     }
    // }else{
    //     int j=MSS;
    //     while(seg->datagram[j] == '\0'){
    //         j--;
    //     }
    //     if (j != 1023 ) cout << "j: " << j << endl;
    //     size_t ret_code = fwrite(seg->datagram, sizeof(char), j+1, fp);
    //     if ((int)ret_code <= 0){
    //         return;
    //     }
    // }

    fclose(fp);
    return;
}

// void writeFile(char* filename, int highest_ack){
//     cout << "Writing file..." << endl;
//     FILE *fp;
//     fp = fopen(filename, "wb");
//     if (fp == NULL) {
//         printf("Could not open file to send.");
//         exit(1);
//     }
//     for(int i=0; i<=highest_ack; i++){
//         segment *seg = buf[i];
//         int j=0;
//         while(seg->datagram[j] != '\0'){
//             fputc(seg->datagram[j], fp);
//             j++;
//         }
//     }
//     fclose(fp);
//     return;
// }

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    
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
        cout << "Received Bytes: " << numbytes << endl;
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
            writeFile(destinationFile, expected_ack);
            int i = expected_ack+1;
            while(buf.count(i) > 0){
                // find next empty slot 
                writeFile(destinationFile, i);
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
        #endif
    }
        
    closeConnection();
    #ifdef _DEBUG
    cout << "Connection close" << endl;
    #endif
    //writeFile(destinationFile, highest_ack, true);

    close(s);
    printf("%s received.", destinationFile);
    return;
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

