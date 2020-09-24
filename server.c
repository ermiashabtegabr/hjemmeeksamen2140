#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <dirent.h>
#include "server.h"
#include "pgmread.h"
#include "packet.h"

#define BUFSIZE 500

int ack_seq_nr = 0;

int main(int argc, char *argv[])
{
    int so, rc;
    struct sockaddr_in server_addr, client_addr;
    struct dirent *entry;
    socklen_t from_len;
    DIR *folder;

    validate_args(argc, argv);

    unsigned short port = atoi(argv[1]);
    char *dir_name = argv[2];
    char *output_filename = argv[3];

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    so = socket(AF_INET, SOCK_DGRAM, 0);
    error(so, "socket");

    rc = bind(so,
              (struct sockaddr *)&server_addr,
              sizeof(struct sockaddr_in));
    error(rc, "bind\n");

    char msg_peek[BUFSIZE];

    unsigned char flag = 0;
    while (flag != TERMINATOR)
    {
        //Run recvfrom with MSG_PEEK flag to get size of the expected packet, -
        //and with src_addr and addrlen parameters to get sockaddr of the sender to send ACK packets
        from_len = sizeof(struct sockaddr_in);
        rc = recvfrom(so,
                      msg_peek,
                      BUFSIZE - 1,
                      MSG_PEEK,
                      (struct sockaddr *)&client_addr,
                      &from_len);

        error(rc, "recvfrom msg_peek\n");

        //Receive packets
        char buf[rc + 1];
        rc = recvfrom(so,
                      buf,
                      rc,
                      0,
                      (struct sockaddr *)&client_addr,
                      &from_len);

        error(rc, "recvfrom\n");

        buf[rc] = '\0';

        char received_seq_nr = buf[DATA_SEQ_OFF];
        flag = buf[FLAG_OFF];

        printf("RECEIVED SEQ NR %d\n", received_seq_nr);

        if (flag == TERMINATOR)
        {
            printf("--- TERMINATION PACKET RECEIVED, CLOSING CONNETION ---\n");
            continue;
        }

        if (received_seq_nr != ack_seq_nr)
        {
            printf("--- WRONG PACKET RECEIVED, PACKET DISCARDED ---\n");
            continue;
        }

        int filename_length = *((int *)&buf[FILENAME_LEN_OFFSET]);
        char *filename = get_filename(filename_length, buf);

        int img_width_offset = FILENAME_OFFSET + filename_length;
        int img_heigth_offset = img_width_offset + sizeof(int);
        int img_data_offset = img_heigth_offset + sizeof(int);

        int img_width = *((int *)&buf[img_width_offset]);
        int img_height = *((int *)&buf[img_heigth_offset]);
        struct Image *img_from_packet = get_img_struct_from_packet(img_width, img_height, img_data_offset, buf);

        char *identical_filename = compare_img_with_dir_files(filename, img_from_packet, dir_name, folder, entry);
        write_output_to_file(filename, identical_filename, output_filename);

        //Send ack packets
        char *ack_buffer = create_ack_buffer();
        int ack_packet_length = sizeof(int) + (sizeof(char) * 4);

        rc = sendto(so,
                    ack_buffer,
                    ack_packet_length,
                    0,
                    (struct sockaddr *)&client_addr,
                    sizeof(struct sockaddr_in));

        error(rc, "sendto\n");

        char ack = ack_buffer[ACK_SEQ_OFF];
        printf("SENT ACK %d\n\n", ack);

        free(ack_buffer);
        free(filename);
        free(identical_filename);
        free(img_from_packet->data);
        free(img_from_packet);
    }

    close(so);
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

void validate_args(int argc, char *argv[])
{
    if (argc < 4)
    {
        printf("usage: %s <PORT> <DIRECTORY NAME> <OUTPUT FILENAME>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
}

char *create_ack_buffer()
{
    if (debug)
        printf("create_ack_buffer\n");

    int ack_packet_length = sizeof(int) + (sizeof(char) * 4);
    unsigned char sent_seq = 0;
    unsigned char ack_seq = ack_seq_nr++;
    unsigned char flag = ACK_PACKET;
    unsigned char unused = UNUSED;

    char *ack_packet = malloc(ack_packet_length);
    if (!ack_packet)
        error(-1, "malloc packet\n");

    memcpy(ack_packet, &ack_packet_length, sizeof(int));
    memcpy(ack_packet + DATA_SEQ_OFF, &sent_seq, sizeof(unsigned char));
    memcpy(ack_packet + ACK_SEQ_OFF, &ack_seq, sizeof(unsigned char));
    memcpy(ack_packet + FLAG_OFF, &flag, sizeof(unsigned char));
    memcpy(ack_packet + UNUSED_OFF, &unused, sizeof(unsigned char));

    return ack_packet;
}

struct Image *read_pgm_files(char *pgm_filename) //Reads from pgm files and creates data struct
{
    if (debug)
        printf("read_pgm_files\n");

    FILE *pgm_file = fopen(pgm_filename, "r"); //Removing new line from filename
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

char *get_filename(int filename_length, char *buf) //Returns filename from the packet received
{
    if (debug)
        printf("get_filename\n");

    char *filename = malloc(filename_length + 1);
    if (!filename)
        error(-1, "malloc filename\n");

    int index = FILENAME_OFFSET;
    int i = 0;
    char letter = buf[index++];
    while (i < filename_length)
    {
        filename[i++] = letter;
        letter = buf[index++];
    }
    filename[i] = '\0';

    return filename;
}

struct Image *get_img_struct_from_packet(int img_width, int img_height, int img_data_offset, char *buffer) //Return image struct from the received packet
{
    if (debug)
        printf("get_img_struct_from_packet\n");

    struct Image *img_struct_from_packet = malloc(sizeof(struct Image));
    if (!img_struct_from_packet)
        error(-1, "malloc img_struct_from_packet\n");

    int img_data_size = img_width * img_height;
    img_struct_from_packet->data = malloc(img_data_size + 1);
    if (!img_struct_from_packet->data)
        error(-1, "malloc img_data\n");

    char data[img_data_size];
    int index = 0;
    while (index < img_data_size)
    {
        data[index++] = buffer[img_data_offset++];
    }
    data[index] = '\0';

    memcpy(img_struct_from_packet->data, data, img_data_size + 1);

    img_struct_from_packet->width = img_width;
    img_struct_from_packet->height = img_height;

    return img_struct_from_packet;
}

char *compare_img_with_dir_files(char *filename, struct Image *img_from_packet, char *dir_name, DIR *folder, struct dirent *entry)
{ //compares img_from_packet to all pgm files
    if (debug)
        printf("compare_img_with_dir_files\n");

    char *filename_to_return = NULL;
    folder = opendir(dir_name);
    if (folder == NULL)
    {
        perror("Unable to read directory\n");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(folder)))
    {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, ".."))
        {
            int dir_length = strlen(dir_name);
            int entry_length = strlen(entry->d_name);
            char *filename = malloc(dir_length + entry_length + 2); //Pluss 2 because of "/"
            if (!filename)
                error(-1, "malloc filename\n");

            memcpy(filename, dir_name, dir_length);
            memcpy(filename + dir_length, "/", 1);
            memcpy(filename + dir_length + 1, entry->d_name, entry_length);
            filename[dir_length + entry_length + 1] = '\0';

            struct Image *image_from_file = read_pgm_files(filename);
            if (Image_compare(image_from_file, img_from_packet))
            {
                filename_to_return = malloc(entry_length + 1);
                if (!filename_to_return)
                    error(-1, "malloc filename_to_return\n");

                memcpy(filename_to_return, entry->d_name, entry_length);
                filename_to_return[entry_length] = '\0';
            }
            Image_free(image_from_file);
            free(filename);
        }
    }

    closedir(folder);
    return filename_to_return;
}

void write_output_to_file(char *filename, char *identical_filename, char *output_filename) //Writes the result from compare_img_with_dir_files to file
{
    if (debug)
        printf("write_output_to_file\n");

    FILE *output_file = fopen(output_filename, "a+");
    if (!output_file)
    {
        printf("File '%s' not found\n", output_filename);
        exit(EXIT_FAILURE);
    }

    if (identical_filename)
        fprintf(output_file, "< %s > < %s >\n", filename, identical_filename);
    else
        fprintf(output_file, "< %s > UNKNOWN\n", filename);

    fclose(output_file);
}
