#include "module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "enclave_utils.h"

TEEC_Result call_entry(ModuleContext *ctx, Entrypoint entry_id) {
    TEEC_Session temp_sess;
    TEEC_Context temp_ctx;
    temp_ctx.fd = ctx->ctx.fd;
    temp_ctx.reg_mem = ctx->ctx.reg_mem;
    temp_ctx.memref_null = ctx->ctx.memref_null;

    temp_sess.session_id = ctx->sess.session_id;
    temp_sess.ctx = &temp_ctx;

    uint32_t err_origin;
    return TEEC_InvokeCommand(
        &temp_sess,
        entry_id,
        &ctx->op,
        &err_origin
    );
}

void send_outputs(
    unsigned int num_outputs,
    unsigned char *conn_ids,
    unsigned char *payloads
) {
    // check if there are any outputs to send out, calling handle_output
    // for each of them

    int pl_offset = 0;
    for(int i = 0; i < num_outputs; i++) {
        uint16_t conn_id = *((uint16_t *) (conn_ids + 2 * i));
        uint32_t cipher_len = *((uint32_t *) (payloads + pl_offset));

        // call handle_output
        reactive_handle_output(conn_id, payloads + pl_offset + 4, cipher_len + SECURITY_BYTES);

        pl_offset += 4 + cipher_len + SECURITY_BYTES;
    }
}

int initialize_context(ModuleContext *ctx, unsigned char* buf, unsigned int size) {
  TEEC_UUID uuid;

  if(size < LOAD_HEADER_LEN) {
    return 0;
  }
  
  int j = 0;
  uint16_t id = 0;
  for(int m = 1; m >= 0; --m){
    id = id + (( buf[m] & 0xFF ) << (8*j));
    ++j;
  }
  ctx->module_id = id;

  j = 0;
  int timelow = 0;
  for(int m = 5; m >= 2; --m){
    timelow = timelow + (( buf[m] & 0xFF ) << (8*j));
    ++j;
  }	
  uuid.timeLow = timelow;

  j = 0;
  int mid = 0;
  for(int m = 7; m >= 6; --m){
    mid = mid + (( buf[m] & 0xFF ) << (8*j));
    ++j;
  }	
  uuid.timeMid = mid;

  j = 0;
  int high = 0;
  for(int m = 9; m >= 8; --m){
    high = high + (( buf[m] & 0xFF ) << (8*j));
    ++j;
  }	
  uuid.timeHiAndVersion = high;

  for(int m = 10; m < 18; m++){
    uuid.clockSeqAndNode[m-10] = buf[m];
  }

  ctx->uuid =  uuid;
  return 1;
}

ResultMessage load_module(unsigned char* buf, unsigned int size) {

    ModuleContext ctx;
    TEEC_Result rc;
    uint32_t err_origin;

    if(!initialize_context(&ctx, buf, size)) {
        ERROR("initialize_context: buffer too small %u/18", size);
        return RESULT(ResultCode_IllegalPayload);
    }

    // here 100 is an overapproximation, since snprintf will write much less bytes
    char fname[100] = { 0 };
    FILE *file = NULL;
    char path[] = "/lib/optee_armtz";

    snprintf(
        fname,
        100,
        "%s/%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x.ta",
        path,
        ctx.uuid.timeLow,
        ctx.uuid.timeMid,
        ctx.uuid.timeHiAndVersion,
        ctx.uuid.clockSeqAndNode[0],
        ctx.uuid.clockSeqAndNode[1],
        ctx.uuid.clockSeqAndNode[2],
        ctx.uuid.clockSeqAndNode[3],
        ctx.uuid.clockSeqAndNode[4],
        ctx.uuid.clockSeqAndNode[5],
        ctx.uuid.clockSeqAndNode[6],
        ctx.uuid.clockSeqAndNode[7]
    );

    file = fopen(fname, "w"); 

    if(file == NULL) {
        ERROR("Cannot open file: %s", fname);
        return RESULT(ResultCode_InternalError);
    }

    fwrite(buf + LOAD_HEADER_LEN, 1, size - LOAD_HEADER_LEN, file);
    fclose(file); 

    /* Initialize a context connecting us to the TEE */
    rc = TEEC_InitializeContext(NULL, &ctx.ctx);
    if(rc != TEEC_SUCCESS) {
        ERROR("TEEC_InitializeContext failed: %d", rc);
        return RESULT(ResultCode_InternalError);
    }

    // open a session to the TA
    rc = TEEC_OpenSession(
        &ctx.ctx,
        &ctx.sess,
        &ctx.uuid,
        TEEC_LOGIN_PUBLIC,
        NULL,
        NULL,
        &err_origin
    );

    if(rc != TEEC_SUCCESS) {
        ERROR("TEEC_OpenSession failed: %d", rc);
        return RESULT(ResultCode_InternalError);
    }

    if(!add_module(&ctx)) {
        //TODO close session with TA?!
        ERROR("Could not add module to internal list");
        return RESULT(ResultCode_InternalError);
    }

    return RESULT(ResultCode_Ok);
}

void handle_input(uint16_t sm, uint16_t conn_id, unsigned char* data, unsigned int len) {
    // check if buffer has enough size
    if(len < SECURITY_BYTES) {
        ERROR("Payload too small: %u", len);
        return;
    }

    if(len > OUTPUT_DATA_MAX_SIZE + SECURITY_BYTES * MAX_CONCURRENT_OUTPUTS) {
        ERROR("Payload too big: %u", len);
        return;
    }

    unsigned int cipher_size = len - SECURITY_BYTES;
    ModuleContext *ctx = get_module_from_id(sm);

    if(ctx == NULL) {
        ERROR("Module %d not found", sm);
        return;
    }

    unsigned char *arg1_buf = malloc(2 * MAX_CONCURRENT_OUTPUTS);
    unsigned char *arg2_buf = malloc(
        OUTPUT_DATA_MAX_SIZE + 
        SECURITY_BYTES * MAX_CONCURRENT_OUTPUTS
    );
    unsigned char *arg3_buf = malloc(SECURITY_BYTES);

    if(arg1_buf == NULL || arg2_buf == NULL || arg3_buf == NULL) {
        ERROR("Failed to allocate buffers");
        if(arg1_buf != NULL) free(arg1_buf);
        if(arg2_buf != NULL) free(arg2_buf);
        if(arg3_buf != NULL) free(arg3_buf);
        return;
    }

    // initialize buffers
    memcpy(arg2_buf, data, cipher_size);
    memcpy(arg3_buf, data + cipher_size, SECURITY_BYTES);
    memset(&ctx->op, 0, sizeof(ctx->op));

    // prepare parameters
    ctx->op.params[0].value.a = cipher_size;
    ctx->op.params[0].value.b = conn_id;
    ctx->op.params[1].tmpref.buffer = (void *) arg1_buf;
    ctx->op.params[1].tmpref.size = 2 * MAX_CONCURRENT_OUTPUTS;
    ctx->op.params[2].tmpref.buffer = (void *) arg2_buf;
    ctx->op.params[2].tmpref.size = OUTPUT_DATA_MAX_SIZE + SECURITY_BYTES * MAX_CONCURRENT_OUTPUTS;
    ctx->op.params[3].tmpref.buffer = (void *) arg3_buf;
    ctx->op.params[3].tmpref.size = SECURITY_BYTES;
    ctx->op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_VALUE_INOUT,
        TEEC_MEMREF_TEMP_OUTPUT,
        TEEC_MEMREF_TEMP_INOUT,
        TEEC_MEMREF_TEMP_INPUT
    );

    // call entry point
    TEEC_Result rc = call_entry(ctx, Entrypoint_HandleInput);

    if (rc == TEEC_SUCCESS) {
        send_outputs(ctx->op.params[0].value.b, arg1_buf, arg2_buf);
    } else {
        ERROR("TEEC_InvokeCommand failed: %d", rc);
    }

    // free buffers
    free(arg1_buf);
    free(arg2_buf);
    free(arg3_buf);
}

ResultMessage set_key(
    uint16_t sm,
    unsigned char* ad,
    unsigned int ad_len,
    unsigned char* cipher,
    unsigned char* tag
) {
    ModuleContext *ctx = get_module_from_id(sm);

    if(ctx == NULL) {
        ERROR("Module %d not found", sm);
        return RESULT(ResultCode_BadRequest);
    }

    unsigned char *arg0_buf = malloc(ad_len);
    unsigned char *arg1_buf = malloc(SECURITY_BYTES);
    unsigned char *arg2_buf = malloc(SECURITY_BYTES);

    if(arg0_buf == NULL || arg1_buf == NULL || arg2_buf == NULL) {
        ERROR("Failed to allocate buffers");
        if(arg0_buf != NULL) free(arg0_buf);
        if(arg1_buf != NULL) free(arg1_buf);
        if(arg2_buf != NULL) free(arg2_buf);
        return RESULT(ResultCode_InternalError);
    }

    // initialize buffers
    memcpy(arg0_buf, ad, ad_len);
    memcpy(arg1_buf, cipher, SECURITY_BYTES);
    memcpy(arg2_buf, tag, SECURITY_BYTES);
    memset(&ctx->op, 0, sizeof(ctx->op));

    // prepare parameters
    ctx->op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_MEMREF_TEMP_INPUT,
        TEEC_MEMREF_TEMP_INPUT,
        TEEC_MEMREF_TEMP_INPUT,
        TEEC_NONE
    );
    ctx->op.params[0].tmpref.buffer = arg0_buf;
    ctx->op.params[0].tmpref.size = ad_len;
    ctx->op.params[1].tmpref.buffer = arg1_buf;
    ctx->op.params[1].tmpref.size = SECURITY_BYTES;
    ctx->op.params[2].tmpref.buffer = arg2_buf;
    ctx->op.params[2].tmpref.size = SECURITY_BYTES;

    // call entry point
    TEEC_Result rc = call_entry(ctx, Entrypoint_SetKey);

    // free buffers
    free(arg0_buf);
    free(arg1_buf);
    free(arg2_buf);

    if(rc != TEEC_SUCCESS) {
        ERROR("TEEC_InvokeCommand failed: %d", rc);
        return RESULT(ResultCode_InternalError);
    }

    return RESULT(ResultCode_Ok);
}

ResultMessage attest(
    uint16_t sm,
    unsigned char* challenge,
    unsigned int challenge_len
) {
    ModuleContext *ctx = get_module_from_id(sm);

    if(ctx == NULL) {
        ERROR("Module %d not found", sm);
        return RESULT(ResultCode_BadRequest);
    }

    unsigned char *arg0_buf = malloc(challenge_len);
    unsigned char *arg1_buf = malloc(SECURITY_BYTES);

    if(arg0_buf == NULL || arg1_buf == NULL) {
        ERROR("Failed to allocate buffers");
        if(arg0_buf != NULL) free(arg0_buf);
        if(arg1_buf != NULL) free(arg1_buf);
        return RESULT(ResultCode_InternalError);
    }

    // initialize buffers
    memcpy(arg0_buf, challenge, challenge_len);
    memset(&ctx->op, 0, sizeof(ctx->op));

    // prepare parameters
    ctx->op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_MEMREF_TEMP_INPUT,
        TEEC_MEMREF_TEMP_OUTPUT,
        TEEC_NONE,
        TEEC_NONE
    );
    ctx->op.params[0].tmpref.buffer = arg0_buf;
    ctx->op.params[0].tmpref.size = challenge_len;
    ctx->op.params[1].tmpref.buffer = arg1_buf;
    ctx->op.params[1].tmpref.size = SECURITY_BYTES;

    // call entry point
    TEEC_Result rc = call_entry(ctx, Entrypoint_Attest);

    // free buffers
    free(arg0_buf);

    // fetch result
    if(rc == TEEC_SUCCESS) {
        return RESULT_DATA(ResultCode_Ok, SECURITY_BYTES, arg1_buf);
    } else {
        ERROR("TEEC_InvokeCommand failed: %d", rc);
        free(arg1_buf);
        return RESULT(ResultCode_InternalError);
    }
}

ResultMessage disable(
    uint16_t sm,
    unsigned char* ad,
    unsigned int ad_len,
    unsigned char* cipher,
    unsigned int cipher_len,
    unsigned char* tag
) {
    ModuleContext *ctx = get_module_from_id(sm);

    if(ctx == NULL) {
        ERROR("Module %d not found", sm);
        return RESULT(ResultCode_BadRequest);
    }

    unsigned char *arg0_buf = malloc(ad_len);
    unsigned char *arg1_buf = malloc(cipher_len);
    unsigned char *arg2_buf = malloc(SECURITY_BYTES);

    if(arg0_buf == NULL || arg1_buf == NULL || arg2_buf == NULL) {
        ERROR("Failed to allocate buffers");
        if(arg0_buf != NULL) free(arg0_buf);
        if(arg1_buf != NULL) free(arg1_buf);
        if(arg2_buf != NULL) free(arg2_buf);
        return RESULT(ResultCode_InternalError);
    }

    // initialize buffers
    memcpy(arg0_buf, ad, ad_len);
    memcpy(arg1_buf, cipher, cipher_len);
    memcpy(arg2_buf, tag, SECURITY_BYTES);
    memset(&ctx->op, 0, sizeof(ctx->op));

    // prepare parameters
	ctx->op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_MEMREF_TEMP_INPUT,
		TEEC_MEMREF_TEMP_INPUT,
		TEEC_MEMREF_TEMP_INPUT,
        TEEC_NONE
    );
	ctx->op.params[0].tmpref.buffer = arg0_buf;
	ctx->op.params[0].tmpref.size = ad_len;
	ctx->op.params[1].tmpref.buffer = arg1_buf;
	ctx->op.params[1].tmpref.size = cipher_len;
	ctx->op.params[2].tmpref.buffer = arg2_buf;
	ctx->op.params[2].tmpref.size = SECURITY_BYTES;

    // call entry point
    TEEC_Result rc = call_entry(ctx, Entrypoint_Disable);

    // free buffers
    free(arg0_buf);
    free(arg1_buf);
    free(arg2_buf);

    if(rc != TEEC_SUCCESS) {
        ERROR("TEEC_InvokeCommand failed: %d", rc);
        return RESULT(ResultCode_InternalError);
    }

    return RESULT(ResultCode_Ok);
}

ResultMessage call(
    uint16_t sm,
    uint16_t entry_id,
    unsigned char* payload,
    unsigned int len
) {
    ModuleContext *ctx = get_module_from_id(sm);

    if(len > OUTPUT_DATA_MAX_SIZE + SECURITY_BYTES * MAX_CONCURRENT_OUTPUTS) {
        ERROR("Payload too big: %u", len);
        return RESULT(ResultCode_BadRequest);
    }

    if(ctx == NULL) {
        ERROR("Module %d not found", sm);
        return RESULT(ResultCode_BadRequest);
    }

    unsigned char *arg1_buf = malloc(2 * MAX_CONCURRENT_OUTPUTS);
    unsigned char *arg2_buf = malloc(
        OUTPUT_DATA_MAX_SIZE + 
        SECURITY_BYTES * MAX_CONCURRENT_OUTPUTS
    );

    if(arg1_buf == NULL || arg2_buf == NULL) {
        ERROR("Failed to allocate buffers");
        if(arg1_buf != NULL) free(arg1_buf);
        if(arg2_buf != NULL) free(arg2_buf);
        return RESULT(ResultCode_InternalError);
    }

    // initialize buffers
    memcpy(arg2_buf, payload, len);
    memset(&ctx->op, 0, sizeof(ctx->op));

    // prepare parameters
    ctx->op.params[0].value.b = entry_id;
    ctx->op.params[0].value.a = len;
    ctx->op.params[1].tmpref.buffer = (void *) arg1_buf;
    ctx->op.params[1].tmpref.size = 2 * MAX_CONCURRENT_OUTPUTS;
    ctx->op.params[2].tmpref.buffer = (void *) arg2_buf;
    ctx->op.params[2].tmpref.size = OUTPUT_DATA_MAX_SIZE + SECURITY_BYTES * MAX_CONCURRENT_OUTPUTS;
    ctx->op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_VALUE_INOUT,
        TEEC_MEMREF_TEMP_OUTPUT,
        TEEC_MEMREF_TEMP_INOUT,
        TEEC_NONE
    );

    // call entry point
    TEEC_Result rc = call_entry(ctx, Entrypoint_User);

    ResultMessage res;
    if (rc == TEEC_SUCCESS) {
        send_outputs(ctx->op.params[0].value.b, arg1_buf, arg2_buf);
        res = RESULT(ResultCode_Ok); //TODO where is the response!?!?!
    } else {
        ERROR("TEEC_InvokeCommand failed: %d", rc);
        res = RESULT(ResultCode_InternalError);
    }

    // free buffers
    free(arg1_buf);
    free(arg2_buf);

    return res;
}