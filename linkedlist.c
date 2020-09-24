#include <time.h>
#include <string.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "linkedlist.h"
#include "pgmread.h"
#include "packet.h"
#include "client.h"

void push(linkedlist *list, struct Packet *packet, int set_sent_time)
{
    if (debug)
        printf("push\n");

    node_s *node = malloc(sizeof(node_s));
    if (!node)
    {
        printf("malloc node\n");
        exit(EXIT_FAILURE);
    }

    node->packet = packet;
    set_between(list->tail->prev, node, list->tail);
    if (set_sent_time)
        time(&node->sent_time);
    list->size++;
}

void set_between(node_s *left, node_s *middle, node_s *right)
{
    if (debug)
        printf("set_between\n");

    left->next = middle;
    middle->prev = left;
    middle->next = right;
    right->prev = middle;
}

void pop(linkedlist *list, int free_packet)
{
    if (debug)
        printf("pop\n");

    node_s *node = list->head->next;

    if (node == list->tail)
        return;
    else
        remove_node(node);

    if (free_packet)
        free_node_memory(node);
    else
        free(node);
    list->size--;
}

void remove_node(node_s *node)
{
    if (debug)
        printf("remove_node\n");

    node_s *left = node->prev;
    node_s *right = node->next;
    left->next = right;
    right->prev = left;
}

void free_node_memory(node_s *node)
{
    if (debug)
        printf("free_node_memory\n");

    packet *packet = node->packet;
    payload *payload = packet->payload;
    if (payload)
    {
        free(payload->filename);
        struct Image *image = payload->img;
        Image_free(image);
    }
    free(payload);
    free(packet);
    free(node);
}