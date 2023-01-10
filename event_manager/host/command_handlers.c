#include "command_handlers.h"

#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>

#include "connection.h"
#include "utils.h"
#include "logging.h"
#include "enclave_utils.h"
#include "module.h"

ResultMessage handler_add_connection(CommandMessage m) {
  Connection connection;
  struct in_addr* parsed_addr;

  ParseState *state = create_parse_state(m->message->payload, m->message->size);

   // The payload format is [conn_id to_sm port address]
  // which is basically this same thing as a Connection.

  if (!parse_int(state, &connection.conn_id))
     return RESULT(ResultCode_IllegalPayload);
  if (!parse_int(state, &connection.to_sm))
     return RESULT(ResultCode_IllegalPayload);
 if (!parse_byte(state, &connection.local))
    return RESULT(ResultCode_IllegalPayload);
  if (!parse_int(state, &connection.to_port))
    return RESULT(ResultCode_IllegalPayload);

  if (!parse_raw_data(state, sizeof(struct in_addr), (uint8_t**)&parsed_addr))
    return RESULT(ResultCode_IllegalPayload);
  connection.to_address = *parsed_addr;

  DEBUG(
    "conn_id: %d, to_sm: %d, local: %d, to_port: %d, to_ip: %s",
    connection.conn_id,
    connection.to_sm,
    connection.local,
    connection.to_port,
    inet_ntoa(connection.to_address)
  );

  free_parse_state(state);

  if (!connections_replace(&connection) && !connections_add(&connection))
     return RESULT(ResultCode_InternalError);

  return RESULT(ResultCode_Ok);
}

ResultMessage handler_call_entrypoint(CommandMessage m) {
  ParseState *state = create_parse_state(m->message->payload, m->message->size);

  // The payload format is [sm_id, index, args]
  uint16_t id;
  if (!parse_int(state, &id))
     return RESULT(ResultCode_IllegalPayload);

  uint16_t index;
  if (!parse_int(state, &index))
     return RESULT(ResultCode_IllegalPayload);

  //DEBUG("id: %d, index: %d", id, index);

  measure_time("call_entrypoint_before_dispatch");
  ResultMessage res;

  switch(index) {
    case Entrypoint_SetKey:
      res = handle_set_key(id, state);
      break;
    case Entrypoint_Attest:
      res = handle_attest(id, state);
      break;
    case Entrypoint_Disable:
      res = handle_disable(id, state);
      break;
    default:
      res = handle_user_entrypoint(id, index, state);
  }

  measure_time("call_entrypoint_after_dispatch");
  free_parse_state(state);
  return res;
}

ResultMessage handler_remote_output(CommandMessage m) {
  ParseState *state = create_parse_state(m->message->payload, m->message->size);

  // The packet format is [sm_id conn_id data]
  uint16_t sm;
  if (!parse_int(state, &sm))
      return RESULT(ResultCode_IllegalPayload);

  uint16_t conn_id;
  if (!parse_int(state, &conn_id))
      return RESULT(ResultCode_IllegalPayload);

  uint8_t* payload;
  unsigned int payload_len;
  if (!parse_all_raw_data(state, &payload, &payload_len))
      return RESULT(ResultCode_IllegalPayload);

  DEBUG("id: %d, conn_id: %d, payload_size: %u", sm, conn_id, payload_len);

  measure_time("remote_output_before_dispatch");
  reactive_handle_input(sm, conn_id, payload, payload_len);
  measure_time("remote_output_after_dispatch");

  free_parse_state(state);
  return RESULT(ResultCode_Ok);
}

ResultMessage handler_load_sm(CommandMessage m) {
  return load_enclave(m);
}

ResultMessage handler_reset(CommandMessage m) {
  //clear all modules
  delete_modules();
  //clear all connections
  delete_connections();

  return RESULT(ResultCode_Ok);
}


ResultMessage handler_register_entrypoint(CommandMessage m) {
  WARNING("Periodic events disabled");
  return RESULT(ResultCode_InternalError);
}