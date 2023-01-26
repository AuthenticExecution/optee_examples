#include "event_manager.h"

#include <stdio.h>
#include <stdlib.h>

#include "logging.h"
#include "command_handlers.h"

ResultMessage _process_message(CommandMessage m);

ResultMessage process_message(CommandMessage m) {
  if(m->code >= CommandCode_Invalid) {
      INFO("ECHO");
      ResultMessage res = RESULT_DATA(ResultCode_Ok, m->message->size, m->message->payload);
      free(m);
      return res;
  } else {
    ResultMessage res = _process_message(m);
    destroy_command_message(m);
    return res;
  }
}

ResultMessage _process_message(CommandMessage m) {
  switch (m->code) {
    case CommandCode_AddConnection:
      INFO("Received add connection");
      return handler_add_connection(m);

    case CommandCode_CallEntrypoint:
      INFO("Received entrypoint call");
      return handler_call_entrypoint(m);

    case CommandCode_RemoteOutput:
      INFO("Received remote output");
      return handler_remote_output(m);

    case CommandCode_LoadSM:
      INFO("Received new module");
      return handler_load_sm(m);

    case CommandCode_Reset:
      INFO("Received reset");
      return handler_reset(m);

    case CommandCode_RegisterEntrypoint:
      INFO("Received entrypoint registration");
      return handler_register_entrypoint(m);

    default: // CommandCode_Invalid
      ERROR("Fatal: should not be here!");
      return RESULT(ResultCode_InternalError);
  }
}
