#ifndef __ENCLAVE_UTILS_H__
#define __ENCLAVE_UTILS_H__

#include <stdint.h>

#include "networking.h"
#include "utils.h"

ResultMessage load_enclave(CommandMessage m);

void reactive_handle_output(uint16_t conn_id, void* data, unsigned int len);
void reactive_handle_input(uint16_t sm, uint16_t conn_id, void* data, unsigned int len);

ResultMessage handle_set_key(uint16_t id, ParseState *state);
ResultMessage handle_attest(uint16_t id, ParseState *state);
ResultMessage handle_disable(uint16_t id, ParseState *state);
ResultMessage handle_user_entrypoint(uint16_t id, uint16_t index, ParseState *state);

#endif
