/*
 A simple client in the internet domain using TCP
 Usage: ./client hostname port (./client 192.168.0.151 10000)
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>      // define structures like hostent
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <stdbool.h>
#include <list>
#include <cmath>
#include "TCPheader.h"

const int rwnd = 5; //reciever window size
const int headerSz = 12; //Bytes
const int maxPktSz = PACKET_SIZE; //max packet size, in chars.

void error(const char *msg)
{
    perror(msg);
    exit(0);
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
    memset(buffer, '\0', buffer_len);
    int rd_len = recvfrom(sockfd, buffer, buffer_len, 0, src_addr, src_addr_len);
    if (rd_len < 0) return rd_len;
    struct packet* inPkt  = (struct packet*) buffer;
    if(print && inPkt->type != TYPE_FIN) printf("Receiving packet %d\n", inPkt->seq);
    // fprintf(stderr,">>> Packet from server: \n%s\n", buffer);
    return rd_len;
}

int sendPkt(int sockfd, packet* pkt, int pkt_len, bool print, struct sockaddr_in dest_addr, socklen_t dest_addr_len)
{
    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(sockfd, &wfds);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec=500000;
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
        printf("Sending packet");
        if(pkt->type == TYPE_SYN) printf(" SYN");
        else
        {
            printf(" %d",pkt->seq);
            if(pkt->type == TYPE_RETRANSMISSION) printf(" Retransmission");
            if(pkt->type == TYPE_FIN) printf(" FIN");
        }
        printf("\n");
    }
    return wr_len;
}

int setAck(packet* inPkt){
    int ack = inPkt->seq + inPkt->size - HEADER_SIZE;
    if(ack > MAX_SEQ) ack %= MAX_SEQ;
    return ack;
}

bool compare_order (const packet* first, const packet* second)
{
    int diff = first->seq - second->seq;
    if((diff == 0) ||(diff > 0 && diff < MAX_SEQ/2) || (diff < 0 && abs(diff) > MAX_SEQ/2)) return false;
    return true;
}

void updatePktList(list <packet*> &l, int &lowestSeq, FILE* fp)
{
    if(l.empty()) return;
    l.sort(compare_order);
    for(auto itr = l.begin(); !l.empty() && itr != l.end(); ++itr){
        if((*itr)->seq == lowestSeq){
            lowestSeq = setAck(*itr);
            fwrite((*itr)->data,sizeof(char),(*itr)->size-HEADER_SIZE,fp);
            fprintf(stderr," >>> Remove packet seq %d, length: %d, lowestSeq: %d\n",(*itr)->seq, (*itr)->size-HEADER_SIZE, lowestSeq);
            itr = l.erase(itr);
            itr--;
        }
    }
}

bool searchPktList(const list <packet*> l, int seq) //return true if packet seq found
{
    if(l.empty()) return false;
    for(auto itr = l.begin(); itr != l.end(); ++itr){
        if((*itr)->seq == seq) return true;
    }
    return false;
}

int main(int argc, char *argv[])
{
    int *expSeqNum = (int*)malloc(sizeof(int));
    *expSeqNum = 0; //expected sequence number. lets say it starts at 0 for now.
    //create (rwnd) buffers each of size (maxPktSz) and store in pktsBuffer
    char ** pktsBuffer; 
    pktsBuffer = (char**)malloc(sizeof(char*)*rwnd);
    //create array of pkt nums, pktNums[i] be -1 if pktsBuffer[i] is empty
    //pktNums[i] gives number of packet from 1 to (rwnd)
    int * pktNums;
    pktNums = (int*)malloc(sizeof(unsigned int)*rwnd);
    int i;
    for (i = 0; i<rwnd;i++){
        pktsBuffer[i] = (char*)malloc(sizeof(char)*maxPktSz);
        pktNums[i] = -1;
    }
    //emptyIndex is index of first empty pkt in pktsBuffer
    unsigned int emptyIndex = 0;
    int serverfd;  // socket descriptor
    int portno;
    struct sockaddr_in serv_addr;
    struct hostent *server;  // contains tons of information, including the server's IP address
    struct sockaddr_storage src_addr;
    socklen_t src_addr_len, serv_addr_len;

    char buffer[PACKET_SIZE];
    if (argc < 4) {
        fprintf(stderr,"usage %s hostname port filename\n", argv[0]);
        exit(0);
    }
    
    portno = atoi(argv[2]);
    serverfd = socket(AF_INET, SOCK_DGRAM, 0);  // create a new socket
    if (serverfd < 0)
        error("ERROR opening socket");

    server = gethostbyname(argv[1]);  // takes a string like "www.yahoo.com", and returns a struct hostent which contains information, as IP address, address type, the length of the addresses...
    if (server == NULL) {
        // fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    char* filename = argv[3];
    FILE *fp;
    int init_seq = 5095;

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; //initialize server's address
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    int rd_len, wr_len;
    struct packet* inPkt  = (struct packet*) buffer;
    struct packet* outPkt = NULL;
    
    int lowestSeq = 0;
    //Initialize connection
    bool fileFound = true;
    while(1)
    {
        outPkt = create_packet(init_seq, 0, TYPE_SYN, 0, "");
        wr_len = sendPkt(serverfd, outPkt, outPkt->size, true, serv_addr, sizeof(serv_addr));
        delete(outPkt);
        //if(wr_len < 0) continue;
        rd_len = recvPkt(serverfd, buffer, PACKET_SIZE, true, (struct sockaddr *)& src_addr, &src_addr_len);
        lowestSeq = setAck(inPkt);
        if(rd_len < 0){
            sleep(rand()%2); //pick random sleep value to reduce collision
            continue; //resend SYN if failed
        }
        fprintf(stderr, " >>> SYN_ACK received\n");
        // TODO: resend this packet if no response
        while(1){
            //send ACK for SYN ACK, along with filename
            outPkt = create_packet(lowestSeq, 0, TYPE_DATA, strlen(filename), filename);
            while((wr_len = sendPkt(serverfd, outPkt, outPkt->size, true, serv_addr, sizeof(serv_addr))) < 0);
            fprintf(stderr," >>> sending ACK for SYNACK and filename\n");
            delete(outPkt);
            rd_len = recvPkt(serverfd, buffer, PACKET_SIZE, true, (struct sockaddr *)& src_addr, &src_addr_len);
            lowestSeq = setAck(inPkt);
            if(rd_len > 0 && (inPkt->type == TYPE_404 || inPkt->type == TYPE_ACK)){
                if(inPkt->type == TYPE_404) {
                    printf("### 404: the requested file does not exist\n");
                    fileFound = false;
                    while(1){
                        outPkt = create_packet(lowestSeq, 0, TYPE_ACK, 1, "0");
                        while((wr_len = sendPkt(serverfd, outPkt, outPkt->size, true, serv_addr, sizeof(serv_addr))) < 0);
                        delete(outPkt);
                        rd_len = recvPkt(serverfd, buffer, PACKET_SIZE, false, (struct sockaddr *)& src_addr, &src_addr_len);
                        // lowestSeq = setAck(inPkt);
                        //cout << "seq: " << inPkt->seq << " size: " << inPkt->size << " type: " << inPkt->type << endl;
                        //cout << "message: " << inPkt->data << endl;
                        if(rd_len > 0 && inPkt->type == TYPE_FIN) break;
                        sleep(rand()%2);
                    }
                    break;
                }
                else{
                    fileFound = true;
                    outPkt = create_packet(lowestSeq, 0, TYPE_ACK, 1, "0");
                    while((wr_len = sendPkt(serverfd, outPkt, outPkt->size, true, serv_addr, sizeof(serv_addr))) < 0);
                    delete(outPkt);
                    //fp = fopen("received.data", "w+");
                    fp = fopen(filename, "w+");
                    break;
                }
            }
        }
        break;
    }
    fprintf(stderr, " >>> Connection Initialized\n\n");
    //Request file
    if(fileFound){
        lowestSeq = setAck(inPkt); //track the lowestSeq packet that is not written to file yet
        list <packet*> recvBuffer;
        while(1){
            rd_len = recvPkt(serverfd, buffer, PACKET_SIZE, true, (struct sockaddr *)& src_addr, &src_addr_len);
            if(rd_len > 0){
                if(inPkt->type == TYPE_FIN) break;
                updatePktList(recvBuffer, lowestSeq, fp);
                int diff = inPkt->seq - lowestSeq;
                if(diff == 0){
                    fwrite(inPkt->data,sizeof(char),inPkt->size-HEADER_SIZE,fp);
                    lowestSeq = setAck(inPkt);
                    fprintf(stderr," >>> Written packet seq %d, length:%d, lowestSeq: %d\n",inPkt->seq,inPkt->size-HEADER_SIZE,lowestSeq);
                }
                else if((diff > 0 && diff < MAX_SEQ/2) || (diff < 0 && abs(diff) > MAX_SEQ/2)){
                    if(!searchPktList(recvBuffer, inPkt->seq)){
                        packet* newPkt = copy_packet(inPkt);
                        recvBuffer.push_back(newPkt);
                        fprintf(stderr," >>> Add packet seq %d, lowestSeq: %d, total: %d\n",inPkt->seq,lowestSeq, recvBuffer.size());
                    }
                }
                outPkt = create_packet(setAck(inPkt), 0, TYPE_ACK, 1, "0");
                wr_len = sendPkt(serverfd, outPkt, outPkt->size, true, serv_addr, sizeof(serv_addr));
                delete(outPkt);
            }
        }
        updatePktList(recvBuffer, lowestSeq, fp); //clear any remaining buffered packets
        fprintf(stderr, " >>> File Transmission Completed, lowestSeq: %d, recvBuffer: %d\n", lowestSeq, recvBuffer.size());
        for(auto itr = recvBuffer.begin(); !recvBuffer.empty() && itr != recvBuffer.end(); ++itr){
            fprintf(stderr, " >>> seq: %d, expectedAck: %d\n",(*itr)->seq,setAck(*itr));
        }
        fclose(fp);
    }

    fprintf(stderr, " >>> Starting close sequence.\n");
    int fin_ack = setAck(inPkt);
    int count = 0;
    while(1){
        cout << "Receiving packet " << inPkt->seq << " FIN" << endl;
        outPkt = create_packet(fin_ack, 0, TYPE_ACK, 1, "0");
        wr_len = sendPkt(serverfd, outPkt, outPkt->size, false, serv_addr, sizeof(serv_addr));
        delete(outPkt);
        outPkt = create_packet(fin_ack, 0, TYPE_FIN, 1, "0");
        wr_len = sendPkt(serverfd, outPkt, outPkt->size, true, serv_addr, sizeof(serv_addr));
        delete(outPkt);
        if(count++ > 5) break;
        rd_len = recvPkt(serverfd, buffer, PACKET_SIZE, false, (struct sockaddr *)& src_addr, &src_addr_len);
        if(rd_len > 0 && inPkt->type == TYPE_ACK) break;
        sleep(rand()%2);
    }
    close(serverfd);  // close socket
    fprintf(stderr, " >>> Finished close sequence\n\n");
    return 0;
}
