#include "enclave_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <arpa/inet.h>

#include "logging.h"
#include "command_handlers.h"
#include "connection.h"
#include "module.h"
#include "utils.h"

#define REMOTE_CONNECTION_HAS_RESPONSE 0

ResultMessage load_enclave(CommandMessage m) {
  return load_module(m->message->payload, m->message->size);
}


static int is_local_connection(Connection* connection) {
  return (int) connection->local;
}

static void handle_local_connection(
  Connection* connection,
  void* data,
  unsigned int len
) {
    reactive_handle_input(connection->to_sm, connection->conn_id, data, len);
}

static void handle_remote_connection(Connection* connection,
                                     void* data, unsigned int len) {
    unsigned char *payload = malloc(len + 4);
    if(payload == NULL) {
      WARNING("Cannot allocate output payload");
      return;
    }

    uint16_t sm_id = htons(connection->to_sm);
    uint16_t conn_id = htons(connection->conn_id);

    memcpy(payload, &sm_id, 2);
    memcpy(payload + 2, &conn_id, 2);
    memcpy(payload + 4, data, len);

    CommandMessage m = create_command_message(
            CommandCode_RemoteOutput,
            create_message(len + 4, payload)
    );

    // open socket
    int sock;
    if(!connect_to_server(connection->to_address, connection->to_port, &sock)) {
      WARNING("Could not connect to remote EM");
    } else {
      if(write_command_message(sock, m) == NETWORK_FAILURE) {
        WARNING("Failed to send payload");
      } else if(REMOTE_CONNECTION_HAS_RESPONSE) {
        ResultMessage res = read_result_message(sock);

        if(res == NULL) {
          WARNING("Failed to read response from remote EM");
        } else if(res->code != ResultCode_Ok) {
          WARNING("Received response from remote EM: %d", res->code);
        }

        destroy_result_message(res);
      }

      close(sock);
    }

    destroy_command_message(m);
}


void reactive_handle_output(uint16_t conn_id, void* data, unsigned int len) {
  // data is owned by the caller, we don't manage its memory
  Connection* connection = connections_get(conn_id);

  if (connection == NULL) {
      WARNING("no connection for id %u", conn_id);
      return;
  }

  DEBUG(
      "accepted output %u to be delivered at %s:%d %d",
      connection->conn_id,
      inet_ntoa(connection->to_address),
      connection->to_port,
      connection->to_sm
  );

  if (is_local_connection(connection))
      handle_local_connection(connection, data, len);
  else
      handle_remote_connection(connection, data, len);
}


void reactive_handle_input(uint16_t sm, uint16_t conn_id, void* data, unsigned int len) {
    DEBUG("Calling handle_input of sm %d", sm);
    handle_input(sm, conn_id, data, len);
}


ResultMessage handle_set_key(uint16_t id, ParseState *state) {
  // Associated data: [encryption_u8 conn_id_u16 io_id_u16 nonce_u16]
  uint8_t* ad;
  const unsigned int AD_LEN = 7;
  if (!parse_raw_data(state, AD_LEN, &ad))
      return RESULT(ResultCode_IllegalPayload);

  uint8_t* cipher;
  if (!parse_raw_data(state, SECURITY_BYTES, &cipher))
      return RESULT(ResultCode_IllegalPayload);

  uint8_t* tag;
  if (!parse_raw_data(state, SECURITY_BYTES, &tag))
      return RESULT(ResultCode_IllegalPayload);

  DEBUG("Calling set_key of sm %d", id);
  return set_key(id, ad, AD_LEN, cipher, tag);
}


ResultMessage handle_attest(uint16_t id, ParseState *state) {
  uint16_t challenge_len;
  if (!parse_int(state, &challenge_len))
      return RESULT(ResultCode_IllegalPayload);

  uint8_t* challenge;
  if (!parse_raw_data(state, challenge_len, &challenge))
      return RESULT(ResultCode_IllegalPayload);

  DEBUG("Calling attest of sm %d", id);
  return attest(id, challenge, challenge_len);
}

ResultMessage handle_disable(uint16_t id, ParseState *state) {
  uint8_t* ad;
  const unsigned int AD_LEN = 2;
  const unsigned int CIPHER_LEN = 2;

  if (!parse_raw_data(state, AD_LEN, &ad))
      return RESULT(ResultCode_IllegalPayload);

  uint8_t* cipher;
  if (!parse_raw_data(state, CIPHER_LEN, &cipher))
      return RESULT(ResultCode_IllegalPayload);

  uint8_t* tag;
  if (!parse_raw_data(state, SECURITY_BYTES, &tag))
      return RESULT(ResultCode_IllegalPayload);

  DEBUG("Calling disable of sm %d", id);
  return disable(id, ad, AD_LEN, cipher, CIPHER_LEN, tag);
}

ResultMessage handle_user_entrypoint(uint16_t id, uint16_t index, ParseState *state) {
  uint8_t* payload;
  unsigned int payload_len;
  if(!parse_all_raw_data(state, &payload, &payload_len)) {
    return RESULT(ResultCode_IllegalPayload);
  }

  DEBUG("Calling entry point %d of sm %d", index, id);
  return call(id, index, payload, payload_len);
}
