#include "enclave_utils.h"

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <err.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>

#include <sys/types.h>  
#include <sys/socket.h>  
#include <netinet/in.h> 

/* OP-TEE TEE client API (built by optee_client) */
#include "tee_client_api.h"

#include "addr.h"
#include "networking.h"
#include "command_handlers.h"
#include "utils.h"
#include "connection.h"
#include "uuid.h"

uint16_t PORT = 1236;

typedef struct
{
  TEEC_UUID uuid;
	TEEC_Context ctx;
	TEEC_Session sess;
  TEEC_Operation op;
} TA_CTX;

typedef struct CTX_Node
{
    TA_CTX ta_ctx;
    struct CTX_Node* next;
} CTX_Node;

static CTX_Node* ta_ctx_head = NULL;

int ta_ctx_add(TA_CTX* ta_ctx)
{
    CTX_Node* node = malloc_aligned(sizeof(CTX_Node));

    if (node == NULL)
        return 0;

    node->ta_ctx = *ta_ctx;
    node->next = ta_ctx_head;
    ta_ctx_head = node;
    return 1;
}

TA_CTX* ta_ctx_get(TEEC_UUID uuid)
{
    CTX_Node* current = ta_ctx_head;

    while (current != NULL) {
        TA_CTX* ctx = &current->ta_ctx;

        if ((ctx->uuid.timeLow == uuid.timeLow) &&
            (ctx->uuid.timeMid == uuid.timeMid) &&
            (ctx->uuid.timeHiAndVersion == uuid.timeHiAndVersion) &&
            !memcmp(ctx->uuid.clockSeqAndNode, uuid.clockSeqAndNode, 8)) {

            return ctx;
        }

        current = current->next;
    }

    return NULL;
}
//---------------------------------------------------------------------------------------
TEEC_UUID calculate_uuid (unsigned char* buf){

  UUID uuid_struct;
  TEEC_UUID uuid;

  int j = 0;
  uint16_t id = 0;
  for(int m = 1; m >= 0; --m){
    id = id + (( buf[m] & 0xFF ) << (8*j));
    ++j;
  }
  uuid_struct.module_id = id;

  j = 0;
  int timelow = 0;
  for(int m = 5; m >= 2; --m){
    timelow = timelow + (( buf[m] & 0xFF ) << (8*j));
    ++j;
  }	
  uuid.timeLow = timelow;

//-----------------------------------------------

  j = 0;
  int mid = 0;
  for(int m = 7; m >= 6; --m){
    mid = mid + (( buf[m] & 0xFF ) << (8*j));
    ++j;
  }	
  uuid.timeMid = mid;
//-------------------------------------------------------------

  j = 0;
  int high = 0;
  for(int m = 9; m >= 8; --m){
    high = high + (( buf[m] & 0xFF ) << (8*j));
    ++j;
  }	
  uuid.timeHiAndVersion = high;
  ///------------------------------------------------------------------------------

  for(int m = 10; m < 18; m++){
    uuid.clockSeqAndNode[m-10] = buf[m];
  }
  uuid_struct.uuid =  uuid;
  uuid_add(&uuid_struct);
  return uuid;
}

ResultMessage load_enclave(unsigned char* buf, uint32_t size) {

  TA_CTX ctx;
  TEEC_Result rc;
  uint32_t err_origin;

  ctx.uuid = calculate_uuid(buf);

  char fname[255] = { 0 };
	FILE *file = NULL;
  char path[] = "/lib/optee_armtz";

  snprintf(fname, PATH_MAX,
		     "%s/%08x-%04x-%04x-%02x%02x%s%02x%02x%02x%02x%02x%02x.ta",
         path,
		     ctx.uuid.timeLow,
		     ctx.uuid.timeMid,
		     ctx.uuid.timeHiAndVersion,
		     ctx.uuid.clockSeqAndNode[0],
		     ctx.uuid.clockSeqAndNode[1],
		     "-",
		     ctx.uuid.clockSeqAndNode[2],
		     ctx.uuid.clockSeqAndNode[3],
		     ctx.uuid.clockSeqAndNode[4],
		     ctx.uuid.clockSeqAndNode[5],
		     ctx.uuid.clockSeqAndNode[6],
		     ctx.uuid.clockSeqAndNode[7]);
  
  file = fopen(fname, "w"); 
  
  fwrite(buf + 18 ,1, size - 18 , file);
  fclose(file); 

/* Initialize a context connecting us to the TEE */
  rc = TEEC_InitializeContext(NULL, &ctx.ctx);
  if(rc != TEEC_SUCCESS) {
    return RESULT(ResultCode_InternalError);
  }

// open a session to the TA
  rc = TEEC_OpenSession(&ctx.ctx, &ctx.sess, &ctx.uuid, TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
  if(rc != TEEC_SUCCESS) {
    return RESULT(ResultCode_InternalError);
  }

//-----------------------------^^^^^^^^^&&&&&&&&^^^^^^^^^^----------
  ta_ctx_add(&ctx);

//-----------------------------------------------------------------
// everything went good
  return RESULT(ResultCode_Ok);
}

ResultMessage handle_set_key(unsigned char* buf, uint16_t module_id) {

  UUID* uuid_struct = uuid_get(module_id);
  TEEC_Result rc;
  uint32_t err_origin;
  unsigned char* ad;
  unsigned char* cipher;
  unsigned char* tag;
//----------------------------------------------------------------------------------
  
  ad = malloc(7);
  memcpy(ad, buf+4, 7);
  cipher = malloc(16);
  memcpy(cipher, buf+11, 16);
  tag = malloc(16);
  memcpy(tag, buf+27, 16);

//-----------------------------^^^^^^^^^&&&&&&&&^^^^^^^^^^------------------------
  TA_CTX* ta_ctx = ta_ctx_get(uuid_struct->uuid);
//-----------------------------------------------------------------
  memset(&ta_ctx->op, 0, sizeof(ta_ctx->op));
	ta_ctx->op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
					 TEEC_MEMREF_TEMP_INPUT,
					 TEEC_MEMREF_TEMP_INPUT, TEEC_NONE);
	ta_ctx->op.params[0].tmpref.buffer = ad;
	ta_ctx->op.params[0].tmpref.size = 7;
	ta_ctx->op.params[1].tmpref.buffer = cipher;
	ta_ctx->op.params[1].tmpref.size = 16;
	ta_ctx->op.params[2].tmpref.buffer = tag;
	ta_ctx->op.params[2].tmpref.size = 16;

  TEEC_Session temp_sess;
  TEEC_Context temp_ctx;
  temp_ctx.fd = ta_ctx->ctx.fd;
  temp_ctx.reg_mem = ta_ctx->ctx.reg_mem;
  temp_ctx.memref_null = ta_ctx->ctx.memref_null;

  temp_sess.session_id = ta_ctx->sess.session_id;
  temp_sess.ctx = &temp_ctx;

  rc = TEEC_InvokeCommand(&temp_sess, Entrypoint_SetKey, &ta_ctx->op, &err_origin);

  free(ad);
  free(cipher);
  free(tag);

  if(rc != TEEC_SUCCESS) {
    return RESULT(ResultCode_InternalError);
  }

  return RESULT(ResultCode_Ok);
}

ResultMessage handle_attest(unsigned char* buf, uint16_t module_id) {

  UUID* uuid_struct = uuid_get(module_id);
  TEEC_Result rc;
  uint32_t err_origin;
  unsigned char* challenge;
  unsigned char* challenge_mac;
//----------------------------------------------------------------------------------
  
  challenge = malloc(16);
  memcpy(challenge, buf+6, 16);
  challenge_mac = malloc(16);

//-----------------------------^^^^^^^^^&&&&&&&&^^^^^^^^^^-------------------------
  TA_CTX* ta_ctx = ta_ctx_get(uuid_struct->uuid);
//-----------------------------------------------------------------
  memset(&ta_ctx->op, 0, sizeof(ta_ctx->op));
	ta_ctx->op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
					 TEEC_MEMREF_TEMP_OUTPUT,
					 TEEC_NONE, TEEC_NONE);
	ta_ctx->op.params[0].tmpref.buffer = challenge;
	ta_ctx->op.params[0].tmpref.size = 16;
	ta_ctx->op.params[1].tmpref.buffer = challenge_mac;
	ta_ctx->op.params[1].tmpref.size = 16;

  TEEC_Session temp_sess;
  TEEC_Context temp_ctx;
  temp_ctx.fd = ta_ctx->ctx.fd;
  temp_ctx.reg_mem = ta_ctx->ctx.reg_mem;
  temp_ctx.memref_null = ta_ctx->ctx.memref_null;
  
  temp_sess.session_id = ta_ctx->sess.session_id;
  temp_sess.ctx = &temp_ctx;

  rc = TEEC_InvokeCommand(&temp_sess, Entrypoint_Attest, &ta_ctx->op, &err_origin);
  free(challenge);

  if(rc != TEEC_SUCCESS) {
    return RESULT(ResultCode_InternalError);
  }
 
  return RESULT_DATA(ResultCode_Ok, 16, challenge_mac);
}

ResultMessage handle_disable(unsigned char* buf, uint16_t module_id) {

  UUID* uuid_struct = uuid_get(module_id);
  TEEC_Result rc;
  uint32_t err_origin;
  unsigned char* nonce;
  unsigned char* cipher;
  unsigned char* mac;

  
//----------------------------------------------------------------------------------
  
  //nonce in buf+4, cipher in buf+6, mac in  buf+8
  nonce = malloc(2);
  memcpy(nonce, buf+4, 2);
  cipher = malloc(2);
  memcpy(cipher, buf+6, 2);
  mac = malloc(16);
  memcpy(mac, buf+8, 16);

//-----------------------------^^^^^^^^^&&&&&&&&^^^^^^^^^^-------------------------
  TA_CTX* ta_ctx = ta_ctx_get(uuid_struct->uuid);
//-----------------------------------------------------------------
  memset(&ta_ctx->op, 0, sizeof(ta_ctx->op));
	ta_ctx->op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
					 TEEC_MEMREF_TEMP_INPUT,
					 TEEC_MEMREF_TEMP_INPUT, TEEC_NONE);
	ta_ctx->op.params[0].tmpref.buffer = nonce;
	ta_ctx->op.params[0].tmpref.size = 2;
	ta_ctx->op.params[1].tmpref.buffer = cipher;
	ta_ctx->op.params[1].tmpref.size = 2;
	ta_ctx->op.params[2].tmpref.buffer = mac;
	ta_ctx->op.params[2].tmpref.size = 16;

  TEEC_Session temp_sess;
  TEEC_Context temp_ctx;
  temp_ctx.fd = ta_ctx->ctx.fd;
  temp_ctx.reg_mem = ta_ctx->ctx.reg_mem;
  temp_ctx.memref_null = ta_ctx->ctx.memref_null;
  
  temp_sess.session_id = ta_ctx->sess.session_id;
  temp_sess.ctx = &temp_ctx;

  rc = TEEC_InvokeCommand(&temp_sess, Entrypoint_Disable, &ta_ctx->op, &err_origin);

  free(nonce);
  free(cipher);
  free(mac);

  if(rc != TEEC_SUCCESS) {
    return RESULT(ResultCode_InternalError);
  }
 
  return RESULT(ResultCode_Ok);
}

ResultMessage handle_user_entrypoint(unsigned char* buf, uint32_t size, uint16_t module_id) {

  UUID* uuid_struct = uuid_get(module_id);
  TEEC_Result rc;
  uint32_t err_origin;

  //----------------------------------------------------------------------------------
  int j = 0;
  uint32_t index = 0;
  for(int m = 3; m >= 2; --m){
    index = index + (( buf[m] & 0xFF ) << (8*j));
    ++j;
  }
  //-----------------^^^^^^^^^&&&&&&&&^^^^^^^^^^---------------------
  TA_CTX* ctx1 = ta_ctx_get(uuid_struct->uuid);
  //-----------------------------------------------------------------
  unsigned char *conn_id_buf;
  conn_id_buf = malloc(32);
  unsigned char *encrypt_buf;
  encrypt_buf = malloc(16 * 16);
  memcpy(encrypt_buf, buf + 4, size);
  unsigned char *tag_buf;
  tag_buf = malloc(256);

  memset(&ctx1->op, 0, sizeof(ctx1->op));
  ctx1->op.params[0].value.b = index; // the number of output
  ctx1->op.params[0].value.a = size; // size of data
  ctx1->op.params[1].tmpref.buffer = (void *) conn_id_buf;
  ctx1->op.params[1].tmpref.size = 32; // 16 * 2
  ctx1->op.params[2].tmpref.buffer = (void *) encrypt_buf;
  ctx1->op.params[2].tmpref.size = 16 * 16; // 16 * 16
  ctx1->op.params[3].tmpref.buffer = (void *) tag_buf;
  ctx1->op.params[3].tmpref.size = 256; // 16 * 16
  ctx1->op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_OUTPUT,
                TEEC_MEMREF_TEMP_INOUT, TEEC_MEMREF_TEMP_OUTPUT);


  TEEC_Session temp_sess1;
  TEEC_Context temp_ctx1;
  temp_ctx1.fd = ctx1->ctx.fd;
  temp_ctx1.reg_mem = ctx1->ctx.reg_mem;
  temp_ctx1.memref_null = ctx1->ctx.memref_null;
  
  temp_sess1.session_id = ctx1->sess.session_id;
  temp_sess1.ctx = &temp_ctx1;

  rc = TEEC_InvokeCommand(&temp_sess1, Entrypoint_User, &ctx1->op, &err_origin);

  if(rc == TEEC_SUCCESS) {
    int index = 0;
    for(int i = 0; i < ctx1->op.params[0].value.b; i++) {

      uint16_t conn_id = 0;
      unsigned char *handle_encrypt;
      unsigned char *handle_tag;
      int data_len = 0;
      data_len = encrypt_buf[index] & 0xFF;
      
      handle_encrypt = malloc(data_len);
      handle_tag = malloc(16);
      int j = 0;
      for(int m = (2 * i) + 1; m >= (2*i); --m){
        conn_id = conn_id + (( conn_id_buf[m] & 0xFF ) << (8*j));
        ++j;
      }
      memcpy(handle_encrypt, encrypt_buf + index + 1, data_len);
      memcpy(handle_tag, tag_buf + (16 * i), 16);

      reactive_handle_output(conn_id, handle_encrypt, data_len, handle_tag);

      index =  index + data_len + 1;
      free(handle_encrypt);
      free(handle_tag);
    }
  }
  
  // *************************************************
  free(conn_id_buf);
  free(encrypt_buf);
  free(tag_buf);

  if(rc != TEEC_SUCCESS) {
    return RESULT(ResultCode_InternalError);
  }

  return RESULT(ResultCode_Ok);
}

static int is_local_connection(Connection* connection) {
  return connection->local;
  
}

static void handle_local_connection(Connection* connection,
                          unsigned char *encrypt, uint32_t size, unsigned char *tag) {
    reactive_handle_input(connection->to_sm, connection->conn_id, encrypt, size, tag);
}

static void handle_remote_connection(Connection* connection,
                            unsigned char *encrypt, uint32_t size, unsigned char *tag) {
    unsigned char payload[23 + size];

    //----------------------------------------------------------
    int sockfd; 
    struct sockaddr_in servaddr; 

    char loopback[16] = "127.0.0.1";
    char ip[16] = {0};

    sprintf(ip, "%d.%d.%d.%d", connection->to_address.u8[0], connection->to_address.u8[1], 
                connection->to_address.u8[2],connection->to_address.u8[3]);

    if(strcmp(ip, loopback) == 0){
        sprintf(ip, "%d.%d.%d.%d", 10, 0, 2, 2); //10.0.2.2 --> QEMU gateway IP address
    }
	// socket create and varification 
    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1) { 
        printf("socket creation failed...\n"); 
        return;
    } 
    else
        printf("Socket successfully created..\n"); 
                    
    bzero(&servaddr, sizeof(servaddr)); 
  
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = inet_addr(ip);
    servaddr.sin_port = htons(connection->to_port); 
  
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) { 
        printf("connection with the server failed...\n"); 
		    perror("connect");
        return;
    } 
    else
        printf("connected to the server..\n"); 

    //---------------------------------------------------------------------
    uint16_t conn_id = htons(connection->conn_id);
    uint16_t to_sm   = htons(connection->to_sm);

    bzero(payload, 23 + size);
    payload[0] = command_code_to_u8(CommandCode_RemoteOutput);
    uint16_t htons_size = htons(2 + 2 + size + 16); // module id + conn id + cipher + tag
    memcpy(payload + 1, &htons_size, 2);
    memcpy(payload + 3, &to_sm, 2);
    memcpy(payload + 5, &conn_id, 2);
    memcpy(payload + 7, encrypt, size);
    memcpy(payload + 7 + size, tag, 16);
   
    // and send that buffer to client 
    write(sockfd, payload, sizeof(payload));
    bzero(payload, sizeof(payload)); 
    int ret = read(sockfd, payload, sizeof(payload));
    ResultCode code = u8_to_result_code(payload[0]);
    if(code == ResultCode_Ok){
      close(sockfd);
    }
}

void reactive_handle_output(uint16_t conn_id, unsigned char* encrypt, uint32_t size, unsigned char *tag)
{
  Connection* connection = connections_get(conn_id);

  if (is_local_connection(connection))
      handle_local_connection(connection, encrypt, size, tag);
  else
      handle_remote_connection(connection, encrypt, size, tag);
}

void reactive_handle_input(uint16_t sm, conn_index conn_id, 
                          unsigned char *encrypt, uint32_t size, unsigned char *tag) {

  struct timeval start, stop, tval_result;

	gettimeofday(&start, NULL);
	printf("Time elapsed in EM: %ld.%06ld\n", (long int)start.tv_sec, 
							(long int)start.tv_usec);
  
  TEEC_Result rc;
  uint32_t err_origin;
  UUID* uuid_struct = uuid_get(sm);
  //-----------------------------------------------------------------
  TA_CTX* ta_ctx = ta_ctx_get(uuid_struct->uuid);
  //-----------------------------------------------------------------
  unsigned char *conn_id_buf;
  conn_id_buf = malloc(32);
  unsigned char *encrypt_buf;
  encrypt_buf = malloc(16 * 16);
  memcpy(encrypt_buf, encrypt, size);
  unsigned char *tag_buf;
  tag_buf = malloc(256);
  memcpy(tag_buf, tag, 16);

  memset(&ta_ctx->op, 0, sizeof(ta_ctx->op));
	ta_ctx->op.params[0].value.a = size;
  ta_ctx->op.params[0].value.b = conn_id;
	ta_ctx->op.params[1].tmpref.buffer = (void *) conn_id_buf;
	ta_ctx->op.params[1].tmpref.size = 32;
	ta_ctx->op.params[2].tmpref.buffer = (void *) encrypt_buf;
	ta_ctx->op.params[2].tmpref.size = 16 * 16;
  ta_ctx->op.params[3].tmpref.buffer = (void *) tag_buf;
	ta_ctx->op.params[3].tmpref.size = 16 * 16;
  ta_ctx->op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_MEMREF_TEMP_OUTPUT,
					        TEEC_MEMREF_TEMP_INOUT, TEEC_MEMREF_TEMP_INOUT);

  TEEC_Session temp_sess;
  TEEC_Context temp_ctx;
  temp_ctx.fd = ta_ctx->ctx.fd;
  temp_ctx.reg_mem = ta_ctx->ctx.reg_mem;
  temp_ctx.memref_null = ta_ctx->ctx.memref_null;
 
  temp_sess.session_id = ta_ctx->sess.session_id;
  temp_sess.ctx = &temp_ctx;

  rc = TEEC_InvokeCommand(&temp_sess, Entrypoint_HandleInput, &ta_ctx->op, &err_origin);

  if (rc == TEEC_SUCCESS) {
    int index = 0;
    for(int i = 0; i < ta_ctx->op.params[0].value.b; i++) {
      uint16_t conn_id = 0;
      unsigned char *handle_encrypt;
      unsigned char *handle_tag;
      int data_len = 0;
      data_len = encrypt_buf[index] & 0xFF;
      
      handle_encrypt = malloc(data_len);
      handle_tag = malloc(16);
      int j = 0;
      for(int m = (2 * i) + 1; m >= (2*i); --m){
        conn_id = conn_id + (( conn_id_buf[m] & 0xFF ) << (8*j));
        ++j;
      }
      memcpy(handle_encrypt, encrypt_buf + index + 1, data_len);
      memcpy(handle_tag, tag_buf + (16 * i), 16);

      reactive_handle_output(conn_id, handle_encrypt, data_len, handle_tag);

      index =  index + data_len + 1;
      free(handle_encrypt);
      free(handle_tag);
    } // for
  } // if success
  // *************************************************
 
  free(conn_id_buf);
  free(encrypt_buf);
  free(tag_buf);
}
