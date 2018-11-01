/* 
 * File:   sender_main.c
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
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <vector>
#include <cmath>
#include <iostream>
#include <string>

using namespace std;

#define MSS 1024
#define SS 0
#define CA 1
#define FR 2

typedef struct sharedData{
    int timeout = 10;
    int dupAcksCount = 0;
    int threshold = 10;
    double CW = 1;
    int mode = SS; //SS
    int sendBase = 0;
    long int timer = 0;
    int currIdx = 0;
    bool complete = false;
    pthread_mutex_t mutex;
}shared_data;

typedef struct Segment{
    short seq_num;
    char datagram[MSS+1];
}segment;

vector<segment> buffer;
int package_total;

shared_data data;

struct sockaddr_in si_other;
struct sockaddr_in receiver_addr;
int s;
socklen_t addrlen = sizeof(receiver_addr);

void readFile(char* filename, unsigned long long int numBytes);

void diep(const char *s) {
    perror(s);
    exit(1);
}

void* watchDog(void *id){
    struct timeval tp;
    while(true){
        pthread_mutex_lock(&(data.mutex));
        if(data.complete){
            pthread_mutex_unlock(&(data.mutex));
            return NULL;
        }
        gettimeofday(&tp, NULL);
        long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
        if(ms - data.timer > data.timeout){
            cout << "time out" << endl;
            // retrans pkt
            segment seg = buffer[data.sendBase];
            if(sendto(s, &seg, sizeof(seg), 0, (struct sockaddr *)&si_other, sizeof(si_other)) == -1){
                const string msg = "send error";
                diep(msg.c_str());
            }
            data.timer = ms; // update timer
            data.threshold = data.CW / 2;
            data.CW = 1.0;
            data.dupAcksCount = 0;
            data.mode = SS;
        }
        pthread_mutex_unlock(&(data.mutex));
    }
}

void* receiveAck(void *id){
    char c;
    struct timeval tp;
    while(true){
        int numbytes = recvfrom(s, &c, 1, 0, (struct sockaddr *)&receiver_addr, &addrlen);
        if(numbytes > 0){
            pthread_mutex_lock(&(data.mutex));
            int ack_sig = c - '0';
            if(ack_sig == package_total-1){
                // finish trans
                data.complete = true;
                pthread_mutex_unlock(&(data.mutex));
                return NULL;
            } 
            cout << "ack#: " << ack_sig << endl;
            switch(data.mode){
                case SS: {
                    if(ack_sig >= data.sendBase){
                        // update window base, reset timer, 
                        data.sendBase = ack_sig+1;
                        gettimeofday(&tp, NULL);
                        long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
                        data.timer = ms;
                        data.dupAcksCount = 0;
                        data.CW += 1;
                        if(data.CW >= data.threshold){
                            data.mode = CA;
                        }
                    }else{
                        if(ack_sig == data.sendBase-1){
                            // fast retransmission
                            data.dupAcksCount++;
                            if(data.dupAcksCount == 3){
                                // retran
                                cout << "3 dup" << endl;
                                segment seg = buffer[data.sendBase-1];
                                if(sendto(s, &seg, sizeof(seg), 0, (struct sockaddr *)&si_other, sizeof(si_other)) == -1){
                                    diep("send error");
                                }
                                data.mode = FR;
                                data.threshold = data.CW / 2;
                                data.CW = data.threshold + 3;
                            }
                        }
                    }
                    break;
                }
                case CA: {
                    if(ack_sig >= data.sendBase){
                        // update window base, reset timer, 
                        data.sendBase = ack_sig+1;
                        gettimeofday(&tp, NULL);
                        long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
                        data.timer = ms;
                        data.dupAcksCount = 0;
                        data.CW = data.CW + 1.0 / int(data.CW);
                    }else{
                        if(ack_sig == data.sendBase-1){
                            data.dupAcksCount++;
                            if(data.dupAcksCount == 3){
                                data.mode = FR;
                                // fast retrans
                                cout << "3 dup" << endl;
                                segment seg = buffer[data.sendBase-1];
                                if(sendto(s, &seg, sizeof(seg), 0, (struct sockaddr *)&si_other, sizeof(si_other)) == -1){
                                    diep("send error");
                                }
                                data.threshold = data.CW / 2;
                                data.CW = data.threshold + 3;
                            }
                        }
                    }
                    break;
                }

                case FR: {
                    if(ack_sig >= data.sendBase){
                        // update window base, reset timer, 
                        data.sendBase = ack_sig+1;
                        gettimeofday(&tp, NULL);
                        long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
                        data.timer = ms;
                        data.dupAcksCount = 0;
                        data.CW = data.threshold;
                        data.mode = CA;
                    }else{
                        data.CW += 1;
                        data.dupAcksCount++;
                    }
                    break;
                }
                default:
                    break;
            }
            pthread_mutex_unlock(&(data.mutex));
        }
    }
}


void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    //Open the file
    struct sockaddr_in si_other;

    // read file into buffer
    readFile(filename, bytesToTransfer);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }
    pthread_t t_a;
    pthread_t t_b;
    pthread_create(&t_a, NULL, &receiveAck, NULL);
    pthread_create(&t_b, NULL, &watchDog, NULL);

    while(true){
        pthread_mutex_lock(&(data.mutex));
        if(data.complete){
            pthread_mutex_unlock(&(data.mutex));
            break;
        }

        if(data.currIdx < data.sendBase + (int)data.CW){
            segment seg = buffer[data.currIdx];
            if(sendto(s, &seg, sizeof(seg), 0, (struct sockaddr *)&si_other, sizeof(si_other)) == -1){
                diep("send error");
            }
            data.currIdx++;
        }
        pthread_mutex_unlock(&(data.mutex));
    }

    /* Send data and receive acknowledgements on s*/
    printf("Closing the socket\n");
    close(s);

    return;

}

void readFile(char* filename, unsigned long long int numBytes){
    FILE *fp;
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

    int i=0;
    package_total = ceil(numBytes / MSS) + 1;
    int count = 0;
    while(i<numBytes){
        int j = 0;
        segment seg;
        seg.seq_num = count;
        while(i<numBytes && j<1024){
            char c = fgetc(fp);
            seg.datagram[j] = c;
            j++;
            i++;
        }
        seg.datagram[j] = '\0';
        buffer.push_back(seg);
        count++;
    }
    segment seg;
    seg.seq_num = -1;
    buffer.push_back(seg);
    fclose(fp);
    return;
}



int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);

    data.mutex = PTHREAD_MUTEX_INITIALIZER;

    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);

    return (EXIT_SUCCESS);
}


