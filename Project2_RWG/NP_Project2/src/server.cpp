#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <errno.h>
#include <sstream>
#include <cstring>
#include <string>
using namespace std;
#define _DEBUG          1

#define DEFAULT_SERVER_PORT     9081
#define CLIENTS_MAX     31

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
            cout << errno<<"\n";
            cout << "[Server] Failed to accept\n";
            continue;
        }

        int child_pid=0;
        if((child_pid=fork())==1){
            printf("I fall my people\n");
            _exit(1);
        }else if(child_pid==0) {// children process
            if(_DEBUG)      cout << "[Server] Connected\n";
            close(sockfd);
            dup2(childfd,STDOUT_FILENO);
            dup2(childfd,STDIN_FILENO);
            dup2(childfd,STDERR_FILENO);
            char *arg[]={NULL};
            cout<<"****************************************"<<endl;
            cout<<"** Welcome to the information server. **"<<endl;
            cout<<"****************************************"<<endl;
            execv("./bin/mybash",arg);
            // Fork child socket
        }else{//parent process
            close(childfd);

        }

    }

    return 0;
}          





