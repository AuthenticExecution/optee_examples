#ifndef __NETWORKING_H__
#define __NETWORKING_H__

#include <stddef.h>
#include <stdint.h>

#define NETWORK_SUCCESS 1
#define NETWORK_FAILURE 0

typedef struct message {
  unsigned int size;
  unsigned char *payload;
} *Message;

Message create_message(unsigned int size, unsigned char *payload);
void destroy_message(Message m);

typedef enum {
    ResultCode_Ok,
    ResultCode_IllegalCommand,
    ResultCode_IllegalPayload,
    ResultCode_InternalError,
    ResultCode_BadRequest,
    ResultCode_CryptoError,
    ResultCode_GenericError
} ResultCode;

ResultCode u8_to_result_code(uint8_t code);
uint8_t result_code_to_u8(ResultCode code);

typedef struct result {
  ResultCode code;
  Message message;
} *ResultMessage;

#define RESULT(code)  create_result_message(code, create_message(0, NULL))
#define RESULT_DATA(code, size, payload)  create_result_message(code, create_message(size, payload))

ResultMessage create_result_message(ResultCode code, Message m);
void destroy_result_message(ResultMessage m);

typedef enum {
    CommandCode_AddConnection,
    CommandCode_CallEntrypoint,
    CommandCode_RemoteOutput,
    CommandCode_LoadSM,
    CommandCode_Ping,
    CommandCode_RegisterEntrypoint,
    CommandCode_Invalid
} CommandCode;

CommandCode u8_to_command_code(uint8_t code);
uint8_t command_code_to_u8(CommandCode code);

typedef struct command {
  CommandCode code;
  Message message;
} *CommandMessage;

CommandMessage create_command_message(CommandCode code, Message m);
void destroy_command_message(CommandMessage m);


int read_byte(int fd, unsigned char* b);
int write_byte(int fd, unsigned char b);


int read_buf(int fd, unsigned char* buf, unsigned int size);
int write_buf(int fd, unsigned char* buf, unsigned int size);


int read_u16(int fd, uint16_t *val);
int write_u16(int fd, uint16_t val);


int read_u32(int fd, uint32_t *val);


Message read_message(int fd, unsigned int size);
int write_message(int fd, Message m);


ResultMessage read_result_message(int fd);
int write_result_message(int fd, ResultMessage m);


CommandMessage read_command_message(int fd);
int write_command_message(int fd, CommandMessage m);

#endif
