#ifndef __COMMAND_HANDLERS_H__
#define __COMMAND_HANDLERS_H__

#include "networking.h"

typedef enum
{
    Entrypoint_SetKey                = 0x0,
    Entrypoint_Attest                = 0x1,
    Entrypoint_Disable               = 0x2,
    Entrypoint_HandleInput           = 0x3,
    Entrypoint_User                  = 0x4
} Entrypoint;

ResultMessage handler_add_connection(CommandMessage m);
ResultMessage handler_call_entrypoint(CommandMessage m);
ResultMessage handler_remote_output(CommandMessage m);
ResultMessage handler_load_sm(CommandMessage m);
ResultMessage handler_reset(CommandMessage m);
ResultMessage handler_register_entrypoint(CommandMessage m);

#endif
