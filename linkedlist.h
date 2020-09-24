#ifndef LINKEDLIST_H
#define LINKEDLIST_H

typedef struct Node
{
    struct Node *next;
    struct Node *prev;
    struct Packet *packet;
    time_t sent_time;
} node_s;

typedef struct Linkedlist
{
    node_s *head, *tail;
    int size;
} linkedlist;

void push(linkedlist *list, struct Packet *packet, int set_sent_time);
void set_between(node_s *left, node_s *middle, node_s *right);
void pop(linkedlist *list, int free_packet);
void remove_node(node_s *node);
void free_node_memory(node_s *node);

#endif