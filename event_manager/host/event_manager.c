#include "event_manager.h"

#include <stdio.h>

#include "logging.h"
#include "command_handlers.h"

ResultMessage process_message(CommandMessage m) {
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

    case CommandCode_Ping:
      INFO("Received ping");
      return handler_ping(m);

    case CommandCode_RegisterEntrypoint:
      INFO("Received entrypoint registration");
      return handler_register_entrypoint(m);

    default: // CommandCode_Invalid
      WARNING("wrong cmd id");
      return NULL;
  }
}
