#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <deque>
using namespace std;

#define DEFAULT_SERVER_PORT	8758
#define CLIENTS_MAX	35

#define RECV_BUFF_SIZE	10020

struct rule_t {
	string action;
	string mode;
	string address;
};
struct header_t {
	int VN;
	int CD;
	unsigned int DST_PORT;
	string DST_IP;
	
	string USER_ID;
	string Domain_Name;
	
	bool permit;
};

/* Tool */
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

/* Network */
int send(const int sockfd, char* message) {
	return write(sockfd, message, strlen(message));
}
int send(const int sockfd, const string message) {
	return send(sockfd, message.c_str());
}
int recv(const int sockfd, string &result) {
	char recv_buff[RECV_BUFF_SIZE];
	int len;
	
	result.clear();
	if((len = read(sockfd, recv_buff, RECV_BUFF_SIZE-1)) > 0)
		result = recv_buff;
	
	return len;
}
int getSockPort(const int sockfd) {
	struct sockaddr_in sa;
	unsigned int sa_len;
	if(getsockname(sockfd, (struct sockaddr*) &sa, &sa_len) < 0) {
		cerr << "[Socks] Fail to get sock name\n";
		return -1;
	}
		
	return ntohs(sa.sin_port);
}
int bindnListen(const int server_port) {
	int sockfd;
	struct sockaddr_in serv_addr;
	
	// Socket
	if((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		cerr << "[Server] Cannot create socket\n";
		return -1;
	}
		
	// Reuse address
	int ture = 1;
	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &ture, sizeof(ture)) < 0) {  
        cerr << "[Server] Setsockopt failed\n";
        return -1;
    }
	
	// Bind
	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr	= htonl(INADDR_ANY);
	serv_addr.sin_port			= htons(server_port);
	
	if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		cerr << "[Server] Cannot bind address\n";
		return -1;
	}
	
	// Listen
	if(listen(sockfd, CLIENTS_MAX) < 0) {
		cerr << "[Server] Failed to listen\n";
		return -1;
	}
	
	return sockfd;
}
int connectServer(const string host, const int port) {
	int sockfd;
	struct sockaddr_in client_sin;
	struct hostent *he = gethostbyname(host.c_str());
	
	sockfd = socket(PF_INET, SOCK_STREAM, 0);
	
	bzero(&client_sin, sizeof(client_sin));
	client_sin.sin_family = AF_INET;
	client_sin.sin_addr = *((struct in_addr *)he->h_addr); 
	client_sin.sin_port = htons(port);
	
	if(connect(sockfd, (struct sockaddr *)&client_sin, sizeof(client_sin)) < 0) {
		cerr << "[Socks] Cannot connect to target\n";
		return -1;
	}
	
	return sockfd;
}

/* SOCKs */
void socks_dumpHeader(const header_t h) {
	string permit_s = (h.permit)? "Accept - ": "Deny - ";

	cout	<< "[Socks] " << permit_s
			<< "VN: " << h.VN 
			<< " CD: " << h.CD 
			<< " DST PORT: " << h.DST_PORT
			<< " DST IP: " << h.DST_IP 
			<< " USER ID: " << h.USER_ID
			<< " Domain Name: " << h.Domain_Name
			<< endl;
}
bool socks_checkHeader(const header_t header, deque<rule_t> rules) {
	// Check format
	if(header.VN != 4)
		return 0;
	else if (header.CD < 1 || header.CD > 2)
		return 0;
		
	// No rule
	if(rules.size() < 1)
		return 1;
	
	// Check rules	
	deque<string> h_ips, r_ips;
	for(int i = 0; i < rules.size(); i++) {
		rule_t r = rules[i];
		
		if((r.mode == "c" && header.CD != 1) || (r.mode == "b" && header.CD != 2))
			continue;
		if(r.action == "permit") {
			// Check ip
			h_ips = splitString(header.DST_IP, '.');
			r_ips = splitString(r.address, '.');
			
			if(h_ips.size() != 4 || r_ips.size() != 4)		continue;
			if(h_ips[0] != r_ips[0] && r_ips[0] != "*")		continue;
			if(h_ips[1] != r_ips[1] && r_ips[1] != "*")		continue;
			if(h_ips[2] != r_ips[2] && r_ips[2] != "*")		continue;
			if(h_ips[3] != r_ips[3] && r_ips[3] != "*")		continue;
			
			// Pass
			return 1;
		}
	}
		
	return 0;
}
int socks_recvHeader(const int sockfd, header_t &header) {
	unsigned char buffer[8];
	char ip[16];
	
	// Retrive header
	if(read(sockfd, buffer, 8) < 0)
		return -1;	
	
	snprintf(ip, 16, "%d.%d.%d.%d", (int)buffer[4], (int)buffer[5], (int)buffer[6], (int)buffer[7]);
	
	header.VN		= buffer[0];
	header.CD		= buffer[1];
	header.DST_PORT	= buffer[2] << 8 | buffer[3];
	header.DST_IP	= ip;
	
	if(recv(sockfd, header.USER_ID) < 0)
		return -1;		

	return 1;
}
int socks_replayHeader(const int sockfd, const header_t h, const int port) {
	unsigned char buffer[8];
	int ip[4], reply_cd;
	
	if(h.permit)	reply_cd = 90;	// Accept
	else			reply_cd = 91;	// Deny
	
	if(h.CD == 1) {			// Connect mode
		sscanf(h.DST_IP.c_str(), "%d.%d.%d.%d", &ip[0], &ip[1], &ip[2], &ip[3]);
		
		buffer[0] = 0;
		buffer[1] = reply_cd;
		buffer[2] = h.DST_PORT / 256;
		buffer[3] = h.DST_PORT % 256;
		buffer[4] = ip[0];
		buffer[5] = ip[1];
		buffer[6] = ip[2];
		buffer[7] = ip[3];
	} else if (h.CD == 2) {	// Bind mode
		buffer[0] = 0;
		buffer[1] = reply_cd;
		buffer[2] = port / 256;
		buffer[3] = port % 256;
		buffer[4] = 0;
		buffer[5] = 0;
		buffer[6] = 0;
		buffer[7] = 0;
	}
	
	write(sockfd, (char*)buffer, 8);
	
	return 0;
}
int socks_transfer(const int sendfd, const int recvfd) {
	char buffer[RECV_BUFF_SIZE];
	int len;
	
	// Read from sender
	if((len = read(sendfd, buffer, RECV_BUFF_SIZE-1)) <= 0)
		return -1;
	
	// Send to another socket
	if(write(recvfd, buffer, len) < 0)
		return -1;
	
	return 0;
}

int socks_redirectData(const int cntfd, const int tarfd) {
	int nfds;
	fd_set afds, rfds;
	char buffer[RECV_BUFF_SIZE];
	
	FD_ZERO(&afds);
	FD_SET(cntfd, &afds);
	FD_SET(tarfd, &afds);
	nfds = (cntfd > tarfd)? cntfd: tarfd;
		
	while(1) {
		int ret;
		
		FD_ZERO(&rfds);
		memcpy(&rfds, &afds, sizeof(rfds));
		
		if(select(nfds+1, &rfds, NULL, NULL, NULL) < 0) {
			cerr << "[Socks] Fail to select\n";
			return -1;
		}
		
		// Transfer message
		if(FD_ISSET(cntfd, &rfds)) {
			if(socks_transfer(cntfd, tarfd) < 0)
				break;
		} else if(FD_ISSET(tarfd, &rfds)) {
			if(socks_transfer(tarfd, cntfd) < 0)
				break;
		}
	}
	
	return 0;
}
int socks(const int sockfd, deque<rule_t> rules) {
	header_t header;
	int tarfd;
	
	// Retrive header
	if(socks_recvHeader(sockfd, header) < 0) {
		cerr << "[Socks] Fail to retrive header\n";
		return 0;
	}
	
	// Check header
	header.permit = socks_checkHeader(header, rules);
	
	socks_dumpHeader(header);
	if(!header.permit) 		return 0;
	
	// Action
	if(header.CD == 1) {
		// Reply
		socks_replayHeader(sockfd, header, 0);
		if((tarfd = connectServer(header.DST_IP, header.DST_PORT)) < 0) {
			close(sockfd);
			return 0;
		}
			
		socks_redirectData(sockfd, tarfd);
	} else if(header.CD == 2) {
		int tmpfd, port;
		// Bind and reply1
		if((tmpfd = bindnListen(INADDR_ANY)) < 0 || (port = getSockPort(tmpfd)) < 0) {
			close(sockfd);
			return 0;
		}
		socks_replayHeader(sockfd, header, port);
		
		// Accept and reply2
		if((tarfd = accept(tmpfd, NULL, NULL)) < 0) {
			close(sockfd);
			return 0;
		}
		socks_replayHeader(sockfd, header, port);
		close(tmpfd);
		
		socks_redirectData(sockfd, tarfd);
	}
	
	// Clean up
	close(sockfd);
	close(tarfd);
	
	cout << "[Socks] Leave\n";
	return 0;
}

/* MAIN */
void reaper(const int signal) {
	int status;
	while(waitpid(0, &status, WNOHANG) > 0);
}
void dumpRule(const deque<rule_t> rules) {
	cout << "[Server] Rule:\n";	
	for(int i = 0; i < rules.size(); i++) {
		rule_t r = rules[i];
		cout << "\tAction: " << r.action << "\tMode: " << r.mode << "\tAddress: " << r.address << endl;
	}
}
int readRule(deque<rule_t> &rules) {
	ifstream fin("socks.conf", ifstream::in); 
	rule_t rule;
		
	while(fin >> rule.action >> rule.mode >> rule.address)
		rules.push_back(rule);
	
	return 0;
}

int main(int argc, char *argv[]) {
	int server_fd, server_port = DEFAULT_SERVER_PORT;
	deque<rule_t> rules;
		
	// Initial
	if(argc > 1)
		server_port = atoi(argv[1]);
	
	signal(SIGCHLD, reaper); 			// Reap slave
	
	// Read rule
	readRule(rules);
	if(rules.size() > 0)
		dumpRule(rules);
	else
		cout << "[Server] Rule unavailable\n";
	
	// Create server
	if((server_fd = bindnListen(server_port)) < 0) {
		cerr << "[Server] Fail to create server\n";
		exit(0);
	}
		
	cout << "[Server] Waiting for connections on " << server_port << "...\n";
	
	while(1) {	
		int pid, child_fd;
		
		// Accept
		if((child_fd = accept(server_fd, NULL, NULL)) < 0) {
			cerr << "[Server] Failed to accept\n";
			continue;
		}
		
		// Fork
		pid = fork();
		
		if(pid == 0) {	
			close(server_fd);
			exit(socks(child_fd, rules));
		}
		
		close(child_fd);
	}

	return 0;
}
