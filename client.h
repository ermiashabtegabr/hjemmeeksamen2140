#ifndef CLIENT_H
#define CLIENT_H

//Packet values
#define CONTAINS_DATA 0x1
#define TERMINATOR 0x4
#define UNUSED 0x7f

#define debug 0

//Offset
#define DATA_SEQ_OFF (sizeof(int))
#define ACK_SEQ_OFF (DATA_SEQ_OFF + sizeof(unsigned char))
#define FLAG_OFF (ACK_SEQ_OFF + sizeof(unsigned char))
#define UNUSED_OFF (FLAG_OFF + sizeof(unsigned char))
#define UNIQUE_NR_OFF (UNUSED_OFF + sizeof(unsigned char))
#define FILENAME_LEN_OFF (UNIQUE_NR_OFF + sizeof(int))
#define FILENAME_OFF (FILENAME_LEN_OFF + sizeof(int))

//Window size
#define WINDOW_SIZE 7

//Free packet malloc
#define FREE_PACKET 1

//Set time on sent packet
#define SET_SENT_TIME 1

void error(int i, char *msg);
void validate_args(int argc, char const *argv[]);
void initialize_list_variables();
void read_filenames(FILE *file_names);
int get_buf_size(FILE *pgm_filename);
struct Image *read_pgm_files(char *pgm_filename);
struct Payload *create_payload(struct Image *img, char *filename);
struct Packet *create_packet(struct Payload *pay_load, unsigned char flag);
int get_packet_size(struct Payload *payload);
void insert_packet_to_list(struct Packet *packet);
char *insert_packet_value_to_buffer(struct Packet *packet);
void free_list_malloc();

#endif