#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "client.h"
#include "pgmread.h"
#include "packet.h"
#include "linkedlist.h"
#include "send_packet.h"

#define BUFSIZE 250

//Linked list for storing packets
linkedlist *main_list; //Stores all packets read from file
linkedlist *sent_list; //Stores all packets sent to server that have yet to receive ack

int lower_bound = 0;
int upper_bound = 0;

int unique_nr = 100;
int seq_nr = 0;

int received_all_ack_packets = 0;
int sent_all_packets = 0;
int sent_termination_packet = 0;

int main(int argc, char const *argv[])
{
    int so, rc;
    struct in_addr ip_address;
    struct sockaddr_in server_addr;
    struct timeval timeout;
    double diff_t;
    fd_set set;
    time_t end;

    validate_args(argc, argv);

    char const *IP = argv[1];
    unsigned short dest_port = atoi(argv[2]);
    char const *filename = argv[3];
    float drop_percentage = strtof(argv[4], NULL) / 100;

    inet_pton(AF_INET, IP, &ip_address);
    set_loss_probability(drop_percentage);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(dest_port);
    server_addr.sin_addr = ip_address;

    so = socket(AF_INET, SOCK_DGRAM, 0);
    error(so, "socket");

    initialize_list_variables();

    FILE *file = fopen(filename, "r");
    if (!file)
    {
        printf("File '%s' not found \n", filename);
        return EXIT_FAILURE;
    }

    read_filenames(file);

    node_s *current_node = main_list->head->next;
    packet *packet_to_send = current_node->packet;

    node_s *first_sent;
    time_t time_of_first_sent;

    while (!sent_termination_packet)
    {
        while ((upper_bound - lower_bound) < WINDOW_SIZE && (main_list->size != 0 || packet_to_send->flag == TERMINATOR)) //Sending packets
        {
            if (packet_to_send->flag == TERMINATOR) //Since the termination packet cannot get lost, it is not pushed to sent_list
            {
                pop(main_list, !FREE_PACKET); //Waiting for the termination packet to be sent before freeing the memory allocated for the packet
            }
            else
            {
                pop(main_list, !FREE_PACKET);                   //Removing the packet sent from main and inserting it to the sent list
                push(sent_list, packet_to_send, SET_SENT_TIME); //Pushing and setting time of the sent packet
            }

            if (packet_to_send->flag == TERMINATOR && !received_all_ack_packets) //To not send termination packet before all other packets have received correct ack
            {
                break;
            }
            else if (packet_to_send->flag == TERMINATOR)
            {
                sent_termination_packet = 1;
            }

            printf("\n--- SENDING PACKET ---\n");

            char *buffer_packet = insert_packet_value_to_buffer(packet_to_send);

            rc = send_packet(so,
                             buffer_packet,
                             packet_to_send->length,
                             0,
                             (struct sockaddr *)&server_addr,
                             sizeof(struct sockaddr_in));

            error(rc, "send_packet\n");
            free(buffer_packet);

            printf("SEQ NR %d\n", packet_to_send->sent_seq);

            if (packet_to_send->flag != TERMINATOR)
                upper_bound++;

            if (!sent_termination_packet) //If the termination packet is sent then main_list -> head -> next would be the tail
            {
                current_node = main_list->head->next;
                packet_to_send = current_node->packet;
            }
            else
            {
                printf("\n--- TERMINATION PACKET SENT, CLOSING CONNECTION ---\n");
                break;
            }
            printf("\n");
        }

        if (main_list->size == 1) //If the remaining packet in main list is 1, then it is the termination packet and all others have been sent
            sent_all_packets = 1;

        if (sent_list->size != 0)
        {
            first_sent = sent_list->head->next;
            time_of_first_sent = first_sent->sent_time;
        }

        time(&end);
        FD_ZERO(&set);

        while (difftime(end, time_of_first_sent) < 5 && (upper_bound - lower_bound == WINDOW_SIZE || sent_all_packets)) //Receiving ACK packets
        {
            if (sent_all_packets && sent_list->size == 0)
            {
                received_all_ack_packets = 1;
                break;
            }

            printf("--- WAINTING FOR ACK ---\n");

            FD_SET(so, &set);
            timeout.tv_sec = 0;
            timeout.tv_usec = 500000;
            rc = select(FD_SETSIZE, &set, NULL, NULL, &timeout);
            error(rc, "select\n");

            char buf[BUFSIZE];
            if (rc != 0 && FD_ISSET(so, &set))
            {
                rc = recvfrom(so, buf, BUFSIZE, 0, NULL, NULL);
                error(rc, "read\n");
                buf[rc - 1] = '\0';

                unsigned char ack_seq = buf[ACK_SEQ_OFF];
                unsigned char expected_ack = sent_list->head->next->packet->sent_seq;

                if (ack_seq == expected_ack)
                {
                    printf("RECEIVED ACK NR %d\n", ack_seq);
                    pop(sent_list, FREE_PACKET);

                    if (sent_list->head->next != sent_list->tail)
                    {
                        first_sent = sent_list->head->next;
                        time_of_first_sent = first_sent->sent_time;
                    }

                    lower_bound++;
                }
            }

            time(&end);
        }
        time(&end);

        if (sent_list->size != 0 && difftime(end, time_of_first_sent) >= 5) //There are packets sent and have not received ack, and therefore have to be sent again
        {
            printf("\n--- TIMEOUT, RESENDING PACKETS ---\n");
            upper_bound -= sent_list->size;
            packet *packet_to_resend = first_sent->packet;
            while (first_sent != sent_list->tail)
            {
                time(&first_sent->sent_time); //Setting new time to the resent packets
                char *resend_buffer = insert_packet_value_to_buffer(packet_to_resend);
                rc = send_packet(so,
                                 resend_buffer,
                                 packet_to_resend->length,
                                 0,
                                 (struct sockaddr *)&server_addr,
                                 sizeof(struct sockaddr_in));

                error(rc, "send_packet\n");

                printf("SEQ NR %d\n", packet_to_resend->sent_seq);

                upper_bound++;
                first_sent = first_sent->next;
                packet_to_resend = first_sent->packet;
                free(resend_buffer);
            }
            printf("\n");
        }
    }

    free(packet_to_send); //Free packet from termination packet
    close(so);
    fclose(file);
    free_list_malloc();
    return EXIT_SUCCESS;
}

void error(int i, char *msg)
{
    if (i == -1)
    {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}

void validate_args(int argc, char const *argv[])
{
    if (argc < 5)
    {
        printf("usage: %s <IP> <PORT> <FILENAME> <DROP PERCENTAGE>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    else if (atoi(argv[4]) < 0 || atoi(argv[4]) > 20)
    {
        printf("Drop percentage should be between 0-20\n");
        printf("usage: %s <IP> <PORT> <FILENAME> <DROP PERCENTAGE>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
}

void initialize_list_variables() //Initialize and allocate memory for linked list variables
{
    if (debug)
        printf("initialize_list_variables\n");

    main_list = malloc(sizeof(linkedlist));
    if (!main_list)
        error(-1, "malloc main_list\n");

    sent_list = malloc(sizeof(linkedlist));
    if (!sent_list)
        error(-1, "malloc sent_list\n");

    main_list->head = malloc(sizeof(node_s));
    if (!main_list->head)
        error(-1, "malloc main_list -> head\n");

    main_list->tail = malloc(sizeof(node_s));
    if (!main_list->tail)
        error(-1, "malloc main_list -> tail\n");

    sent_list->head = malloc(sizeof(node_s));
    if (!sent_list->head)
        error(-1, "malloc sent_list -> head\n");

    sent_list->tail = malloc(sizeof(node_s));
    if (!sent_list->tail)
        error(-1, "malloc sent_list -> tail\n");

    main_list->size = 0;
    sent_list->size = 0;

    main_list->head->next = main_list->tail;
    main_list->tail->prev = main_list->head;

    sent_list->head->next = sent_list->tail;
    sent_list->tail->prev = sent_list->head;
}

char *insert_packet_value_to_buffer(struct Packet *packet)
{
    if (debug)
        printf("insert_packet_value_to_buffer\n");

    int packet_length = packet->length;
    char *buf = malloc(packet_length + 1);
    if (!buf)
        error(-1, "malloc buf\n");

    //Header
    memcpy(buf, &packet_length, sizeof(int));
    memcpy(buf + DATA_SEQ_OFF, &packet->sent_seq, sizeof(unsigned char));
    memcpy(buf + ACK_SEQ_OFF, &packet->ack_seq, sizeof(unsigned char));
    memcpy(buf + FLAG_OFF, &packet->flag, sizeof(unsigned char));
    memcpy(buf + UNUSED_OFF, &packet->unused, sizeof(unsigned char));

    if (packet->flag != TERMINATOR)
    {
        //Payload
        memcpy(buf + UNIQUE_NR_OFF, &packet->payload->unique_nr, sizeof(int));
        memcpy(buf + FILENAME_LEN_OFF, &packet->payload->filename_length, sizeof(int));

        int filename_length = packet->payload->filename_length;
        memcpy(buf + FILENAME_OFF, packet->payload->filename, filename_length);

        //Image
        long img_width_offset = FILENAME_OFF + filename_length;
        int img_width = packet->payload->img->width;
        memcpy(buf + img_width_offset, &img_width, sizeof(int));

        long img_height_offset = img_width_offset + sizeof(int);
        int img_height = packet->payload->img->height;
        memcpy(buf + img_height_offset, &img_height, sizeof(int));

        long img_data_offset = img_height_offset + sizeof(int);
        int img_size = img_width * img_height;
        memcpy(buf + img_data_offset, packet->payload->img->data, img_size);
    }
    buf[packet_length] = '\0';

    return buf;
}

//Reading from file

void read_filenames(FILE *filenames) //Reads pgm filenames from file and constructs packets that are later pushed to main_list
{
    if (debug)
        printf("read_filenames\n");

    char buf[BUFSIZE];
    while (fgets(buf, BUFSIZE, filenames))
    {
        struct Image *img = read_pgm_files(buf);
        payload *payload = create_payload(img, buf);
        packet *packet = create_packet(payload, CONTAINS_DATA);
        push(main_list, packet, !SET_SENT_TIME);
    }
    packet *term_packet = create_packet(NULL, TERMINATOR);
    push(main_list, term_packet, !SET_SENT_TIME);
}

struct Image *read_pgm_files(char *pgm_filename) //Reads from pgm files and creates Image struct with given method Image_create
{
    if (debug)
        printf("read_pgm_files\n");

    FILE *pgm_file = fopen(strtok(pgm_filename, "\n"), "r"); //Removing new line from filename
    if (pgm_file == NULL)
    {
        printf("File '%s' not found \n", pgm_filename);
        exit(EXIT_FAILURE);
    }

    int size = get_buf_size(pgm_file);
    char buffer[size + 1];

    fseek(pgm_file, 0, SEEK_SET);
    char value;
    int index = 0;
    while (!feof(pgm_file))
    {
        fscanf(pgm_file, "%c", &value);
        buffer[index++] = value;
    }
    buffer[size] = '\0';

    struct Image *img = Image_create(buffer);
    fclose(pgm_file);
    return img;
}

int get_buf_size(FILE *pgm_filename) //Goes through pgm file and counts each character and return the value
{
    if (debug)
        printf("get_buf_size\n");

    int size = 0;
    char value;
    while (!feof(pgm_filename))
    {
        fscanf(pgm_filename, "%c", &value);
        size++;
    }
    return size;
}

//Creating packets

payload *create_payload(struct Image *img, char *name)
{
    if (debug)
        printf("create_payload\n");

    strtok(name, "/");
    char *filename = strtok(NULL, "\n");
    int name_length = strlen(filename) + 1;

    payload *payload = malloc(sizeof(struct Payload));
    if (!payload)
        error(-1, "malloc payload\n");

    payload->unique_nr = unique_nr++;
    payload->filename_length = name_length;

    payload->filename = malloc(name_length);
    if (!payload->filename)
        error(-1, "malloc payload -> file_name\n");
    memcpy(payload->filename, filename, name_length);

    payload->img = img;
    return payload;
}

packet *create_packet(payload *payload, unsigned char flag)
{
    if (debug)
        printf("create_packet\n");
    int packet_length = get_packet_size(payload);

    packet *packet = malloc(sizeof(struct Packet));
    if (!packet)
        error(-1, "malloc packet\n");

    packet->length = packet_length;
    packet->sent_seq = seq_nr++;
    packet->ack_seq = 0;
    packet->flag = flag;
    packet->unused = UNUSED;
    packet->payload = payload;

    return packet;
}

int get_packet_size(payload *payload) //Returns the total size of a packet without padding
{
    if (debug)
        printf("get_buf_size\n");

    if (!payload)
        return (sizeof(int) + sizeof(unsigned char) * 4); //Termination packet

    int img_width = payload->img->width;
    int img_heigth = payload->img->height;
    int img_data_size = img_width * img_heigth;
    int img_struct_size = img_data_size + sizeof(img_width) + sizeof(img_heigth);

    int unique_nr = payload->unique_nr;
    int filename_length = payload->filename_length;
    int filename_size = filename_length;

    int payload_struct_size = img_struct_size + sizeof(unique_nr) + sizeof(filename_length) + filename_size;
    int packet_size = payload_struct_size + (sizeof(unsigned char) * 4) + sizeof(int);
    return packet_size;
}

//Freeing memory

void free_list_malloc() //Frees the memory allocated by the linked list
{
    if (debug)
        printf("free_malloc\n");

    free(main_list->head);
    free(main_list->tail);
    free(sent_list->head);
    free(sent_list->tail);
    free(main_list);
    free(sent_list);
}
