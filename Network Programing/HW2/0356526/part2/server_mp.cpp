#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
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
#define CLIENTS_MAX	35

#define RECV_BUFF_SIZE	1000
#define CMD_BUFF_SIZE	260

#define PIPE_NULL	0
#define PIPE_IPC	1

#define CLIENT_MESSAGE_SIZE	1025
#define CLIENT_NAME_SIZE	25

/* Global Variabels*/
int shmid;
struct client_t *clients_info;

// Pipe Queue Struct
struct pipe_qt {
    int read_fd;
    int write_fd;
	int type;
	int occupied_pid;
	
	pipe_qt(): read_fd(0), write_fd(0), type(PIPE_NULL), occupied_pid(0) {}
};
struct command_t {
	deque<string> commands;
	int pipe_to_cmd;
	string pipe_to_file;
	int pipe_from_user;
	int pipe_to_user;
	
	command_t(): pipe_to_cmd(-1), pipe_to_file(""), pipe_from_user(-1), pipe_to_user(-1) {}
};

// Client Info Struct
struct client_message_t {
	int target;
	char content[CLIENT_MESSAGE_SIZE];
};
struct client_t {
    int id;
	int sockfd;
    char name[CLIENT_NAME_SIZE];
	char address[30];
	int pipes[CLIENTS_MAX];		// Incoming pipe
	client_message_t message;
	bool used;
	
	client_t() {
		used = false;
		for(int i = 0; i < CLIENTS_MAX; i++) {
			pipes[i] = 0;
		}
		message.target = -1;
	}
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
	while(result[result.length()-1] == '\n' || result[result.length()-1] == '\r')
		result.resize(result.length()-1);
	
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
deque<string> splitString(string target, const char pattern, bool save_space) {
	deque<string> result;
	istringstream ss(target);
	string token;

	while(std::getline(ss, token, pattern)) {
		if(token == "" && !save_space) continue;
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
int getFIFOName(int src, int tar, char *result) {
	sprintf(result, "/tmp/maziras_%d_to_%d", src, tar);
	
	return 1;
}

/* Master Tool */
void dump_cmd(command_t command) {
	cout << "Command: ";
	for(int j = 0; j < command.commands.size(); j++)
		cout << "[" << command.commands[j] << "] ";
	cout << endl << "| to command:\t " << command.pipe_to_cmd;
	cout << endl << "| to file:\t " << command.pipe_to_file;
	cout << endl << "| to user:\t " << command.pipe_to_user;
	cout << endl << "| from user:\t " << command.pipe_from_user << endl;
}
void dump_cmd_que(deque<command_t> command_queue) {
	for(int i = 0; i < command_queue.size(); i++)
		dump_cmd(command_queue[i]);
}

/* RAS Command Implementing */
int ras_talk(int target, char message[CLIENT_MESSAGE_SIZE], int speaker) {
	// Call parent to send message
	clients_info[speaker].message.target	= target;
	strcpy(clients_info[speaker].message.content, message);
	
	kill(getppid(), SIGUSR1);
	
	// Wait for message be sent
	while(clients_info[speaker].message.target >= 0);
	
	return 1;
}

int rascmd_who(deque<string> argv, int id) {
	cout << "<ID>\t<nickname>\t<IP/port>\t<indicate me>\n";
	for(int i = 0; i < CLIENTS_MAX; i++) {
		if(!clients_info[i].used)	continue;
		
		cout << clients_info[i].id << '\t' << clients_info[i].name << '\t' << clients_info[i].address;
		if(id == i)
			cout << "\t<-me";
		cout << '\n';
	}
	
	return 1;
}
int rascmd_tell(deque<string> argv, int id) {
	int tid;
	char message[CLIENT_MESSAGE_SIZE];
	if(argv.size() < 3) {
		cout << "Usage : tell <TARGET's ID> <MESSAGE>\n";
		return 0;
	}
	
	argv.pop_front();	// Pop command name
	tid = atoi(argv.front().c_str());
	argv.pop_front();	// Pop target's id
	
	if(tid < CLIENTS_MAX && !clients_info[tid].used) {
		cout << "*** Error: user #" << tid << " does not exist yet. ***\n";
		return 0;
	}
	
	sprintf(message, "*** %s told you ***: %s", clients_info[id].name, argv.front().c_str());
	argv.pop_front();
	while(!argv.empty()) {
		strcat(message, " ");
		strcat(message, argv.front().c_str());
		argv.pop_front();
	}
	strcat(message, "\n");
	
	ras_talk(tid, message, id);
	
	return 1;
}
int rascmd_yell(deque<string> argv, int id) {
	char message[CLIENT_MESSAGE_SIZE];
	if(argv.size() < 2) {
		cout << "Usage : yell <MESSAGE>\n";
		return 0;
	}
	
	argv.pop_front();	// Pop command name
	
	sprintf(message, "*** %s yelled ***: %s", clients_info[id].name, argv.front().c_str());
	argv.pop_front();
	while(!argv.empty()) {
		strcat(message, " ");
		strcat(message, argv.front().c_str());
		argv.pop_front();
	}
	strcat(message, "\n");
	
	ras_talk(0, message, id);
	
	return 1;
}
int rascmd_name(deque<string> argv, int id) {
	char tmp[100], name[CLIENT_NAME_SIZE];
	if(argv.size() < 2) {
		cout << "Usage : name <NAME>\n";
		return 0;
	}
	
	strcpy(name, argv[1].c_str());
	for(int i = 0; i < CLIENTS_MAX; i++) {
		if(strcmp(clients_info[i].name, name) == 0) {
			cout << "*** User '" << name << "' already exists. ***\n";
			return 0;
		}
	}
	
	strcpy(clients_info[id].name, name);
	sprintf(tmp, "*** User from %s is named '%s'. ***\n", clients_info[id].address, clients_info[id].name);
	ras_talk(0, tmp, id);
	
	return 1;
}

int rascmd_printenv(deque<string> argv, int id) {
	if(argv.size() < 2) {
		cout << "Usage : printenv <VARIABLE>\n";
		return 0;
	}
	if(getenv(argv[1].c_str()) == NULL)
		return 1;
		
	cout << argv[1] << "=" << getenv(argv[1].c_str()) << endl;	
	return 1;
}
int rascmd_setenv(deque<string> argv, int id) {
	if(argv.size() < 3) {
		cout << "Usage : setenv <VARIABLE> <VALUE>\n";
		return 0;
	}
	setenv(argv[1].c_str(), argv[2].c_str(), 1);
	return 1;
}
int rascmd_exit(deque<string> argv, int id) {
	return 2;	// Special case
}

/* RAS Command Execution */
map<string, int(*)(deque<string>, int) > ras_mapRascmd() {
	map<string, int(*)(deque<string>, int) > result;
	
	result["who"] = &rascmd_who;
	result["tell"] = &rascmd_tell;
	result["yell"] = &rascmd_yell;
	result["name"] = &rascmd_name;
	
	result["printenv"] = &rascmd_printenv;
	result["setenv"] = &rascmd_setenv;
	result["exit"] = &rascmd_exit;
	
	return result;
}
int ras_rascmdProcess(string cmd_name, deque<string> argv, int id) {
	int sockfd = clients_info[id].sockfd;
	map<string, int(*)(deque<string>, int) > m;
	int stdin_fd, stdout_fd, stderr_fd;
	int return_value;
	
	m = ras_mapRascmd();
	
	if(m.find(cmd_name) == m.end())
		return -1;	// Command not found
	
	// Redirect stdout & stderr
	stdin_fd	= dup(STDIN_FILENO);
	stdout_fd	= dup(STDOUT_FILENO);
	stderr_fd	= dup(STDERR_FILENO);
	dup2(sockfd, STDIN_FILENO);
	dup2(sockfd, STDOUT_FILENO);
	dup2(sockfd, STDERR_FILENO);
	
	return_value = (*m[cmd_name])(argv, id);
	
	// Restore stdout & stderr
	dup2(stdin_fd, STDIN_FILENO);
	dup2(stdout_fd, STDOUT_FILENO);
	dup2(stderr_fd, STDERR_FILENO);
	close(stdin_fd);
	close(stdout_fd);
	close(stderr_fd);
	
	return return_value;
}
int ras_cmdProcess(string cmd_name, deque<string> argv, int sockfd, int* fd, char* fifo_name) {
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
		// FIFO Write
		if(strlen(fifo_name) > 0) {
			fd[1] = open(fifo_name, O_WRONLY);
			fd[2] = fd[1];
		}
	
		// Redirect stdout & stderr
		dup2(fd[0], STDIN_FILENO);
		dup2(fd[1], STDOUT_FILENO);
		dup2(fd[2], STDERR_FILENO);
		
		if(execvp(cmd_name.c_str(), argv_c) < 0)
			return -1;
		
		exit(EXIT_SUCCESS);
	}
	
	// Do not wait for write FIFO
	if(strlen(fifo_name) == 0)
		waitpid(pid, NULL, 0);
	
	return pid;
}

/* RAS Command Process */
int ras_syntaxCheck(deque<command_t> cmd_queue) {
	for(int i = 0; i < cmd_queue.size(); i++) {
		// Two special charters stick together, e.g. ls | | cat
		if((cmd_queue[i].pipe_to_cmd > 0 || cmd_queue[i].pipe_to_file.length() || cmd_queue[i].pipe_from_user > 0
			|| cmd_queue[i].pipe_to_user > 0) && !cmd_queue[i].commands.size())
			return 0;
			
		// Special charters syntax error
		if(cmd_queue[i].pipe_to_cmd == 0)
			return 0;
		if(cmd_queue[i].pipe_to_user == 0)
			return 0;
		if(cmd_queue[i].pipe_from_user == 0)
			return 0;
		
		// Illegal charaters, e.g. /
		if(cmd_queue[i].commands.size() > 0)
			for(int j = 0; j < cmd_queue[i].commands.size(); j++)
				if(containChars(cmd_queue[i].commands[j], "/"))
					return 0;
	}
	
	return 1;
}
int ras_semanticsCheck(command_t action, int id) {
	int pipe_src = action.pipe_from_user, pipe_tar = action.pipe_to_user;
	char tmp[200];
	
	if(pipe_src > 0) {
		if(clients_info[id].pipes[pipe_src] < 1) {
			sprintf(tmp, "*** Error: the pipe #%d->#%d does not exist yet. ***\n", pipe_src, id);
			ras_talk(id, tmp, id);		
			return 0;
		}
	}
	
	if(pipe_tar > 0) {
		if(!clients_info[pipe_tar].used) {
			sprintf(tmp, "*** Error: user #%d does not exist yet. ***\n", pipe_tar);
			ras_talk(id, tmp, id);	
			return 0;
		} else if(clients_info[pipe_tar].pipes[id] > 0) {
			sprintf(tmp, "*** Error: the pipe #%d->#%d already exists. ***\n", id, pipe_tar);
			ras_talk(id, tmp, id);	
			return 0;
		}
	}
	
	return 1;
}
int fdHandler(command_t action, int child_fd, int id, string cmd_line, int* tar_fd, int* own_fd) {
	static deque<pipe_qt> pipe_queue;	// < read fd, write fd, pipe type >
	pipe_qt	pipe_info;
	char tmp[200];
	
	// Initial
	tar_fd[0] = child_fd;
	tar_fd[1] = child_fd;
	tar_fd[2] = child_fd;
	
	// Input file descriptor	
	if(!pipe_queue.empty()) {
		pipe_info = pipe_queue.front();
		pipe_queue.pop_front();
		if(pipe_info.type == PIPE_IPC){
			tar_fd[0] = pipe_info.read_fd;
		}
	}	
		
	// Set for close pipe
	own_fd[0] = pipe_info.read_fd;
	own_fd[1] = pipe_info.write_fd;
		
	// FIFO
	if(action.pipe_from_user > 0) {
		int pipe_src = action.pipe_from_user;
		char fifo_name[30];
		
		getFIFOName(action.pipe_from_user, id, fifo_name);
		tar_fd[0] = open(fifo_name, O_RDONLY);
	}
		
	// Output file descriptor	
	if(action.pipe_to_cmd > 0) {			// Pipe
		int pipe_tmp[2], pipe_tar = action.pipe_to_cmd-1;
		
		if(pipe_queue.size() < pipe_tar+1)
			pipe_queue.resize(pipe_tar+1);
		
		if(pipe_queue[pipe_tar].type == PIPE_NULL) {	// Create pipe for target
			if(pipe(pipe_tmp) < 0) {
				if(_DEBUG)	cout << "[Client] Fail to create pipe\n";
				return -1;
			}
			pipe_queue[pipe_tar].read_fd = pipe_tmp[0];
			pipe_queue[pipe_tar].write_fd = pipe_tmp[1];
			pipe_queue[pipe_tar].type = PIPE_IPC;
			
			tar_fd[1] = pipe_tmp[1];
		} else {							// Target already has pipe
			tar_fd[1] = pipe_queue[pipe_tar].write_fd;
		}
	} else if(action.pipe_to_user > 0) {	// FIFO
		int pipe_tmp[2], pipe_tar = action.pipe_to_user;
		char fifo_name[30];
		
		getFIFOName(id, pipe_tar, fifo_name);
		
		if(mkfifo(fifo_name, 0666) < 0) 
			cout << "[Client] fail to mkfifo\n";
		
		sprintf(tmp, "*** %s (#%d) just piped '%s' to %s (#%d) ***\n", clients_info[id].name, id, cmd_line.c_str(), clients_info[pipe_tar].name, pipe_tar);
		ras_talk(0, tmp, id);
	}
	
	return 1;
}
deque<command_t> ras_formatCommand(string target) {
	deque<string> token_queue;
	deque<command_t> result(1);
	
	// Split by [Space]
	token_queue = splitString(target, ' ');
	
	// Chat
	if(token_queue[0] == "tell" || token_queue[0] == "yell") {
		token_queue = splitString(target, ' ', 1);
		for(int i = 0; i < token_queue.size(); i++)
			result.back().commands.push_back(token_queue[i]);
		return result;
	}
	
	// Look for delimiters
	for(int i = 0; i < token_queue.size(); i++) {
		if(token_queue[i][0] == '|') {
			string tmp = token_queue[i].substr(1, token_queue[i].size()-1);
			if(tmp.empty())
				result.back().pipe_to_cmd = 1;
			else
				result.back().pipe_to_cmd = atoi(tmp.c_str());
			
			result.resize(result.size()+1);
		} else if (token_queue[i][0] == '>') {
			string tmp = token_queue[i].substr(1, token_queue[i].size()-1);
			if(tmp.empty())
				if(token_queue.size()-2 < i)
					result.back().pipe_to_cmd = 0;	// ls > (empty)
				else
					result.back().pipe_to_file = token_queue[++i];
			else
				result.back().pipe_to_user = atoi(tmp.c_str());
		} else if (token_queue[i][0] == '<') {
			string tmp = token_queue[i].substr(1, token_queue[i].size()-1);
			if(tmp.empty())
				result.back().pipe_from_user = 0;
			else
				result.back().pipe_from_user = atoi(tmp.c_str());
		} else {
			result.back().commands.push_back(token_queue[i]);
		}
	}
	
	return result;
}
int ras_processCommand(deque<command_t> cmd_queue, int id, string cmd_line) {
	// Initial
	int sockfd = clients_info[id].sockfd;
	
	while(!cmd_queue.empty()) {
		command_t action;
		deque<string> commands;
		string cmd_args, sp_op;
		int return_val, tar_fd[3], own_fd[2], file_fd = 0, cpid;
		char fifo_name[30] = "";
		
		action = cmd_queue.front();
		commands = action.commands;
		cmd_queue.pop_front();
		
		// Empty command
		if(commands.size() == 0)
			return 1;
		
		// RAS command
		return_val = ras_rascmdProcess(commands[0], commands, id);
		if(return_val == 0)	{			// RAS command but error
			return 1;
		} else if(return_val == 1)	{	// RAS command
			fdHandler(action, sockfd, id, cmd_line, tar_fd, own_fd);
			return 1;
		} else if(return_val == 2) {		// Client exit
			return 2;
		}
		
		// Check execute file
		if(!fileAccessUnderPATH(commands[0], F_OK)) {
			string tmp = "Unknown command: ["+commands[0]+"].\n";
			sendMessage(sockfd, tmp);
			return 0;
		}
		// Check for X permission
		if(!fileAccessUnderPATH(commands[0], X_OK)) {
			string tmp = "Permission denied: ["+commands[0]+"].\n";
			sendMessage(sockfd, tmp);
			return -1;
		}
		
		// Semantics Check
		if(ras_semanticsCheck(action, id) < 1)
			continue;
			
		// Prepare file descriper, aka pipe handler
		if(fdHandler(action, sockfd, id, cmd_line, tar_fd, own_fd) < 0)
			return -1;
		if(action.pipe_to_user > 0)
			getFIFOName(id, action.pipe_to_user, fifo_name);
			
		// Write to file
		if(action.pipe_to_file.length()) {
			FILE *file_ptr;
			string file_name = action.pipe_to_file;
						
			file_ptr = fopen(file_name.c_str(), "w");
			file_fd = file_ptr->_fileno;
			tar_fd[1] = file_fd;
		}
		
		// Close Write pipe for THIS PROCESS
		if(sockfd != own_fd[1] && own_fd[1] > 0)
			close(own_fd[1]);
		
		// Normal command
		if((cpid = ras_cmdProcess(commands[0], commands, sockfd, tar_fd, fifo_name)) < 0) 
			return 0;
		if(action.pipe_to_user > 0)
			clients_info[action.pipe_to_user].pipes[id] = cpid;
		
		// Close Read pipe for THIS PROCESS
		if(sockfd != own_fd[0] && own_fd[0] > 0)
			close(own_fd[0]);
		
		// Close file
		if(file_fd)
			close(file_fd);
			
		// Close FIFO
		if(action.pipe_from_user > 0) {
			char tmp[200];
			
			getFIFOName(action.pipe_from_user, id, fifo_name);
			sprintf(tmp, "*** %s (#%d) just received from %s (#%d) by '%s' ***\n", clients_info[id].name, id, clients_info[action.pipe_from_user].name, action.pipe_from_user, cmd_line.c_str());
			ras_talk(0, tmp, id);
			close(tar_fd[0]);
			unlink(fifo_name);
			
			clients_info[id].pipes[action.pipe_from_user] = 0;
		}
	}
	
	return 1;
}

/* MAIN */
void s_reaper(int signal) {
	int status;
	while(waitpid(0, &status, WNOHANG) > 0);
}
void reaper(int signal) {
	int status, id;
	while(waitpid(0, &status, WNOHANG) > 0) {
		if(WIFEXITED(status)) {
			id = WEXITSTATUS(status);
			close(clients_info[id].sockfd);
		}
	}
}
void testament(int signal) {
	// Release shm
	shmctl(shmid, IPC_RMID, NULL);
	
	exit(0);
}
void messageHandler(int signal) {
	for(int i = 0; i < CLIENTS_MAX; i++) {
		int target = clients_info[i].message.target;
		string content = clients_info[i].message.content;
		if(target == -1)	continue;
		
		if(target == 0)	{			// Broadcast
			for(int j = 0; j < CLIENTS_MAX; j++)
				if(clients_info[j].used)
					sendMessage(clients_info[j].sockfd, content);
		} else if(target == -2)	{	// Broadcast exclude self
			for(int j = 0; j < CLIENTS_MAX; j++)
				if(clients_info[j].used && i != j)
					sendMessage(clients_info[j].sockfd, content);
		} else {					// Whisper
			sendMessage(clients_info[target].sockfd, content);
		}
		
		clients_info[i].message.target = -1;
	}
}

int ras_login(char* address, int childfd) {
	int id;
	char tmp[100];

	while(clients_info[++id].used);
	
	clients_info[id].id = id;
	clients_info[id].sockfd = childfd;
	strcpy(clients_info[id].name, "(no name)");
	strcpy(clients_info[id].address, address);
	clients_info[id].used = true;
	clients_info[id].message.target = -1;
		
	return id;
}
int ras_logout(int id) {
	pipe_qt	pipe_info;
	char tmp[100], fifo_name[30];
	int self_fd = clients_info[id].sockfd;
	
	if(_DEBUG)	cout << "[Client] " << clients_info[id].address << " Exit\n";
	
	// Leave Message
	sprintf(tmp, "*** User '%s' left. ***\n", clients_info[id].name);
	ras_talk(0, tmp, id);
	
	// Clear FIFO (not FIFO at here though...)
	for(int i = 0; i < CLIENTS_MAX; i++) {
		// To me
		if(clients_info[id].pipes[i]) {
			getFIFOName(i, id, fifo_name);
			unlink(fifo_name);
			kill(clients_info[id].pipes[i], SIGTERM);
			clients_info[id].pipes[i] = 0;
		}
		// From me
		if(clients_info[i].pipes[id]) {
			getFIFOName(id, i, fifo_name);
			unlink(fifo_name);
			kill(clients_info[i].pipes[id], SIGTERM);
			clients_info[i].pipes[id] = 0;
		}
	}
	
	clients_info[id].used = false;
	
	// Close
	shmdt(clients_info);
	close(self_fd);
	
	return 1;
}
int ras(int id) {
	// Initial
	char tmp[100];
	int self_fd = clients_info[id].sockfd;
	
	signal(SIGCHLD, s_reaper); 			// Reap slave's child
	
	clearenv();
	setenv("PATH", "bin:.", 1);
	
	// Welcome Message	
	sprintf(tmp, "*** User '%s' entered from %s. ***\n", clients_info[id].name, clients_info[id].address);
	sendMessage(self_fd, "****************************************\n** Welcome to the information server. **\n****************************************\n");
	ras_talk(0, tmp, id);
	
	while(1) {
		string recv_message;
		deque<command_t> command_queue;
		
		// Prompt
		sendMessage(self_fd, "% ");
		if(!recvMessage(self_fd, recv_message))	break;
		//cout << '[' << recv_message << ']' << endl;
		
		// Empty input
		if(recv_message.length() < 1)
			continue;
		
		// Format command
		command_queue = ras_formatCommand(recv_message);
		
		// Syntax Check
		if(!ras_syntaxCheck(command_queue)) {
			sendMessage(self_fd, "Syntax error.\n");
			continue;
		}
		
		// Process command
		if(ras_processCommand(command_queue, id, recv_message) == 2)
			break;		// Client exit
	}
	
	// Logout
	ras_logout(id);
	
	return id;
}

int createServer(int server_port) {
	int sockfd;
	struct sockaddr_in serv_addr;
	
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
	
	return sockfd;
}
int main(int argc, char *argv[]) {
	int sockfd, childfd, pid, server_port = DEFAULT_SERVER_PORT;
	unsigned int cli_len;
	struct sockaddr_in serv_addr, cli_addr;
		
	// Initial
	if(argc > 1)
		server_port = atoi(argv[1]);
	
	signal(SIGINT, testament); 			// Be killed :(
	signal(SIGCHLD, reaper); 			// Reap slave
	signal(SIGUSR1, messageHandler); 	// Handle message from client
	
	chdir(getenv("HOME"));
	chdir("./ras");
	
	// Share memory
	if ((shmid = shmget(IPC_PRIVATE, CLIENTS_MAX*sizeof(client_t), IPC_CREAT | 0666)) < 0) {
		cout << "[Server] shmget fail\n";
		exit(EXIT_FAILURE);
	}
	clients_info = ((client_t*) shmat(shmid, NULL, 0));
		
	sockfd = createServer(server_port);
	
	// Accept
	while(1) {
		int id;
		char cli_ip[20], address[30];		
	
		if(_DEBUG)	cout << "[Server] Waiting for connections on " << server_port << "...\n";
		
		cli_len = sizeof(cli_addr);
		if((childfd = accept(sockfd, (struct sockaddr *) &cli_addr, &cli_len)) < 0) {
			cout << "[Server] Failed to accept\n";
			continue;
		}	
		
		inet_ntop(AF_INET, &cli_addr.sin_addr, cli_ip, sizeof(cli_addr));
		sprintf(address, "%s/%d", cli_ip, ntohs(cli_addr.sin_port));
		
		if(_DEBUG)	cout << "[Server] " << address << " connected\n";
		
		// Login
		id = ras_login(address, childfd);
			
		// Fork child socket
		pid = fork();
		
		if(pid < 0)
			cout << "[Server] Failed to fork\n";
		else if(pid == 0) {			
			
			close(sockfd);
			for(int i = 0; i < CLIENTS_MAX; i++)
				if(clients_info[i].used && id != i)
					close(clients_info[i].sockfd);					
			
			exit(ras(id));
		}
	}
	
	// Release shm
	shmctl(shmid, IPC_RMID, NULL);
	return 0;
}
