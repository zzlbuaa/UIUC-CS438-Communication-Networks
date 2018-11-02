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
#include <fstream>

using namespace std;

#define MSS 1024
#define SS 0
#define CA 1
#define FR 2


typedef struct sharedData{
    int timeout = 2000;
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
    short len;
    char datagram[MSS+1];
}segment;

vector<segment> buffer;
int package_total;

shared_data data;

struct sockaddr_in si_other;
//struct sockaddr_in receiver_addr;
int s;
socklen_t addrlen = sizeof(si_other);
long int FIN_timer;

void readFile(char* filename, unsigned long long int numBytes);
void writeFileTest(char* filename);

void diep(const char *s) {
    perror(s);
    exit(1);
}

void* FIN_WATCH(void *id){
    struct timeval tp;
    segment seg;
    memset((char*)&seg, 0, sizeof(seg));
    seg.seq_num = -1;
    while(true){
        gettimeofday(&tp, NULL);
        long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
        if(ms - FIN_timer > data.timeout){
            if(sendto(s, &seg, sizeof(seg), 0, (struct sockaddr *)&si_other, sizeof(si_other)) == -1){
                diep("send error");
            }
            gettimeofday(&tp, NULL);
            FIN_timer = tp.tv_sec * 1000 + tp.tv_usec / 1000;
        }
    }
}

void FIN(){
    struct timeval tp;

    segment seg;
    memset((char*)&seg, 0, sizeof(seg));
    seg.seq_num = -1;
    int ack_sig = 0;

    while(true){
        if(sendto(s, &seg, sizeof(seg), 0, (struct sockaddr *)&si_other, sizeof(si_other)) == -1){
            diep("send error");
        }
        gettimeofday(&tp, NULL);
        FIN_timer = tp.tv_sec * 1000 + tp.tv_usec / 1000;
        pthread_t FIN_t;
        pthread_create(&FIN_t, NULL, &FIN_WATCH, NULL);
        
        int numbytes = recvfrom(s, &ack_sig, sizeof(ack_sig), 0, (struct sockaddr *)&si_other, &addrlen);
        cout << "ack_sig: " << ack_sig << endl;
        if(numbytes == -1){
            diep("fin recv error");
        }

        if(ack_sig != -1){
            int status = pthread_cancel(FIN_t);                                     
            if (status <  0) {                                                            
               diep("kill error");
            }
            pthread_join(FIN_t, NULL);
            continue;
        }

        int status = pthread_cancel(FIN_t);                                     
        if (status <  0) {                                                            
           diep("kill error");
        }
        pthread_join(FIN_t, NULL);
        break;
    }
    return;
}

void* watchDog(void *id){
    cout << "watchDog running..." << endl;
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
                pthread_mutex_unlock(&(data.mutex));
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
    cout << "receiveAck running..." << endl;
    int c;
    struct timeval tp;
    while(true){
        int numbytes = recvfrom(s, &c, sizeof(c), 0, (struct sockaddr *)&si_other, &addrlen);
        cout << "ack received, ack_sig:" << c << endl;
        if(numbytes > 0){
            pthread_mutex_lock(&(data.mutex));
            int ack_sig = c;
            if(ack_sig == package_total-1){
                // finish trans
                cout << "Prepare for FIN" << endl;
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
                                    pthread_mutex_unlock(&(data.mutex));
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
                                    pthread_mutex_unlock(&(data.mutex));
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
    // struct sockaddr_in si_other;
   
    // read file into buffer
    readFile(filename, bytesToTransfer);
    if(package_total < 0){
        cout << "No packages" << endl;
        return;
    }
    //writeFileTest("output");
    //return;

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
    cout << "Sending..." << endl;
    while(true){
        pthread_mutex_lock(&(data.mutex));
        if(data.complete){
            // prepare for closing connection
            pthread_mutex_unlock(&(data.mutex));
            break;
        }

        if(data.currIdx < package_total && data.currIdx < data.sendBase + (int)data.CW){
            segment seg = buffer[data.currIdx];
            if(sendto(s, &seg, sizeof(seg), 0, (struct sockaddr *)&si_other, sizeof(si_other)) == -1){
                pthread_mutex_unlock(&(data.mutex));
                diep("send error");
            }
            data.currIdx++;
            cout << "Send: " << data.currIdx << endl;
        }
        pthread_mutex_unlock(&(data.mutex));
    }

    FIN();

    /* Send data and receive acknowledgements on s*/
    printf("Closing the socket\n");
    close(s);

    return;

}

void readFile(char* filename, unsigned long long int numBytes){

    FILE *fp = fopen(filename, "rb");
    if(!fp) {
        perror("fopen error:");
        return;
    }
    int i = 0;
    int count = 0;
    char c;
    package_total = ceil(double(numBytes) / double(MSS));
    while(!feof(fp) && i < numBytes) {
        segment seg;
        memset((char*)&seg, 0, sizeof(seg));
        seg.seq_num = count;
        size_t ret_code = fread(seg.datagram, sizeof(char), MSS, fp);
        if(ret_code <= 0) {
            printf("finish reading.\n");
            break;
        } else if(ret_code == MSS){ // error handling
            seg.datagram[MSS] = '\0';
            seg.len = MSS;
            i += MSS;
        } else {
            i += (int)ret_code;
            seg.len = (int)ret_code;
            seg.datagram[(int)ret_code] = '\0';
        }
        buffer.push_back(seg);
        count++;
    }
    #ifdef _DEBUG
    cout << "Pkts: " << package_total << endl;
    #endif
    fclose(fp);
    return;
}

void writeFileTest(char* filename){
    FILE *fp;
    fp = fopen(filename, "wb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }
    cout << "buffer size: " << buffer.size() << endl;
    for(int i=0; i<buffer.size(); i++){
        segment seg = buffer[i];
        size_t ret_code = fwrite(seg.datagram, sizeof(char), MSS, fp);
        if ((int)ret_code <= 0){
            break;
        }
    }
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


