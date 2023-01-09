#ifndef __TA_H__
#define __TA_H__

#include "networking.h"
#include "command_handlers.h"

// platform-specific library and definitions
#include "tee_client_api.h"

#define SECURITY_BYTES 16
#define LOAD_HEADER_LEN 18
#define OUTPUT_DATA_MAX_SIZE 1024 * 1024 // total size (for all concurrent outputs) in bytes
#define MAX_CONCURRENT_OUTPUTS 32

// data structure containing all the required fields
typedef struct {
    TEEC_UUID uuid;
    TEEC_Context ctx;
    TEEC_Session sess;
    TEEC_Operation op;
    uint16_t module_id;
} ModuleContext;

/* Functions for managing internal linked list */

// Generic functions
int add_module(ModuleContext *ta_ctx);
ModuleContext *get_module_from_id(uint16_t id);

/* API for interfacing with modules */

// Platform-specific functions
TEEC_Result call_entry(ModuleContext *ctx, Entrypoint entry_id);
void send_outputs(unsigned int num_outputs, unsigned char *conn_ids, unsigned char *payloads);

// Generic functions
int initialize_context(ModuleContext *ctx, unsigned char* buf, unsigned int size);
ResultMessage load_module(unsigned char* buf, unsigned int size);
void handle_input(uint16_t sm, uint16_t conn_id, unsigned char* data, unsigned int len);
ResultMessage set_key(uint16_t sm, unsigned char* ad, unsigned int ad_len, unsigned char* cipher, unsigned char* tag);
ResultMessage attest(uint16_t sm, unsigned char* challenge, unsigned int challenge_len);
ResultMessage disable(uint16_t sm, unsigned char* ad, unsigned int ad_len, unsigned char* cipher, unsigned int cipher_len, unsigned char* tag);
ResultMessage call(uint16_t sm, uint16_t entry_id, unsigned char* payload, unsigned int len);


#endif