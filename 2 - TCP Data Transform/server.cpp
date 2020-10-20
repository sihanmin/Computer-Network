/* A simple server in the internet domain using TCP
   The port number is passed as an argument
   This version runs forever, forking off a separate
   process for each connection
*/
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>  /* signal name macros, and the kill() prototype */
#include <stdbool.h>
#include <iostream>
#include <list>
#include "TCPheader.h"

const int WND_SIZE = 5120;

struct packetContainer{
    packet* pkt;
    time_t timer;
    bool received;
};


void error(const char *msg)
{
    perror(msg);
    exit(1);
}

int recvPkt(int sockfd, char* buffer, int buffer_len, bool print, struct sockaddr*  src_addr, socklen_t* src_addr_len)
{
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec=500000;
    int ret = select(sockfd+1,&rfds, NULL, NULL, &tv);
    if(ret == -1){
        fprintf(stderr, " >>> Recv: select() on sockfd <%d> failed\n", sockfd);
        return -1;
    }
    else if(ret == 0){
        //fprintf(stderr, " >>> Recv: select() on sockfd <%d> timed out\n", sockfd);
        return -2;
    }
    memset(buffer, 0, buffer_len);
    int rd_len = recvfrom(sockfd, buffer, buffer_len, 0, src_addr, src_addr_len);
    if (rd_len < 0) return rd_len;
    struct packet* inPkt  = (struct packet*) buffer;
    if(print) printf("Receiving packet %d\n", inPkt->seq);
    // fprintf(stderr, ">>> Packet from client: \n%s\n", buffer);
    return rd_len;
}

int sendPkt(int sockfd, packet* pkt, int pkt_len, bool print, struct sockaddr_storage dest_addr, socklen_t dest_addr_len)
{
    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(sockfd, &wfds);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000;
    int ret = select(sockfd+1,NULL, &wfds, NULL, &tv);
    if(ret == -1){
        fprintf(stderr, " >>> Send: select() on sockfd <%d> failed\n", sockfd);
        return -1;
    }
    else if(ret == 0){
        //fprintf(stderr, " >>> Send: select() on sockfd <%d> timed out\n", sockfd);
        return -2;
    }
    char buffer[PACKET_SIZE];
    memcpy(buffer, (char*) pkt, pkt_len);
    int wr_len = sendto(sockfd,buffer,pkt_len,0,(struct sockaddr *)& dest_addr, dest_addr_len);
    if (wr_len < 0) return wr_len;
    if(print){
        printf("Sending packet %d %d",pkt->seq, pkt->wnd);
        if(pkt->type == TYPE_RETRANSMISSION) printf(" Retransmission");
        if(pkt->type == TYPE_SYN) printf(" SYN");
        if(pkt->type == TYPE_FIN) printf(" FIN");
        printf("\n");
    }
    return wr_len;
}

void updateSeq(int& seq, int rd_len){
    if(seq < 0){
        fprintf(stderr, "Seq is less than 0, %d", seq);
    }
    seq += rd_len;
    if(seq > MAX_SEQ) seq %= MAX_SEQ;
    fprintf(stderr, " >>> Data Length: %d\n >>> SEQ is now: %d\n", rd_len,seq);
}

int setAck(packet* inPkt){
    int ack = inPkt->seq + inPkt->size - HEADER_SIZE;
    if(ack > MAX_SEQ) ack %= MAX_SEQ;
    return ack;
}

void updatePktList(list <packetContainer*> &l, int ack, int& wnd)
{
    fprintf(stderr, " >>> updatePktList %d\n", l.size());
    if(l.empty()) return;
    for(auto itr = l.begin(); !l.empty() && itr != l.end(); ++itr){
        packet* content = (*itr)->pkt;
        int expectedACK = (content->seq + content->size - HEADER_SIZE) % MAX_SEQ;
        if(ack == expectedACK) (*itr)->received = true;
        //fprintf(stderr," >>> oldestUnackedSeq: %d, pkt Seq: %d, expectedAck: %d, ack received: %d\n", oldestUnackedSeq, content->seq, expectedACK, ack);
        if(itr == l.begin() && (*itr)->received){
            fprintf(stderr, " >>> removing seq %d\n", (*itr)->pkt->seq);
            wnd += (*itr)->pkt->size;
            itr = l.erase(itr);
            itr--;
            fprintf(stderr, " >>> remaining: %d\n",l.size());
        }
    }
    //fprintf(stderr, " >>> updatePktList Done\n");
}

void check_timeout(list <packetContainer*> l, int sockfd, struct sockaddr_storage dest_addr, socklen_t dest_addr_len)
{
    fprintf(stderr," >>> check_timeout, list size: %d\n", l.size());
    time_t curTime;
    time(&curTime);
    for(auto pkt = l.begin(); pkt != l.end(); ++pkt){
        fprintf(stderr, " >>> Packet seq %d, Received: %d\n",(*pkt)->pkt->seq,(*pkt)->received);
        if(!(*pkt)->received && difftime(curTime,(*pkt)->timer) >= TIME_OUT){
            sendPkt(sockfd, (*pkt)->pkt, (*pkt)->pkt->size, true, dest_addr, dest_addr_len);
            time(&(*pkt)->timer);
        }
    }
    //fprintf(stderr," >>> check_timeout Done\n");
}
// return the seq num of the oldest unack packet


int main(int argc, char *argv[])
{
    int sockfd,portno;
    struct sockaddr_in serv_addr;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(struct sockaddr_storage);

    if (argc < 2) {
        // fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);  // create socket
    if (sockfd < 0)
        error("ERROR opening socket");
    memset((char *) &serv_addr, 0, sizeof(serv_addr));   // reset memory

    // fill in address info
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    int seq = -1, wnd_size = WND_SIZE;
    int rd_len,wr_len;
    char buffer[PACKET_SIZE];
    struct packet* inPkt  = (struct packet*) buffer;
    struct packet* outPkt = NULL;
    bool fileFound = true;
    while(1)
    {
        FILE * fp;
        while(1)
        {
            //Initialize connection
            //read client's message, expecting SYN
            rd_len = recvPkt(sockfd, buffer, PACKET_SIZE, false, (struct sockaddr *)& client_addr, &client_addr_len);
            if(rd_len <= 0 || inPkt->type != TYPE_SYN) continue;
            seq = setAck(inPkt);

            while(1){
                //send SYN ACK
                outPkt = create_packet(seq, wnd_size, TYPE_SYN, 1, "0");
                wr_len = sendPkt(sockfd, outPkt, outPkt->size, true, client_addr, client_addr_len);
                delete(outPkt);

                //Wait for ACK
                rd_len = recvPkt(sockfd, buffer, PACKET_SIZE, true, (struct sockaddr *)& client_addr, &client_addr_len);
                seq = inPkt->seq;
                //cout << "seq: " << inPkt->seq << endl << "size: " << inPkt->size << endl;
                //cout << "message: " << inPkt->data << endl;
                if(rd_len > 0 && inPkt->type == TYPE_DATA){
                    int filename_len = inPkt->size-HEADER_SIZE;
                    char*filename = new char[filename_len+1];
                    memset(filename,'\0',filename_len+1);
                    strncpy(filename, inPkt->data,filename_len);
                    fprintf(stderr," >>> Requested file name: %s, length: %d\n", filename, filename_len);
                    fp = fopen(filename,"r");
                    delete []filename;
                    if (fp == NULL){
                        fileFound = false;
                        while(1){
                            outPkt = create_packet(seq, wnd_size, TYPE_404, 1, "0");
                            wr_len = sendPkt(sockfd, outPkt, outPkt->size, true, client_addr, client_addr_len);
                            delete(outPkt);
                            fprintf(stderr, " >>> File Not Found\n");
                            rd_len = recvPkt(sockfd, buffer, PACKET_SIZE, true, (struct sockaddr *)& client_addr, &client_addr_len);
                            seq = inPkt->seq;
                            if(rd_len > 0 && inPkt->type == TYPE_ACK) break;
                        }
                        break;
                    }
                    else{
                        fileFound = true;
                        while(1){
                            outPkt = create_packet(seq, wnd_size, TYPE_ACK, 1, "0");
                            wr_len = sendPkt(sockfd, outPkt, outPkt->size, true, client_addr, client_addr_len);
                            delete(outPkt);
                            rd_len = recvPkt(sockfd, buffer, PACKET_SIZE, true, (struct sockaddr *)& client_addr, &client_addr_len);
                            seq = setAck(inPkt);
                            if(rd_len > 0 && inPkt->type == TYPE_ACK){
                                seq = inPkt->seq;
                                break;
                            }
                        }
                        break;
                    }

                    break; //Not incrementing seq number because it is for the filename not data
                }
                //retransmit when no reply after 500ms
            }
            break;
        }
        fprintf(stderr, " >>> Connection Initialized\n\n");
        
        if(fileFound){
            int file_rd_len;
            int rm_wnd = 5120;  // remaining used window size
            char fileBuffer[DATA_SIZE]; //size of read should be min(DATA_SIZE, remaining wnd size)
            memset(fileBuffer,'\0',DATA_SIZE);
            list<struct packetContainer*> window; // a link list of packets in the window

            while(window.size()*PACKET_SIZE == WND_SIZE || (file_rd_len = fread(fileBuffer,sizeof(char),DATA_SIZE,fp)) > 0 || !window.empty()){
                fprintf(stderr, " >>> Newly read file length: %d, rm_wnd: %d\n", file_rd_len, rm_wnd);
                if(file_rd_len > 0 && rm_wnd >= file_rd_len + HEADER_SIZE){
                    struct packetContainer* newPkt = new packetContainer;
                    outPkt = create_packet(seq, wnd_size, TYPE_DATA, file_rd_len, fileBuffer);
                    memset(fileBuffer,'\0',DATA_SIZE);
                    packet* copy = copy_packet(outPkt);
                    newPkt->pkt = copy;
                    copy->type = TYPE_RETRANSMISSION;
                    wr_len = sendPkt(sockfd, outPkt, outPkt->size, true, client_addr, client_addr_len);
                    delete(outPkt);
                    time(&newPkt->timer);
                    newPkt->received = false;
                    window.push_back(newPkt);
                    fprintf(stderr, " >>> Adding new pkt: %d, total: %d\n", newPkt->pkt->seq, window.size());
                    updateSeq(seq,file_rd_len);
                    rm_wnd -= (file_rd_len + HEADER_SIZE);
                } //create packet if there is remaining space in window
                check_timeout(window, sockfd, client_addr, client_addr_len);
                rd_len = recvPkt(sockfd, buffer, PACKET_SIZE, true, (struct sockaddr *)& client_addr, &client_addr_len);
                if(rd_len > 0 && inPkt->type == TYPE_ACK){
                    updatePktList(window, inPkt->seq, rm_wnd);
                }
                file_rd_len = 0;
            }
            fprintf(stderr, " >>> File Transmission Completed\n");
            fclose(fp);
        }

        fprintf(stderr, " >>> Starting close sequence.\n");
        while(1){
            outPkt = create_packet(seq, wnd_size, TYPE_FIN, 1, "0");
            wr_len = sendPkt(sockfd, outPkt, outPkt->size, true, client_addr, client_addr_len);
            delete(outPkt);
            rd_len = recvPkt(sockfd, buffer, PACKET_SIZE, false, (struct sockaddr *)& client_addr, &client_addr_len);
            if(rd_len <= 0 || (inPkt->type != TYPE_FIN && inPkt->type != TYPE_ACK)) continue;
            if(inPkt->type == TYPE_ACK){
                while(inPkt->type != TYPE_FIN){
                    rd_len = recvPkt(sockfd, buffer, PACKET_SIZE, false, (struct sockaddr *)& client_addr, &client_addr_len);
                }
                cout << "Receiving packet " << inPkt->seq << " FIN" << endl;
            }
            outPkt = create_packet(setAck(inPkt), wnd_size, TYPE_ACK, 1, "0");
            wr_len = sendPkt(sockfd, outPkt, outPkt->size, false, client_addr, client_addr_len);
            delete(outPkt);
            break;
        }
        //timewait
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sockfd, &rfds);
        struct timeval tv;
        tv.tv_sec = TIME_OUT*2;
        tv.tv_usec= 0;
        while(1){
            int ret = select(sockfd+1,&rfds, NULL, NULL, &tv);
            if(ret == -1){
                fprintf(stderr," >>> select() on sockfd <%d> failed\n", sockfd);
                return -1;
            }
            else if(ret == 0){
                fprintf(stderr, " >>> Time-Wait expired\n");
                break;
            }
            rd_len = recvPkt(sockfd, buffer, PACKET_SIZE, false, (struct sockaddr *)& client_addr, &client_addr_len);
            if(rd_len > 0){
                outPkt = create_packet(setAck(inPkt), wnd_size, TYPE_ACK, 1, "0");
                wr_len = sendPkt(sockfd, outPkt, outPkt->size, false, client_addr, client_addr_len);
                delete(outPkt);
            }
        }
        fprintf(stderr, " >>> Finished close sequence\n\n");
   }
    close(sockfd);
    return 0;
}

