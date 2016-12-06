#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <cstring>
#include <string>
using namespace std;
#define _DEBUG          1

#define DEFAULT_SERVER_PORT     9081
#define CLIENTS_MAX     10

#define RECV_BUFF_SIZE  1000
#define CMD_BUFF_SIZE   260

#define PIPE_NULL       0
#define PIPE_IPC        1


int main(int argc, char *argv[]) {
        int sockfd, childfd, pid, server_port = DEFAULT_SERVER_PORT;
        unsigned int cli_len;
        struct sockaddr_in serv_addr, cli_addr;

        // Socket
        if((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
                cout << "[Server] Cannot create socket\n";
                exit(EXIT_FAILURE);
        }

        // Reuse address
        int ture = 1;
        if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &ture, sizeof(ture)) < 0) {
                cout << "[Server] Setsockopt failed\n";
                exit(EXIT_FAILURE);
        }

        // Bind
        serv_addr.sin_family            = AF_INET;
        serv_addr.sin_addr.s_addr       = htonl(INADDR_ANY);
        serv_addr.sin_port              = htons(server_port);

        if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
                cout << "[Server] Cannot bind address\n";
                exit(EXIT_FAILURE);
        }

        // Listen
        if(listen(sockfd, CLIENTS_MAX) < 0) {
                cout << "[Server] Failed to listen\n";
	}
        // Accept
        while(1) {
                if(_DEBUG)      cout << "[Server] Waiting for connections on " << server_port << "...\n";

                if((childfd = accept(sockfd, (struct sockaddr *) &cli_addr, &cli_len)) < 0) {
                        cout << "[Server] Failed to accept\n";
                        continue;
                }

                dup2(childfd,STDOUT_FILENO);
                dup2(childfd,STDIN_FILENO);
                execlp("../mybash/mybash",NULL);
                // Fork child socket
                if(_DEBUG)      cout << "[Server] Connected\n";
                close(childfd);
        }

        return 0;
}          
