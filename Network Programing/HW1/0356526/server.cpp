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
#include <deque>
#include <map>
using namespace std;

#define _DEBUG		1

#define DEFAULT_SERVER_PORT	8588
#define CLIENTS_MAX	10

#define RECV_BUFF_SIZE	1000
#define CMD_BUFF_SIZE	260

#define PIPE_NULL	0
#define PIPE_IPC	1

// Pipe Queue Struct
struct pipe_qt {
    int read_fd;
    int write_fd;
	int type;
	int occupied_pid;
	
	pipe_qt(): read_fd(0), write_fd(0), type(PIPE_NULL), occupied_pid(0) {}
};

/* Tools */
int sendMessage(int sockfd, string message) {
	
	write(sockfd, message.c_str(), strlen(message.c_str()));
	
	return 1;
}
int recvMessage(int sockfd, string &result) {
	char recv_buff[RECV_BUFF_SIZE];
	int len = 0;
	
	// Clear buffer
	result.clear();
	
	while(1) {
		if((len = read(sockfd, recv_buff, RECV_BUFF_SIZE-1)) <= 0)
			return 0;
			
		recv_buff[len] = 0;
		// Concatenate broken string
		result += recv_buff;
		if(recv_buff[len-1] == '\n')			
			break;
	}
	
	// Remove nextline
	result.resize(result.length()-2);
	
	return 1;
}
deque<string> splitString(string target, const char pattern) {
	deque<string> result;
	istringstream ss(target);
	string token;

	while(std::getline(ss, token, pattern)) {
		if(token == "") continue;
		result.push_back(token);
	}
	
	return result;
}
char containChars(string target, string pattern) {
	size_t found = target.find_first_of(pattern);
	if(found != std::string::npos)
		return target[found];
		
	return 0;
}
int fileAccessUnderPATH(string fname, int mode) {
	deque<string> paths;
	string path = getenv("PATH");
	
	paths = splitString(path, ':');
	for(int i = 0; i < paths.size(); i ++) {
		string tmp = paths[i] + '/' + fname;
		if(access(tmp.c_str(), mode) == 0)
			return 1;	// Found file
	}
	return 0;
}

int getPipeType(char target) {
	switch(target) {
		case '|':	return PIPE_IPC;
	}
	return PIPE_NULL;
}

/* RAS Command Implementing */
int rascmd_printenv(deque<string> argv) {
	if(argv.size() < 2) {
		cout << "Usage : printenv <VARIABLE>\n";
		return 1;
	}
	
	cout << argv[1] << "=" << getenv(argv[1].c_str()) << endl;	
	return 1;
}
int rascmd_setenv(deque<string> argv) {
	if(argv.size() < 3) {
		cout << "Usage : setenv <VARIABLE> <VALUE>\n";
		return 1;
	}
	setenv(argv[1].c_str(), argv[2].c_str(), 1);
	return 1;
}
int rascmd_exit(deque<string> argv) {
	return 2;	// Special case
}

/* RAS Command Execution */
map<string, int(*)(deque<string>) > ras_mapRascmd() {
	map<string, int(*)(deque<string>) > result;
	
	result["printenv"] = &rascmd_printenv;
	result["setenv"] = &rascmd_setenv;
	result["exit"] = &rascmd_exit;
	
	return result;
}
int ras_rascmdProcess(string cmd_name, deque<string> argv, int sockfd) {
	map<string, int(*)(deque<string>) > m;
	int stdin_fd, stdout_fd, stderr_fd;
	int return_value;
	
	m = ras_mapRascmd();
	
	if(m.find(cmd_name) == m.end())
		return 0;	// Command not found
	
	// Redirect stdout & stderr
	stdin_fd	= dup(STDIN_FILENO);
	stdout_fd	= dup(STDOUT_FILENO);
	stderr_fd	= dup(STDERR_FILENO);
	dup2(sockfd, STDIN_FILENO);
	dup2(sockfd, STDOUT_FILENO);
	dup2(sockfd, STDERR_FILENO);
	
	return_value = (*m[cmd_name])(argv);
	
	// Restore stdout & stderr
	dup2(stdin_fd, STDIN_FILENO);
	dup2(stdout_fd, STDOUT_FILENO);
	dup2(stderr_fd, STDERR_FILENO);
	close(stdin_fd);
	close(stdout_fd);
	close(stderr_fd);
	
	if(return_value == 2)
		return 2;		// Exit
	
	return 1;
}
int ras_cmdProcess(string cmd_name, deque<string> argv, int sockfd, int* fd) {
	char **argv_c;
	int pid;
	
	argv_c = (char**)malloc((argv.size()+1) * sizeof(char*));
		
	// Prepare arguments
	for(int i = 0; i < argv.size(); i++) {
		argv_c[i] = (char*)malloc(CMD_BUFF_SIZE * sizeof(char));
		strcpy(argv_c[i], argv[i].c_str());
	}
	argv_c[argv.size()] = (char*)malloc(CMD_BUFF_SIZE * sizeof(char));
	argv_c[argv.size()] = NULL;
	
	// Execute
	pid = fork();
	
	if(pid < 0)
		cout << "[Client] Failed to fork\n";
	else if(pid == 0) {
		// Redirect stdout & stderr
		dup2(fd[0], STDIN_FILENO);
		dup2(fd[1], STDOUT_FILENO);
		dup2(fd[2], STDERR_FILENO);
		
		if(execvp(cmd_name.c_str(), argv_c) < 0)
			return -1;
		exit(EXIT_SUCCESS);
	}
	
	waitpid(pid, NULL, 0);
	
	return pid;
}

/* RAS Command Process */
int ras_syntaxCheck(deque<deque<string> > cmd_queue) {
	for(int i = 0; i < cmd_queue.size(); i++) {
		// Two special charters stick together, e.g. ls | | cat
		if(cmd_queue[i].size() && containChars(cmd_queue[i][0] , "|>"))
			return 0;
			
		// Special charters syntax error
		if(i < cmd_queue.size()-1) {
			string sp_op = cmd_queue[i].back();
			if(sp_op[0] == '|') {
				string tmp = sp_op.substr(1, sp_op.size()-1);
				if(!(tmp.empty() || atoi(tmp.c_str()) > 0))
					return 0;
			} else if(sp_op[0] == '>') {
				if(sp_op != ">")
					return 0; 
			}
		}
		
		// Illegal charaters, e.g. /
		if(cmd_queue[i].size() > 0)
			for(int j = 0; j < cmd_queue[i].size(); j++)
				if(containChars(cmd_queue[i][j], "/"))
					return 0;
	}
	
	return 1;
}
// Return 0: normal, 1: file pipe
int fdHandler(string sp_op, int child_fd, int* tar_fd, int* own_fd) {
	static deque<pipe_qt> pipe_queue;	// < read fd, write fd, pipe type >
	pipe_qt	pipe_info;
	
	// Input file descriptor
	tar_fd[0] = child_fd;
	
	if(!pipe_queue.empty()) {
		pipe_info = pipe_queue.front();
		pipe_queue.pop_front();
		if(pipe_info.type == PIPE_IPC){
			tar_fd[0] = pipe_info.read_fd;
		}
	}
		
	// Output file descriptor
	tar_fd[1] = child_fd;
	
	if(!sp_op.empty() && sp_op[0] == '|') {
		int pipe_tmp[2], pipe_tar = 0;
		
		if(sp_op.length() > 1)		// Dump crap
			pipe_tar = atoi(sp_op.substr(1, sp_op.size()-1).c_str())-1;
		
		if(pipe_queue.size() < pipe_tar+1)
			pipe_queue.resize(pipe_tar+1);
		
		if(pipe_queue[pipe_tar].type == PIPE_NULL) {	// Create pipe for target
			if(pipe(pipe_tmp) < 0) {
				if(_DEBUG)	cout << "[Client] Fail to create pipe\n";
				return -1;
			}
			pipe_queue[pipe_tar].read_fd = pipe_tmp[0];
			pipe_queue[pipe_tar].write_fd = pipe_tmp[1];
			pipe_queue[pipe_tar].type = getPipeType(sp_op[0]);
			
			tar_fd[1] = pipe_tmp[1];
		} else {										// Target already has pipe
			tar_fd[1] = pipe_queue[pipe_tar].write_fd;
		}
	}
	
	// Output file descriptor
	tar_fd[2] = child_fd;
	
	// Set for close pipe
	own_fd[0] = pipe_info.read_fd;
	own_fd[1] = pipe_info.write_fd;
	
	return 0;
}
deque<deque<string> > ras_splitCommand(string target, string sp_char) {
	deque<string> token_queue;
	deque<deque<string> > result(1);
	
	// Split by [Space]
	token_queue = splitString(target, ' ');
	
	// Look for delimiters
	for(int i = 0; i < token_queue.size(); i++) {
		result.back().push_back(token_queue[i]);
		
		for(int sp_i = 0; sp_i < sp_char.size(); sp_i++) {
			if(token_queue[i][0] == sp_char[sp_i]) {				
				result.resize(result.size()+1);
				break;
			}
		}
	}
	
	return result;
}
int ras_processCommand(deque<deque<string> > cmd_queue, int sockfd) {
	while(!cmd_queue.empty()) {
		deque<string> cmd;
		string cmd_args, sp_op;
		int return_val, tar_fd[3], own_fd[2], file_fd = 0;
		pid_t pid;
		
		cmd = cmd_queue.front();
		cmd_queue.pop_front();
		
		// Empty command
		if(cmd.size() == 0)
			return 1;
		
		// RAS command
		return_val = ras_rascmdProcess(cmd[0], cmd, sockfd);
		if(return_val == 1)	// This is a RAS command
			return 1;		
		else if(return_val == 2)
			return 2;		// Client exit
		
		// Check execute file
		if(!fileAccessUnderPATH(cmd[0], F_OK)) {
			string tmp = "Unknown command: ["+cmd[0]+"].\n";
			sendMessage(sockfd, tmp);
			return 0;
		}
		// Check for X permission
		if(!fileAccessUnderPATH(cmd[0], X_OK)) {
			string tmp = "Permission denied: ["+cmd[0]+"].\n";
			sendMessage(sockfd, tmp);
			return -1;
		}
			
		// Prepare file descriper, aka pipe handler
		if(!cmd_queue.empty()) {
			sp_op = cmd.back();
			cmd.pop_back();
		}
		if(fdHandler(sp_op, sockfd, tar_fd, own_fd) < 0)
			return -1;
			
		// Write to file
		if(sp_op == ">") {
			FILE *file_ptr;
			string file_name;
			
			file_name = cmd_queue.front()[0];
			cmd_queue.pop_front();
			
			file_ptr = fopen(file_name.c_str(), "w");
			file_fd = file_ptr->_fileno;
			tar_fd[1] = file_fd;
		}
		
		// Close Write pipe for THIS PROCESS
		if(sockfd != own_fd[1] && own_fd[1] > 0)
			close(own_fd[1]);
		
		// Normal command
		if((pid = ras_cmdProcess(cmd[0], cmd, sockfd, tar_fd)) < 0) 
			return 0;
				
		// Close Read pipe for THIS PROCESS
		if(sockfd != own_fd[0] && own_fd[0] > 0)
			close(own_fd[0]);
		
		// Close file
		if(file_fd)
			close(file_fd);
	}
	
	return 1;
}

/* MAIN */
void reaper(int signal) {
	while(waitpid(0, NULL, WNOHANG) >= 0);
}

int ras(int sockfd) {
	// Initial
	setenv("PATH", "bin:.", 1); 
	
	// Welcome Message
	sendMessage(sockfd, "****************************************\n** Welcome to the information server. **\n****************************************\n");
	
	while(1) {
		string recv_message;
		deque<deque<string> > command_queue;
		
		// Prompt
		sendMessage(sockfd, "% ");
		if(!recvMessage(sockfd, recv_message))	break;
		//cout << '[' << recv_message << ']' << recv_message.length() << '\n';
		
		// Empty input
		if(recv_message.length() < 1)
			continue;
		
		// Format command
		command_queue = ras_splitCommand(recv_message, "|>");
		
		// Syntax Check
		if(!ras_syntaxCheck(command_queue)) {
			sendMessage(sockfd, "Syntax error.\n");
			continue;
		}
		
		// Process command
		if(ras_processCommand(command_queue, sockfd) == 2)
			break;		// Client exit
	}
	
	// Close
	close(sockfd);
	
	if(_DEBUG)	cout << "[Client] Exit\n";
	
	return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
	int sockfd, childfd, pid, server_port = DEFAULT_SERVER_PORT;
	unsigned int cli_len;
	struct sockaddr_in serv_addr, cli_addr;
		
	// Initial
	if(argc > 1)
		server_port = atoi(argv[1]);
	
	signal(SIGCHLD, reaper); 	// Reap slave
	
	chdir(getenv("HOME"));
	chdir("./ras");
	
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
	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr	= htonl(INADDR_ANY);
	serv_addr.sin_port			= htons(server_port);
	
	if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		cout << "[Server] Cannot bind address\n";
		exit(EXIT_FAILURE);
	}
	
	// Listen
	if(listen(sockfd, CLIENTS_MAX) < 0) {
		cout << "[Server] Failed to listen\n";
		exit(EXIT_FAILURE);
	}
	
	// Accept
	while(1) {
		if(_DEBUG)	cout << "[Server] Waiting for connections on " << server_port << "...\n";
		
		if((childfd = accept(sockfd, (struct sockaddr *) &cli_addr, &cli_len)) < 0) {
			cout << "[Server] Failed to accept\n";
			continue;
		}
		
		if(_DEBUG)	cout << "[Server] Connected\n";
		
		// Fork child socket
		pid = fork();
		
		if(pid < 0)
			cout << "[Server] Failed to fork\n";
		else if(pid == 0) {
			close(sockfd);
			exit(ras(childfd));
		} else {
			close(childfd);
		}
	}
	
	return 0;
}
