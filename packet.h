#ifndef PACKET_H
#define PACKET_H

typedef struct Packet
{
    int length;
    unsigned char sent_seq;
    unsigned char ack_seq;
    unsigned char flag;
    unsigned char unused;
    struct Payload *payload;
} packet;

typedef struct Payload
{
    int unique_nr;
    int filename_length;
    char *filename;
    struct Image *img;
} payload;

#endif