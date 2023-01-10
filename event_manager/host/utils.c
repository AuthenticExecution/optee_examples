#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#ifdef MEASURE_TIME
#include <time.h>
#endif

#include "logging.h"

struct ParseState
{
    uint8_t* buf;
    unsigned int   len;
};

ParseState* create_parse_state(uint8_t* buf, unsigned int len)
{
    ParseState* state = malloc(sizeof(ParseState));
    state->buf = buf;
    state->len = len;
    return state;
}

void free_parse_state(ParseState* state)
{
    free(state);
}

static void advance_state(ParseState* state, unsigned int len)
{
    state->buf += len;
    state->len -= len;
}

int parse_byte(ParseState* state, uint8_t* byte)
{
    if (state->len < 1)
        return 0;

    *byte = state->buf[0];
    advance_state(state, 1);
    return 1;
}

int parse_int(ParseState* state, uint16_t* i)
{
    if (state->len < 2)
        return 0;

    uint8_t msb = state->buf[0];
    uint8_t lsb = state->buf[1];
    *i = (msb << 8) | lsb;
    advance_state(state, 2);
    return 1;
}

int parse_string(ParseState* state, char** str)
{
    uint8_t* end = memchr(state->buf, 0x00, state->len);

    if (end == NULL)
        return 0;

    unsigned int str_len = end - state->buf;
    *str = (char*)state->buf;
    advance_state(state, str_len + 1);
    return 1;
}

int parse_raw_data(ParseState* state, unsigned int len, uint8_t** buf)
{
    if (state->len < len)
        return 0;

    *buf = state->buf;
    advance_state(state, len);
    return 1;
}

int parse_all_raw_data(ParseState* state, uint8_t** buf, unsigned int* len)
{
    *buf = state->buf;
    *len = state->len;
    advance_state(state, state->len);
    return 1;
}

int connect_to_server(struct in_addr address, uint16_t port, int *fd) {
    int sock;
    struct sockaddr_in server_addr;

    // Creating socket file descriptor
	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		ERROR("socket creation failed");
		return 0;
	}

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr = address;
    server_addr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr*) &server_addr, sizeof(server_addr)) != 0) {
		ERROR("Connection to server failed");
        return 0;
    }

    *fd = sock;
    return 1;
}

void measure_time(char *msg) {
#ifdef MEASURE_TIME
    struct timespec tms;

    if (clock_gettime(CLOCK_REALTIME,&tms)) {
        WARNING("Failed to measure time");
        return;
    }

    uint64_t time_micros = tms.tv_nsec / 1000 + (tms.tv_nsec % 1000 >= 500 ? 1 : 0);
    INFO("tz_%s: %lu%06lu us", msg, tms.tv_sec, time_micros);
#endif
}