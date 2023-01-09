#include "networking.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <arpa/inet.h>

#include "logging.h"

// max size of buffer (change if needed)
#define MAX_BUFFER_SIZE 10 * 1024 * 1024

/* ########## Structs implementation ########## */

/*
  Creates a new Message

  @size: size of payload
  @payload: data read

  @return: Message object (heap allocation)
*/
Message create_message(unsigned int size, unsigned char *payload) {
  Message res = malloc(sizeof(*res));
  if(res == NULL) {
    ERROR("Failed allocation of Message struct");
    return NULL;
  }

  res->size = size;
  res->payload = payload;

  return res;
}


/*
  Destroy a Message (release memory allocated)

  @m: Message
*/
void destroy_message(Message m) {
  if(m == NULL) return;

  if(m->payload != NULL) {
    free(m->payload);
  }

  free(m);
}

/*
  Convert from u8 to ResultCode

  @code: code.

  @return: ResultCode. If the code is invalid (i.e. does not match any enum), returns ResultCode_GenericError
*/
ResultCode u8_to_result_code(uint8_t code) {
  if(code > ResultCode_GenericError) return ResultCode_GenericError;
  return code;
}


/*
  Convert from ResultCode to u8
  This function is used because ResultCode is stored as int (16 bits)
  and we want to make sure the conversion doesn't produce errors

  @code: ResultCode.

  @return: u8 representation of the code
*/
uint8_t result_code_to_u8(ResultCode code) {
  return code;
}


/*
  Creates a new ResultMessage

  @code: ResultCode
  @m: Message

  @return: ResultMessage object (heap allocation)
*/
ResultMessage create_result_message(ResultCode code, Message m) {
  ResultMessage res = malloc(sizeof(*res));
  if(res == NULL) {
    ERROR("Failed allocation of ResultMessage struct");
    destroy_message(m);
    return NULL;
  }

  res->code = code;
  res->message = m;

  return res;
}

/*
  Destroy a ResultMessage (release memory allocated)

  @m: ResultMessage
*/
void destroy_result_message(ResultMessage m) {
  if(m == NULL) return;
  destroy_message(m->message);
  free(m);
}


/*
  Convert from u8 to CommandCode

  @code: code.

  @return: CommandCode. If the code is invalid (i.e. does not match any enum), returns CommandCode_Invalid
*/
CommandCode u8_to_command_code(uint8_t code) {
  if(code > CommandCode_Invalid) return CommandCode_Invalid;
  return code;
}


/*
  Convert from CommandCode to u8
  This function is used because CommandCode is stored as int (16 bits)
  and we want to make sure the conversion doesn't produce errors

  @code: CommandCode.

  @return: u8 representation of the code
*/
uint8_t command_code_to_u8(CommandCode code){
  return code;
}


/*
  Creates a new CommandMessage

  @code: CommandCode
  @m: Message

  @return: CommandMessage object (heap allocation)
*/
CommandMessage create_command_message(CommandCode code, Message m) {
  CommandMessage res = malloc(sizeof(*res));
  if(res == NULL) {
    ERROR("Failed allocation of CommandMessage struct");
    destroy_message(m);
    return NULL;
  }

  res->code = code;
  res->message = m;

  return res;
}

/*
  Destroy a CommandMessage (release memory allocated)

  @m: CommandMessage
*/
void destroy_command_message(CommandMessage m) {
  if(m == NULL) return;
  destroy_message(m->message);
  free(m);
}


/* ########## Read / write functions ########## */

int read_byte(int fd, unsigned char* b) {
  int nread = read(fd, b, 1);
  if(nread != 1) return NETWORK_FAILURE;
  return NETWORK_SUCCESS;
}

int write_byte(int fd, unsigned char b) {
  int nwritten = write(fd, &b, 1);
  if(nwritten != 1) return NETWORK_FAILURE;
  return NETWORK_SUCCESS;
}


int read_buf(int fd, unsigned char* buf, unsigned int size) {
  unsigned int total_read = 0;

  //DEBUG("Reading buf with size: %u", size);

  while(1) {
    int nread = read(fd, buf + total_read, size - total_read);

    if(nread <= 0) {
      WARNING("Read retval: %d", nread);
      return NETWORK_FAILURE;
    }

    total_read += nread;
    //DEBUG("Read %u bytes. %u to go", nread, size - total_read);

    if(total_read > size) {
      WARNING("Read more than expected: %u/%u", total_read, size);
      return NETWORK_FAILURE;
    } else if(total_read == size) {
      break;
    }
  }

  return NETWORK_SUCCESS;
}

int write_buf(int fd, unsigned char* buf, unsigned int size) {
  unsigned int total_written = 0;

  //DEBUG("Writing buffer of %u bytes", size);

  while(1) {
    int nwritten = write(fd, buf + total_written, size - total_written);

    if(nwritten <= 0) {
      WARNING("Write retval: %d", nwritten);
      return NETWORK_FAILURE;
    }

    total_written += nwritten;
    //DEBUG("Written %u bytes. %u to go", nwritten, size - total_written);

    if(total_written > size) {
      WARNING("Written more than expected: %u/%u", total_written, size);
      return NETWORK_FAILURE;
    } else if(total_written == size) {
      break;
    }
  }

  return NETWORK_SUCCESS;
}


/*
  Read a u16, performing conversion from network to host byte order

  @return: u16 read
*/
int read_u16(int fd, uint16_t* val) {
  unsigned char buf[2];
  if(read_buf(fd, buf, 2) == NETWORK_FAILURE) return NETWORK_FAILURE;

  *val = ntohs(*(uint16_t *) buf);
  return NETWORK_SUCCESS;
}


/*
  Write a u16, performing conversion from host to network byte order

  @val: u16 to write
*/
int write_u16(int fd, uint16_t val) {
  uint16_t val_n = htons(val);
  return write_buf(fd, (unsigned char *) &val_n, 2);
}


/*
  Read a u32, performing conversion from network to host byte order

  @return: u32 read
*/
int read_u32(int fd, uint32_t* val) {
  unsigned char buf[4];
  if(read_buf(fd, buf, 4) == NETWORK_FAILURE) return NETWORK_FAILURE;

  *val = ntohl(*(uint32_t *) buf);
  return NETWORK_SUCCESS;
}


/*
  Read a message of the format [len u16 - payload]

  @return: Message: caller takes ownership of the data and destroy it
                    using destroy_message when he's done using it
*/
Message read_message(int fd, unsigned int size) {
  unsigned char *buf;

  if(size == 0) {
    // nothing to read
    buf = NULL;
  }
  else {
    buf = malloc(size * sizeof(unsigned char));
    if(buf == NULL) {
      ERROR("Failed allocation of %u bytes", size);
      return NULL;
    }

    if(read_buf(fd, buf, size) == NETWORK_FAILURE) {
      ERROR("Failed to read message");
      free(buf);
      return NULL;
    }
  }

  Message m = create_message(size, buf);
  if(m == NULL) {
    free(buf);
  }
  return m;
}


/*
  Write a message of the format [len u16 - payload]

  @m: Message to write
*/
int write_message(int fd, Message m) {
  if(write_u16(fd, m->size) == NETWORK_FAILURE) return NETWORK_FAILURE;

  if(m->size > 0) {
    return write_buf(fd, m->payload, m->size);
  }

  return NETWORK_SUCCESS;
}


/*
  Read a message of the format [code u8 - len u16 - payload]

  @return: ResultMessage: caller takes ownership of the data and destroy it
                    using destroy_result_message when he's done using it
*/
ResultMessage read_result_message(int fd) {
  uint8_t code;
  uint16_t size;
  if(read_byte(fd, &code) == NETWORK_FAILURE) return NULL;
  if(read_u16(fd, &size) == NETWORK_FAILURE) return NULL;

  Message m = read_message(fd, size);
  if(m == NULL) return NULL;
  return create_result_message(u8_to_result_code(code), m);
}


/*
  Write a message of the format [code u8 - len u16 - payload]

  @m: ResultMessage to write
*/
int write_result_message(int fd, ResultMessage m) {
  if(write_byte(fd, result_code_to_u8(m->code)) == NETWORK_FAILURE) return NETWORK_FAILURE;
  return write_message(fd, m->message);
}


/*
  Read a message of the format [command u8 - len u16 - payload]

  @return: CommandMessage: caller takes ownership of the data and destroy it
                    using destroy_command_message when he's done using it
*/
CommandMessage read_command_message(int fd) {
  uint8_t code;
  unsigned int size;
  if(read_byte(fd, &code) == NETWORK_FAILURE) return NULL;

  CommandCode command_code = u8_to_command_code(code);

  if(command_code == CommandCode_LoadSM) {
    //DEBUG("Reading LoadSM message");
    uint32_t size_u32;
    if(read_u32(fd, &size_u32) == NETWORK_FAILURE) return NULL;

    if(size_u32 > MAX_BUFFER_SIZE) {
      ERROR("Payload too big: %u/%u", size_u32, MAX_BUFFER_SIZE);
      return NULL;
    }

    size = size_u32;
  } else {
    uint16_t size_u16;
    if(read_u16(fd, &size_u16) == NETWORK_FAILURE) return NULL;
    size = size_u16;
  }

  Message m = read_message(fd, size);
  if(m == NULL) return NULL;
  return create_command_message(command_code, m);
}


/*
  Write a message of the format [command u8 - len u16 - payload]

  @m: CommandMessage to write
*/
int write_command_message(int fd, CommandMessage m) {
  if(write_byte(fd, command_code_to_u8(m->code)) == NETWORK_FAILURE) return NETWORK_FAILURE;
  return write_message(fd, m->message);
}
