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
#include <semaphore.h>

using namespace std;

#define MSS 1450

#define _DEBUG

typedef struct Segment{
    long seq_num;
    short len;
    char datagram[MSS];
}segment;




unordered_map<int,segment*> buf;
bool FIN = false;
sem_t sem;
int expected_ack = 0;
pthread_t t_write;

bool WRITE_FIN = false;


struct sockaddr_in si_me, si_other;

int s; 
socklen_t slen;
FILE *fp;
char* filename;

void diep(char *s) {
    perror(s);
    exit(1);
}


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
    if (seq < 0) {
        return;
    } 
    segment *seg = buf[seq];
    char dataToWrite[seg->len];
    int length = seg->len;
    cout << "length: " << length << endl;
    memcpy(dataToWrite,seg->datagram,length);

    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }
    //sem_wait(&data.sem);
    size_t ret_code = fwrite(dataToWrite, sizeof(char), length, fp);
    cout << "ret_code: " << ret_code << endl;
    //sem_post(&data.sem);
    if ((int)ret_code <= 0){
        cout << "writing file failed." << endl;
        WRITE_FIN = true;
    }
    return;
}

void* writeBuf(void *id){
    fp = fopen(filename, "ab");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }
    #ifdef _DEBUG
    cout << "writing the buf..." << endl;
    #endif
    int i = 0;
    while(!FIN && !WRITE_FIN) {
        //cout << "expected_ack: " << data.expected_ack << endl;
        while(i < expected_ack && buf.count(i) > 0) {
                // find next empty slot 
            writeFile(i);
            i++;
        }
    }
    cout << "fclose1..." << endl;
    fclose(fp);
    cout << "fclose..." << endl;
    pthread_exit(0);
}

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    //remove destination file if exist
    filename = destinationFile;
    remove(filename);

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

    int highest_ack = 0;

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
            sem_wait(&sem);
            FIN = true;
            sem_post(&sem);
            if(expected_ack > highest_ack){
                break;
            }
        }else{
            highest_ack = max(seq_num, highest_ack);
            sem_wait(&sem);
            buf[seq_num] = package;
            sem_post(&sem);
        }

        if(seq_num == expected_ack){

            //writeFile(expected_ack);
            int i = expected_ack+1;
            while(buf.count(i) > 0){
                // find next empty slot 
                //writeFile(i);
                i++;
            }
            
            sem_wait(&sem);
            expected_ack = i;
            sem_post(&sem);
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

    // void *res;
        
    // s = pthread_join(t_write, &res);
    // if (s != 0)
    //     diep("pthread_join");
 
    // if (res == PTHREAD_CANCELED)
    //     printf("main(): thread 1 was canceled\n");
    // else
    //     printf("main(): thread 1 wasn't canceled (shouldn't happen!)\n");

    closeConnection();
    #ifdef _DEBUG
    cout << "Connection close" << endl;
    #endif

    close(s);
    //fclose(fp);
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

    int res = sem_init(&sem, 0, 1);
    if(res == -1)
    {
        perror("Semaphore intitialization failed\n");
        exit(EXIT_FAILURE);
    }

    pthread_create(&t_write, NULL, &writeBuf, NULL);
    reliablyReceive(udpPort, argv[2]);
    // clock_t end = clock();
    // double cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    // printf("for loop took %f seconds to execute \n", cpu_time_used);
    pthread_exit(0);
}