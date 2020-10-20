//
//  TCPheader.cpp
//  proj2
//

#include "TCPheader.h"

packet* create_packet(int seq, int wnd, int type, int data_size, char* data)
{
    if (data_size > DATA_SIZE)// || data_size > strlen(data))
    return NULL;
    
    struct packet *temp = new struct packet;
    temp->seq = seq;
    temp->wnd = wnd;
    temp->type = type;
    temp->size = data_size + HEADER_SIZE;
    memset(temp->data,'\0',DATA_SIZE);
    memcpy(temp->data, data, data_size);
    
    return temp;
}

packet* copy_packet(packet* src)
{
    struct packet* res = new struct packet;
    res->seq = src->seq;
    res->wnd = src->wnd;
    res->type = src->type;
    res->size = src->size;
    memset(res->data,'\0',DATA_SIZE);
    memcpy(res->data,src->data,src->size-HEADER_SIZE);
    return res;
}
