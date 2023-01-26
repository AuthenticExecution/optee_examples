#include <stdio.h>
#include <stdlib.h>

#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>

#include "logging.h"
#include "networking.h"
#include "event_manager.h"

#define DEFAULT_PORT 1236
#define BACKLOG 5

void sig_handler(int signum){
    WARNING("Client disconnected");
}

int main(int argc, char const* argv[])
{
	int server_fd, client_socket;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof(address);

    uint16_t port;
    if(argc >= 2) {
        port = atoi(argv[1]);
    } else {
        port = DEFAULT_PORT;
    }

    // Register signal handler in case client disconnects
    signal(SIGPIPE, sig_handler);

	// Creating socket file descriptor
	if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		ERROR("socket failed");
		exit(EXIT_FAILURE);
	}

	// Forcefully attaching socket to the port
	if(setsockopt(
        server_fd, SOL_SOCKET,
		SO_REUSEADDR | SO_REUSEPORT, 
        &opt,
		sizeof(opt)
    )) {
		ERROR("setsockopt failed");
		exit(EXIT_FAILURE);
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	// Forcefully attaching socket to the port
	if(bind(
        server_fd, 
        (struct sockaddr*)&address,
		sizeof(address)
        ) < 0
    ) {
		ERROR("bind failed");
		exit(EXIT_FAILURE);
	}

    // listen to port
	if(listen(server_fd, BACKLOG) < 0) {
		ERROR("listen failed");
		exit(EXIT_FAILURE);
	}

    INFO("Listening to 0.0.0.0:%d", port);

    while(1) {        
        if((client_socket = accept(
            server_fd,
            (struct sockaddr*)&address,
            (socklen_t*)&addrlen
            )) < 0
        ) {
            WARNING("failed to accept new connection");
            exit(EXIT_FAILURE);
        }

        DEBUG("Accepted new connection");

        CommandMessage m = read_command_message(client_socket);
        if(m == NULL) {
            ERROR("Failed to read command");
        } else {
            //DEBUG("Read cmd. ID: %d buf size: %u", m->code, m->message->size);
            ResultMessage res = process_message(m);

            if(res != NULL) {
                DEBUG("Result code: %d, size: %u", res->code, res->message->size);
                write_result_message(client_socket, res);
                destroy_result_message(res);
            }
        }

        // closing the connected socket
        close(client_socket);
        DEBUG("Connection closed");
    }

	// closing the listening socket
	shutdown(server_fd, SHUT_RDWR);
	return 0;
}
