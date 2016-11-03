#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/stat.h>
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

#define _DEBUG	1

#define DEFAULT_SERVER_PORT	8868
#define CLIENTS_MAX	10

#define RECV_BUFF_SIZE	1000
#define CMD_BUFF_SIZE	260

struct ReqHeader_t {
	string version;
	string request;
	string method;
	string query;
	
	map<string, string> data;
};

/* Tools */
int sendMessage(int sockfd, string message) {
	if(write(sockfd, message.c_str(), strlen(message.c_str())) < 0)
		cout << "Write fail";
	
	return 1;
}
int recvLine(int sockfd, string &result) {
	char recv_buff;
	
	// Clear buffer
	result.clear();

	while(1) {
		if(recv(sockfd, &recv_buff, 1, 0) <= 0)
			return 0;
			
		// Wait for CRLF
		if(recv_buff == '\r') {
			if (recv(sockfd, &recv_buff, 1, MSG_PEEK) > 0 && recv_buff == '\n')
				recv(sockfd, &recv_buff, 1, 0);
			return 1;
		}
		
		// Append char
		result += recv_buff;
	}
	
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
deque<string> divideString(string target, string pattern) {
	deque<string> result;	
	size_t found = target.find(pattern);
	
	if(found != string::npos) {		// Found
		result.push_back(target.substr(0, found));
		result.push_back(target.substr(found+pattern.length(), string::npos));
	} else {						// Not Found
		result.push_back(target);
		result.push_back("");
	}
	
	return result;
}
bool endwithString (string const &fullString, string const &ending) {
	if (fullString.length() >= ending.length())
		return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
	else
		return false;
}
int isdir(string fname) {
	struct stat st;
	if(stat(fname.c_str(), &st) == 0)
		return S_ISDIR(st.st_mode);
}

/* Prtocol */
int httpd_recvHeader(int sockfd, ReqHeader_t &header) {
	string recv_message;
	bool first = 1;
	
	while(1) {
		deque<string> tmp;		
		
		if(!recvLine(sockfd, recv_message))
			return -1;
		// Wait for end of header (blank line)
		if(recv_message == "")	break;
				
		if(first) {	
			deque<string> tmp2;
			
			tmp = splitString(recv_message, ' ');
			if(tmp.size() != 3)	return -1;
				
			tmp2 = divideString("."+tmp[1], "?");
			
			header.method	= tmp[0];
			header.request	= tmp2[0];
			header.query	= tmp2[1];
			header.version	= tmp[2];
			first = 0;
		} else {
			tmp = divideString(recv_message, ": ");
			if(tmp.size() != 2)	return -1;
			
			header.data[tmp[0]] = tmp[1];
		}
	}
	
	return 1;
}

int httpd_respond400(int sockfd) {	
	sendMessage(sockfd, "HTTP/1.0 400 BAD REQUEST\r\n");
	sendMessage(sockfd, "Content-type: text/html\r\n");
	sendMessage(sockfd, "\r\n");
	sendMessage(sockfd, "<h1>400 - Bad Request</h1>\r\n");
	sendMessage(sockfd, "<hr>\r\n");
	sendMessage(sockfd, "<p>Well... What a bad request, huh?</p>\r\n");
	
	return 1;
}
int httpd_respond403(int sockfd) {	
	sendMessage(sockfd, "HTTP/1.0 403 FORBIDDEN\r\n");
	sendMessage(sockfd, "Content-type: text/html\r\n");
	sendMessage(sockfd, "\r\n");
	sendMessage(sockfd, "<h1>403 - Forbidden</h1>\r\n");
	sendMessage(sockfd, "<hr>\r\n");
	sendMessage(sockfd, "<p>U can't touch this.</p>\r\n");
	
	return 1;
}
int httpd_respond404(int sockfd) {	
	sendMessage(sockfd, "HTTP/1.0 404 NOT FOUND\r\n");
	sendMessage(sockfd, "Content-type: text/html\r\n");
	sendMessage(sockfd, "\r\n");
	sendMessage(sockfd, "<h1>404 - Not Found</h1>\r\n");
	sendMessage(sockfd, "<hr>\r\n");
	sendMessage(sockfd, "<p>You can't see me.<br>P.S. Directory counts as 404 by now.</p>\r\n");
	
	return 1;
}
int httpd_respond500(int sockfd) {	
	sendMessage(sockfd, "HTTP/1.0 500 Internal Server Error\r\n");
	sendMessage(sockfd, "Content-type: text/html\r\n");
	sendMessage(sockfd, "\r\n");
	sendMessage(sockfd, "<h1>500 - Internal Server Error</h1>\r\n");
	sendMessage(sockfd, "<hr>\r\n");
	sendMessage(sockfd, "<p>CGI without X permission?</p>\r\n");
	
	return 1;
}

/* httpd */
int requestCGI(int sockfd, ReqHeader_t header) {
	int pid;
	char* fname = (char *)splitString(header.request, '/').back().c_str();
	
	sendMessage(sockfd, "HTTP/1.0 200 OK\r\n");
	
	// Execute
	pid = fork();
	
	if(pid < 0)
		cout << "[Client] Failed to fork\n";
	else if(pid == 0) {
		// Set environment
		setenv("QUERY_STRING",		header.query.c_str(), 1);
		setenv("CONTENT_LENGTH",	"0", 1);
		setenv("REQUEST_METHOD",	header.method.c_str(), 1);
		setenv("SCRIPT_NAME",		"SCRIPT_NAME", 1);
		setenv("REMOTE_HOST",		"Client Host Name", 1);
		setenv("REMOTE_ADDR",		"Client IP Address", 1);
		setenv("AUTH_TYPE",			"AUTH_TYPE", 1);
		setenv("REMOTE_USER",		"REMOTE_USER", 1);
		setenv("REMOTE_IDENT",		"REMOTE_IDENT", 1);
		
		// Redirect stdout & stderr
		dup2(sockfd, STDIN_FILENO);
		dup2(sockfd, STDOUT_FILENO);
		dup2(sockfd, STDERR_FILENO);
		
		if(execlp(header.request.c_str(), fname, NULL) < 0)
			return -1;
		exit(EXIT_SUCCESS);
	}
	
	waitpid(pid, NULL, 0);
	
	return pid;
}
int requestText(int sockfd, ReqHeader_t header) {
	char* line;
	size_t len = 0;
	FILE *fp = fopen(header.request.c_str(), "r");
	
	if(!fp) {
		httpd_respond500(sockfd);
		return 0;
	}
	
	sendMessage(sockfd, "HTTP/1.0 200 OK\r\n");
	sendMessage(sockfd, "Content-type: text/html\r\n");
	sendMessage(sockfd, "\r\n");
	
	while(getline(&line, &len, fp) != -1)
		sendMessage(sockfd, line);
		
	fclose(fp);
	
	return 1;
}

int hadleStatus(int sockfd, int status) {
	
	if(status == 400) {
		httpd_respond400(sockfd);
		return 0;
	} else if(status == 403) {
		httpd_respond403(sockfd);
		return 0;
	} else if(status == 404) {
		httpd_respond404(sockfd);
		return 0;
	} else if(status == 500) {
		httpd_respond500(sockfd);
		return 0;
	} 
	
	return 1;
}
int handleRequest(int sockfd, ReqHeader_t &header) {
	deque<string> query_tmp;
	
	if(httpd_recvHeader(sockfd, header) < 0)
		return 400;		// 400 (bad header)	
	if(access(header.request.c_str(), F_OK) != 0)
		return 404;		// 404 (file not found)
	if(isdir(header.request))
		return 404;		// 404 (directory counts as 404 by now)
	if(!endwithString(header.request, ".cgi") && access(header.request.c_str(), R_OK) != 0)
		return 403;		// 403 (cannot read file)
	if(endwithString(header.request, ".cgi") && access(header.request.c_str(), X_OK) != 0)
		return 500;		// 500 (cannot exec cgi)
		
	return 200;
}
int httpd(int sockfd) {
	ReqHeader_t header;
	
	// Initial
	chdir(getenv("HOME"));
	chdir("./public_html"); 
	
	// 0 = bad status, 1 = good status
	if(hadleStatus(sockfd, handleRequest(sockfd, header))) {
		if(_DEBUG)	cout << "[httpd] Request " << header.request << " ? " << header.query << endl;
		if(endwithString(header.request, ".cgi"))
			requestCGI(sockfd, header);
		else
			requestText(sockfd, header);
		//requestBinary();
	}
	
	// Close
	close(sockfd);
	
	if(_DEBUG)	cout << "[httpd] Exit \n";
	
	return EXIT_SUCCESS;
}

/* MAIN */
void reaper(int signal) {
	while(waitpid(0, NULL, WNOHANG) > 0);
}

int main(int argc, char *argv[]) {
	int sockfd, childfd, pid, server_port = DEFAULT_SERVER_PORT;
	unsigned int cli_len;
	struct sockaddr_in serv_addr, cli_addr;
		
	// Initial
	if(argc > 1)
		server_port = atoi(argv[1]);
	
	signal(SIGCHLD, reaper); 	// Reap slave
	
	// Socket
	if((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		cout << "[httpd] Cannot create socket\n";
		exit(EXIT_FAILURE);
	}
		
	// Reuse address
	int ture = 1;
	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &ture, sizeof(ture)) < 0) {  
        cout << "[httpd] Setsockopt failed\n";
        exit(EXIT_FAILURE);  
    }
	
	// Bind
	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr	= htonl(INADDR_ANY);
	serv_addr.sin_port			= htons(server_port);
	
	if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		cout << "[httpd] Cannot bind address\n";
		exit(EXIT_FAILURE);
	}
	
	// Listen
	if(listen(sockfd, CLIENTS_MAX) < 0) {
		cout << "[httpd] Failed to listen\n";
		exit(EXIT_FAILURE);
	}
	
	if(_DEBUG)	cout << "[httpd] Waiting for connections on " << server_port << "...\n";
	
	// Accept
	while(1) {		
		if((childfd = accept(sockfd, (struct sockaddr *) &cli_addr, &cli_len)) < 0) {
			cout << "[httpd] Failed to accept\n";
			continue;
		}
		
		if(_DEBUG)	cout << "[httpd] Connected\n";
		
		// Fork child socket
		pid = fork();
		
		if(pid < 0)
			cout << "[httpd] Failed to fork\n";
		else if(pid == 0) {
			close(sockfd);
			exit(httpd(childfd));
		} else {
			close(childfd);
		}
	}
	
	return 0;
}
