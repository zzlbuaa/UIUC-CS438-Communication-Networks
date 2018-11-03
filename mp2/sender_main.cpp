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
#include <time.h>
#include <queue>
#include <unordered_map>
#include <semaphore.h>

using namespace std;

#define MSS 1450
#define SS 0
#define CA 1
#define FR 2
#define SS_CW 100

#define _DEBUG

typedef struct sharedData{
    int timeout = 5;
    int dupAcksCount = 0;
    int threshold = 100;
    double CW = 100;
    int mode = SS;
    int sendBase = 0;
    long int timer = 0;
    int currIdx = 0;
    bool complete = false;
    // pthread_mutex_t mutex;
    sem_t sem;
}shared_data;

typedef struct Segment{
    long seq_num;
    short len;
    char datagram[MSS];
}segment;

unordered_map<int, segment*> buffer;
int package_total;

shared_data data;
struct sockaddr_in si_other;
int s;
socklen_t addrlen = sizeof(si_other);

void readFile(char* filename, unsigned long long int numBytes);

void diep(const char *s) {
    perror(s);
    exit(1);
}


void closeConnection(){
    #ifdef _DEBUG
    cout << "Prepare for closing..." << endl;
    #endif
    for(int i=0; i<5; i++){
        int fin_sig = -1;
        // int ack_sig;
        if(sendto(s, &fin_sig, sizeof(fin_sig), 0, (struct sockaddr *)&si_other, sizeof(si_other)) == -1){
            diep("FIN sending error");
        }
    }
}


// void* watchDog(void *id){
//     #ifdef _DEBUG
//     cout << "watchDog running..." << endl;
//     #endif
//     struct timeval tp;
//     while(true){
//         if(data.complete){
//             return NULL;
//         }

//         // pthread_mutex_lock(&(data.mutex));
//         sem_wait(&data.sem);

//         gettimeofday(&tp, NULL);
//         long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

//         if(ms - data.timer > data.timeout){
//             int base = data.sendBase;
//             #ifdef _DEBUG
//             cout << "Timeout: " << base << endl;
//             #endif
//             // retrans pkt
//             // segment seg = buffer[data.sendBase];

//             data.timer = ms; // update timer
//             data.threshold = data.CW / 2;
//             data.CW = 100;
//             data.dupAcksCount = 0;
//             data.mode = SS;
//             packetsQueuing.push_back(base);

//             // if(sendto(s, &seg, sizeof(seg), 0, (struct sockaddr *)&si_other, sizeof(si_other)) == -1){
//             //     // pthread_mutex_unlock(&(data.mutex));
//             //     diep("Timeout retransmission error");
//             // }
//         }
//         // pthread_mutex_unlock(&(data.mutex));
//         sem_post(&data.sem);
//     }
// }

void* receiveAck(void*){
    #ifdef _DEBUG
    cout << "receiveAck running..." << endl;
    #endif
    struct timeval tp;
    int ack_sig;
    tp.tv_sec = 0;
    tp.tv_usec = data.timeout * 1000;
    int ret = setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tp, sizeof(tp));
    if(ret == -1){
        diep("Set timeout error");
    }

    while(true){

        int numbytes = recvfrom(s, &ack_sig, sizeof(ack_sig), 0, (struct sockaddr *)&si_other, &addrlen);

        if(numbytes == -1){
            if(errno == EAGAIN){
                #ifdef _DEBUG
                cout << "Timeout" << endl;
                #endif

                // data.threshold = data.CW / 2;
                data.threshold = data.CW * 0.50;
                data.dupAcksCount = 0;
                data.mode = SS;
                sem_wait(&data.sem);
                data.CW = SS_CW;
                sem_post(&data.sem);

                segment *seg = buffer[data.sendBase];
                if(sendto(s, seg, sizeof(*seg), 0, (struct sockaddr *)&si_other, sizeof(si_other)) == -1){
                    diep("Sending error");
                }

                #ifdef _DEBUG
                cout << "Resend: " << data.sendBase << endl;
                #endif

                continue;
            }else{
                diep("Fin recv error");
            }
        }

        #ifdef _DEBUG
        cout << "ack received, ack_sig:" << ack_sig << endl;
        #endif

        if(numbytes > 0){
            if(ack_sig == package_total-1){
                // finish trans
                #ifdef _DEBUG
                cout << "Received last ack..." << endl;
                #endif
                data.complete = true;
                return NULL;
            } 

            switch(data.mode){
                case SS: {
                    if(ack_sig >= data.sendBase){
                        // update window base, reset timer
                        data.dupAcksCount = 0;
                        sem_wait(&data.sem);
                        data.sendBase = ack_sig+1;
                        data.CW += 1;
                        sem_post(&data.sem);

                        if(data.CW >= data.threshold){
                            data.mode = CA;
                        }
                        cout << "update base" << endl;
                        
                    }else{

                        if(++(data.dupAcksCount) == 3){
                            // fast retransmission
                            #ifdef _DEBUG
                            cout << "Received 3 dups" << endl;
                            #endif
                            data.mode = FR;
                            // data.threshold = data.CW / 2;
                            data.threshold = data.CW * 0.50;
                            sem_wait(&data.sem);
                            data.CW = data.threshold + 3;
                            sem_post(&data.sem);
                            segment *seg = buffer[data.sendBase];
                            if(sendto(s, seg, sizeof(*seg), 0, (struct sockaddr *)&si_other, sizeof(si_other)) == -1){
                                diep("Sending error");
                            }
                            #ifdef _DEBUG
                            cout << "Resend: " << data.sendBase << endl;
                            #endif
                        }
                    }
                    break;
                }
                case CA: {
                    // Congestion Avoidance mode
                    if(ack_sig >= data.sendBase){
                        // update window base, reset timer
                        data.dupAcksCount = 0;

                        sem_wait(&data.sem);
                        data.sendBase = ack_sig + 1;
                        data.CW = data.CW + 1.0 / int(data.CW);
                        sem_post(&data.sem);
                        cout << "update base" << endl;

                    }else{
                        if(++(data.dupAcksCount) == 3){
                            data.mode = FR;
                            // data.threshold = data.CW / 2;
                            data.threshold = data.CW * 0.50;
                            sem_wait(&data.sem);
                            data.CW = data.threshold + 3;
                            sem_post(&data.sem);

                            #ifdef _DEBUG
                            cout << "Received 3 dups" << endl;
                            #endif

                            segment *seg = buffer[data.sendBase];
                            if(sendto(s, seg, sizeof(*seg), 0, (struct sockaddr *)&si_other, sizeof(si_other)) == -1){
                                diep("Sending error");
                            }
                            #ifdef _DEBUG
                            cout << "Resend: " << data.sendBase << endl;
                            #endif
                        }
                    }
                    break;
                }

                case FR: {
                    // Fast Recover mode
                    if(ack_sig >= data.sendBase){
                        // update window base, reset timer, 
                        sem_wait(&data.sem);
                        data.sendBase = ack_sig + 1;
                        data.CW = data.threshold;
                        sem_post(&data.sem);
                        data.dupAcksCount = 0;                        
                        data.mode = CA;
                        cout << "update base" << endl;
                    }else{
                        sem_wait(&data.sem);
                        data.CW += 1;
                        sem_post(&data.sem);
                        data.dupAcksCount++;

                        if(data.dupAcksCount % 2 == 0){
                            segment *seg = buffer[data.sendBase];
                            if(sendto(s, seg, sizeof(*seg), 0, (struct sockaddr *)&si_other, sizeof(si_other)) == -1){
                                diep("Sending error");
                            }
                            #ifdef _DEBUG
                            cout << "Resend: " << data.sendBase << endl;
                            #endif
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }
}


void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    //Open the file
    int res = sem_init(&data.sem, 0, 1);
    if(res == -1)
    {
        perror("Semaphore intitialization failed\n");
        exit(EXIT_FAILURE);
    }

    // read file into buffer
    readFile(filename, bytesToTransfer);
    if(package_total < 0){
        cout << "No packages" << endl;
        return;
    }

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
    // pthread_t t_b;

    #ifdef _DEBUG
    cout << "Sending..." << endl;
    #endif
    clock_t start = clock();
    while(true){

        if(data.complete){
            break;
        }

        sem_wait(&data.sem);
        int upper = data.sendBase + (int)data.CW;
        sem_post(&data.sem);
        
        while(data.currIdx < package_total && data.currIdx < upper){
            segment *seg = buffer[data.currIdx];
            if(sendto(s, seg, sizeof(*seg), 0, (struct sockaddr *)&si_other, sizeof(si_other)) == -1){
                diep("Sending error");
            }

            data.currIdx++;

            if(data.currIdx == 1){
                pthread_create(&t_a, NULL, &receiveAck, NULL);
            }

            #ifdef _DEBUG
            cout << "Send: " << data.currIdx << endl;
            #endif
        }
    }
    
    closeConnection();

    clock_t end = clock();
    double cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("File transfer took %f seconds to execute \n", cpu_time_used);

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

    unsigned long long int i = 0;
    int count = 0;
    package_total = ceil(double(numBytes) / double(MSS));
    while(!feof(fp) && i < numBytes) {
        segment *seg = new segment;
        memset((char*)seg, 0, sizeof(*seg));
        seg->seq_num = count;
        size_t ret_code = fread(seg->datagram, sizeof(char), MSS, fp);
        if(ret_code <= 0) {
            printf("finish reading.\n");
            break;
        } else if(ret_code == MSS){ // error handling
            seg->len = MSS;
            i += MSS;
        } else {
            i += (int)ret_code;
            seg->len = (int)ret_code;
        }
        buffer[count] = seg;
        count++;
    }
    #ifdef _DEBUG
    cout << "Pkts: " << package_total << endl;
    #endif
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

    // data.mutex = PTHREAD_MUTEX_INITIALIZER;

    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);

    return (EXIT_SUCCESS);
}

