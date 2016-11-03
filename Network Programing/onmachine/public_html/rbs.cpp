#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <iostream>
#include <sstream>
#include <cstring>
#include <string>
#include <deque>
#include <map>
using namespace std;

#define RECV_BUFF_SIZE	1000

int web_addMessage(int, string, bool);
int web_addMessage(int, string);
int web_alert(string);

// Client Info Struct
struct client_t {
    int id;
	string host;
    int port;
	string file_name;
	FILE* fp;
	
	int sockfd;
};

/* Tools */
int sendMessage(int sockfd, string message) {
	if(write(sockfd, message.c_str(), strlen(message.c_str())) < 0)
		cout << "Write fail";
	
	return 1;
}
int sendMessage(int sockfd, char* message) {
	if(write(sockfd, message, strlen(message)) < 0)
		cout << "Write fail";
	
	return 1;
}
int recvMessage(int sockfd, string &result) {
	char recv_buff[RECV_BUFF_SIZE];
	int len = 0;
	
	// Clear buffer
	result.clear();
	
	if((len = read(sockfd, recv_buff, RECV_BUFF_SIZE-1)) <= 0)
		return 0;
		
	recv_buff[len] = 0;
	result = recv_buff;
	
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
void replaceAll(string& str, const string& from, const string& to) {
    if(from.empty())
        return;
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}
int containString(string target, string pattern) {
	size_t found = target.find(pattern);
	if(found != std::string::npos)
		return 1;
		
	return 0;
}
string int2string(int i) {
	string s;
	stringstream ss(s);
	ss << i;
	return ss.str();
}

/* Protocol */
int responeHeader() {
	const char *http200 =	"HTTP/1.1 200 OK\r\n"
							"Content-Type: text/html\r\n"
							"Cache-Control: max-age=0\r\n"
							"Connection: close\r\n";
	cout << http200;
	cout << "\r\n";
	return 1;
}
map<string, string> getQueries() {
	map<string, string> result;
	deque<string> var_string;
	string query = getenv("QUERY_STRING");
	//string query = "h1=nplinux1.cs.nctu.edu.tw&p1=8588&f1=test5.txt&h2=&p2=&f2=&h3=&p3=&f3=&h4=&p4=&f4=&h5=&p5=&f5=";
		
	var_string = splitString(query, '&');
	for(int i = 0; i < var_string.size(); i++) {
		map<string, string> pair;
		size_t pos;
		
		if(var_string[i] == "")
			break;
		
		pos = var_string[i].find("=");
		if(pos == string::npos)
			result[var_string[i].substr(0, pos)] = "";
		else
			result[var_string[i].substr(0, pos)] = var_string[i].substr(pos+1, string::npos);
	}
	
	return result;
}

/* Web */
int web_printWeb() {
	const char *html = "<!DOCTYPE html><html><head><meta http-equiv='Content-Type' content='text/html; charset=big5'><title>Network Programming Homework 3</title><style>body {background-color: #336699;font-family: Courier New;font-size: 10pt;color: #FFFF99;}table{width: 100%;}td{vertical-align: top;}span.bold{font-weight: bold;}</style><script>function addColumn(name) {len = document.getElementById('head').cells.length;document.getElementById('head').insertCell(len).textContent = name;document.getElementById('content').insertCell(len).id = 'm'+len;}function addMessage(id, text, bold) {bold = bold?1:0;if(bold){text = '<span class=\"bold\">'+text+'</span>';}else{text = '<span>'+text+'</span>';}document.getElementById(id).innerHTML += text;}</script></head><body><table border='1'><tr id='head'></tr><tr id='content'></tr></table></body></html>";
	cout << html;
	return 1;
}
int web_addColumn(string name) {
	cout << "<script>addColumn('" << name << "');</script>";
	return 1;
}
int web_addMessage(int id, string text, bool bold) {
	replaceAll(text, "'", "\\'");
	replaceAll(text, "<", "&lt;");
	replaceAll(text, ">", "&gt;");
	replaceAll(text, "\r", "");
	replaceAll(text, "\n", "<br>");
	cout << "<script>addMessage('m" << id << "', '" << text << "', " << bold << ");</script>\r\n";
	fflush(stdout);
	return 1;
}
int web_addMessage(int id, char* text, bool bold) {
	string text_str(text);
	return web_addMessage(id, text_str, bold);
}
int web_addMessage(int id, string text) {
	return web_addMessage(id, text, 0);
}
int web_alert(string text) {
	cout << "<script>alert('"<< text << "');</script>";
	fflush(stdout);
	return 1;
}

/* RBS */
deque<client_t> prepareClientInfo(map<string, string> queries) {
	deque<client_t> result;
	
	for(int i = 1; i <= 5; i++) {
		client_t info;
		
		if(queries.find("h"+int2string(i)) == queries.end() || queries["h"+int2string(i)] == "")
			break;
		info.id = i-1;
		info.host = queries["h"+int2string(i)];
		info.port = atoi(queries["p"+int2string(i)].c_str());
		info.file_name = queries["f"+int2string(i)];
		info.fp = fopen(info.file_name.c_str(), "r");
		
		if(!info.fp) {
			web_alert("h"+int2string(i)+": Cannot open file");
			continue;
		}
		
		result.push_back(info);
	}
	
	return result;
}
int rbs(client_t info) {
	int re_val;
	string recv_message;
	
	if((re_val = recvMessage(info.sockfd, recv_message)) <= 0)
		return 0;	
	web_addMessage(info.id, recv_message);
	
	if(containString(recv_message, "% ")) {
		char* line;
		size_t len = 0;
		
		if(getline(&line, &len, info.fp) == -1) {
			cout << "EOF<br>" << endl;
			return 0;
		}
		sendMessage(info.sockfd, line);
		web_addMessage(info.id, line, 1);
	}
	
	return 1;
}

/* Main */
int connectServer(string host, int port) {
	int sockfd;
	struct sockaddr_in client_sin;
	struct hostent *he = gethostbyname(host.c_str());
	
	sockfd = socket(PF_INET, SOCK_STREAM, 0);
	
	bzero(&client_sin, sizeof(client_sin));
	client_sin.sin_family = AF_INET;
	client_sin.sin_addr = *((struct in_addr *)he->h_addr); 
	client_sin.sin_port = htons(port);
	
	if(connect(sockfd, (struct sockaddr *)&client_sin, sizeof(client_sin)) == -1) {
		web_alert("Connection fail");
		exit(EXIT_FAILURE);
	}
	
	return sockfd;
}
int main(int argc, char *argv[]) {
	int max_fd;
	deque<client_t> client_infos;
	fd_set afds, rfds;
	
	// Prepare
	responeHeader();
	web_printWeb();
	
	client_infos = prepareClientInfo(getQueries());
	
	for(int i = 0; i < client_infos.size(); i++)
		web_addColumn(client_infos[i].host);
	// Connect
	FD_ZERO(&afds);
	for(int i = 0; i < client_infos.size(); i++) {
		client_infos[i].sockfd = connectServer(client_infos[i].host, client_infos[i].port);
		FD_SET(client_infos[i].sockfd, &afds);
		max_fd = (client_infos[i].sockfd > max_fd)? max_fd = client_infos[i].sockfd: max_fd;
	}
	
	while(1) {
		// Select
		memcpy(&rfds, &afds, sizeof(rfds));
		if(select(max_fd+1, &rfds, NULL, NULL, NULL) < 0) {
			web_alert("Failed to select");
			continue;
		}
		
		// Client
		for(int id = 0; id < client_infos.size(); id++) {
			if(FD_ISSET(client_infos[id].sockfd, &rfds)) {
				if(rbs(client_infos[id]) <= 0) {
					close(client_infos[id].sockfd);
					fclose(client_infos[id].fp);
					FD_CLR(client_infos[id].sockfd, &afds);
					client_infos.erase(client_infos.begin()+id);
				}
			}
		}
		
		if(client_infos.size() < 1)
			break;
	}
	
	return 0;
}