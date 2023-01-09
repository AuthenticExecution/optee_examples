#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdint.h>
#include <unistd.h>
#include <netinet/in.h>

typedef struct ParseState ParseState;

ParseState* create_parse_state(uint8_t* buf, unsigned int len);
void free_parse_state(ParseState* state);
int parse_byte(ParseState* state, uint8_t* byte);
int parse_int(ParseState* state, uint16_t* i);
int parse_string(ParseState* state, char** str);
int parse_raw_data(ParseState* state, unsigned int len, uint8_t** buf);
int parse_all_raw_data(ParseState* state, uint8_t** buf, unsigned int* len);

int connect_to_server(struct in_addr address, uint16_t port, int *fd);

#endif