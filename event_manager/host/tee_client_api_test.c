#include "tee_client_api.h"

#include <stdio.h>

#include "logging.h"

TEEC_Result TEEC_InitializeContext(const char *name, TEEC_Context *context) {
    INFO("TEEC_InitializeContext");
    return TEEC_SUCCESS;
}

void TEEC_FinalizeContext(TEEC_Context *context) {
    INFO("TEEC_FinalizeContext");
}

TEEC_Result TEEC_OpenSession(TEEC_Context *context,
			     TEEC_Session *session,
			     const TEEC_UUID *destination,
			     uint32_t connectionMethod,
			     const void *connectionData,
			     TEEC_Operation *operation,
			     uint32_t *returnOrigin) {
    INFO("TEEC_OpenSession");
    return TEEC_SUCCESS;
}

void TEEC_CloseSession(TEEC_Session *session) {
    INFO("TEEC_CloseSession");
}

TEEC_Result TEEC_InvokeCommand(TEEC_Session *session,
			       uint32_t commandID,
			       TEEC_Operation *operation,
			       uint32_t *returnOrigin) {
    INFO("TEEC_InvokeCommand");
    return TEEC_SUCCESS;
}

TEEC_Result TEEC_RegisterSharedMemory(TEEC_Context *context,
				      TEEC_SharedMemory *sharedMem) {
    INFO("TEEC_RegisterSharedMemory");
    return TEEC_SUCCESS;
}

TEEC_Result TEEC_AllocateSharedMemory(TEEC_Context *context,
				      TEEC_SharedMemory *sharedMem) {
    INFO("TEEC_AllocateSharedMemory");
    return TEEC_SUCCESS;
}

void TEEC_ReleaseSharedMemory(TEEC_SharedMemory *sharedMemory) {
    INFO("TEEC_ReleaseSharedMemory");
}

void TEEC_RequestCancellation(TEEC_Operation *operation) {
    INFO("TEEC_ReleaseSharedMemory");
}