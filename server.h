#ifndef CLIENT_H
#define CLIENT_H

//Packet value
#define ACK_PACKET 0x2
#define TERMINATOR 0x4
#define UNUSED 0x7f
#define debug 0

//Offset
#define DATA_SEQ_OFF (sizeof(int))
#define ACK_SEQ_OFF (DATA_SEQ_OFF + sizeof(unsigned char))
#define FLAG_OFF (ACK_SEQ_OFF + sizeof(unsigned char))
#define UNUSED_OFF (FLAG_OFF + sizeof(unsigned char))
#define FILENAME_LEN_OFFSET (UNUSED_OFF + sizeof(int) + sizeof(unsigned char))
#define FILENAME_OFFSET (FILENAME_LEN_OFFSET + sizeof(int))

int get_buf_size(FILE *pgm_filename);
void error(int i, char *msg);
void validate_args(int argc, char *argv[]);
void read_from_directory(char *dir_name, DIR *folder, struct dirent *entry);
void write_output_to_file(char *filename, char *identical_filename, char *output_filename);
struct Image *get_img_struct_from_packet(int img_width, int img_height, int img_data_offset, char *buffer);
struct Image *read_pgm_files(char *pgm_filename);
char *create_ack_buffer();
char *get_filename(int filename_length, char *buf);
char *compare_img_with_dir_files(char *filename,
                                 struct Image *image_from_packet,
                                 char *dir_name,
                                 DIR *folder,
                                 struct dirent *entry);

#endif