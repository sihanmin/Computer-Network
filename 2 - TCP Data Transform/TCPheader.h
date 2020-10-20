//
//  TCPheader.h
//  proj2
//

#ifndef TCPheader_h
#define TCPheader_h

#include <stdio.h>
#include <string.h>
using namespace std;

#define MAX_SEQ 30720
#define PACKET_SIZE 1024
#define HEADER_SIZE 16
#define DATA_SIZE (PACKET_SIZE - HEADER_SIZE)
#define TIME_OUT 0.5
#define TYPE_DATA 1
#define TYPE_ACK 2
#define TYPE_SYN 3
#define TYPE_FIN 4
#define TYPE_RETRANSMISSION 5
#define TYPE_404 6

struct packet{
    int seq;
    int wnd;
    int type; // 1 data; 2 ack; 3 syn; 4 fin; 5 retransmision
    int size;
    char data[DATA_SIZE];
};

packet* create_packet(int seq, int wnd, int type, int data_size, char* data);

packet* copy_packet(packet* src);

/* casting example
struct packet *test = create_packet(100, 1000, TYPE_DATA, 3, "abc");
char buf[PACKET_SIZE];

 from packet to buffer array:
memcpy(buf, (char *) test, PACKET_SIZE);
 from buffer array to packet:
struct packet *test_back = (struct packet*) buf;
 
 */
#endif /* TCPheader_h */

//destructor