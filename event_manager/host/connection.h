#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#include <stdint.h>

#include <netinet/in.h>

typedef struct
{
    uint16_t        conn_id;
    uint16_t        to_sm;
    uint16_t        to_port;
    struct in_addr  to_address;
    uint8_t         local;
} Connection;

// Copies connection so may be stack allocated.
int connections_add(Connection* connection);

// We keep ownership of the returned Connection. May return NULL.
Connection* connections_get(uint16_t conn_id);

// Replaces an existing connection
int connections_replace(Connection* connection);

// Delete all connections 
void delete_connections(void);

#endif
