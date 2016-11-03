#include <windows.h>
#include <deque>
#include <map>
#include <sstream>
#include <string>
using namespace std;

#include "resource.h"

#define RECV_BUFF_SIZE	1024
#define SERVER_PORT 7799
#define WM_HTTP_NOTIFY	(WM_USER + 1)
#define WM_RBS_NOTIFY	(WM_HTTP_NOTIFY + 1)

struct ReqHeader_t {
	bool finish;
	
	string version;
	string request;
	string method;
	string query;

	map<string, string> data;

	ReqHeader_t(): finish(0) {}
};
struct rbs_client_t {
	int id;
	string host;
	int port;
	string file_name;
	FILE* fp;

	int sockfd;
};
struct http_client_t{
	SOCKET sockfd;
	ReqHeader_t header;

	deque<rbs_client_t> rbs_clients;
};

int httpd_clear(http_client_t);

BOOL CALLBACK MainDlgProc(HWND, UINT, WPARAM, LPARAM);
int EditPrintf (HWND, TCHAR *, ...);
//=================================================================
//	Global Variables
//=================================================================
deque<http_client_t> http_clients;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	
	return DialogBox(hInstance, MAKEINTRESOURCE(ID_MAIN), NULL, MainDlgProc);
}

//=================================================================
//	Tools
//=================================================================
void debugMessage(const char* szFormat, ...) {
	char szBuff[1024];
	va_list arg;
	va_start(arg, szFormat);
	_vsnprintf_s(szBuff, sizeof(szBuff), szFormat, arg);
	va_end(arg);

	OutputDebugString(szBuff);
}

int sendMessage(const int sockfd, const char* message) {
	if (send(sockfd, message, strlen(message), 0) < 0)
		debugMessage("Write fail\n");
	return 1;
}
int sendMessage(const int sockfd, const string message) {
	return sendMessage(sockfd, message.c_str());
}
int recvMessage(const int sockfd, string &result) {
	char recv_buff[RECV_BUFF_SIZE];
	int len = 0;

	// Clear buffer
	result.clear();

	if ((len = recv(sockfd, recv_buff, RECV_BUFF_SIZE - 1, 0)) <= 0)
		return 0;

	recv_buff[len] = 0;
	result = recv_buff;

	return 1;
}
int recvLine(const int sockfd, string &result) {
	char recv_buff;

	// Clear buffer
	result.clear();

	while (1) {
		if (recv(sockfd, &recv_buff, 1, 0) <= 0)
			return 0;

		// Wait for CRLF
		if (recv_buff == '\r') {
			if (recv(sockfd, &recv_buff, 1, MSG_PEEK) > 0 && recv_buff == '\n')
				recv(sockfd, &recv_buff, 1, 0);
			return 1;
		}

		// Append char
		result += recv_buff;
	}

	return 1;
}
int getline(string &result, FILE* fp) {
	char c[4];
	result.clear();

	while (fgets(c, 2, fp) != NULL) {
		result += c;
		if (c[0] == '\n')
			return 1;
	}
	return 0;
}
deque<string> splitString(const string target, const char pattern) {
	deque<string> result;
	istringstream ss(target);
	string token;

	while (std::getline(ss, token, pattern)) {
		if (token == "") continue;
		result.push_back(token);
	}

	return result;
}
deque<string> divideString(const string target, const string pattern) {
	deque<string> result;
	size_t found = target.find(pattern);

	if (found != string::npos) {		// Found
		result.push_back(target.substr(0, found));
		result.push_back(target.substr(found + pattern.length(), string::npos));
	}
	else {						// Not Found
		result.push_back(target);
		result.push_back("");
	}

	return result;
}
void replaceAll(string& str, const string& from, const string& to) {
	if (from.empty())
		return;
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
	}
}
int containString(const string target, const string pattern) {
	size_t found = target.find(pattern);
	if (found != std::string::npos)
		return 1;

	return 0;
}
string int2string(const int i) {
	string s;
	stringstream ss(s);
	ss << i;
	return ss.str();
}
bool endwithString(const string &fullString, const string &ending) {
	if (fullString.length() >= ending.length())
		return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
	else
		return false;
}
BOOL fileExists(const string szPath) {
	DWORD dwAttrib = GetFileAttributes(szPath.c_str());
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}
bool isdir(const string& dirName_in) {
	DWORD ftyp = GetFileAttributesA(dirName_in.c_str());
	if (ftyp == INVALID_FILE_ATTRIBUTES)
		return false;  //something is wrong with your path!
	if (ftyp & FILE_ATTRIBUTE_DIRECTORY)
		return true;   // this is a directory!
	return false;    // this is not a directory!
}

int http_sock2id(const SOCKET sock) {
	for (UINT i = 0; i < http_clients.size(); i++) {
		if (sock == http_clients[i].sockfd)
			return i;
	}
	return -1;
}
int rbs_sock2pid(const SOCKET sock) {
	for (UINT i = 0; i < http_clients.size(); i++) {
		for (UINT j = 0; j < http_clients[i].rbs_clients.size(); j++) {
			if (sock == http_clients[i].rbs_clients[j].sockfd)
				return i;
		}
	}

	return -1;
}
int rbs_sock2id(const SOCKET sock) {
	for (UINT i = 0; i < http_clients.size(); i++) {
		for (UINT j = 0; j < http_clients[i].rbs_clients.size(); j++) {
			if (sock == http_clients[i].rbs_clients[j].sockfd)
				return j;
		}
	}
	
	return -1;
}

//=================================================================
//	Remote Batch Server
//=================================================================

/* Protocol */
int responeHeader(const SOCKET sockfd) {
	const char *http200 = "HTTP/1.1 200 OK\r\n"
		"Content-Type: text/html\r\n"
		"Cache-Control: max-age=0\r\n"
		"Connection: close\r\n";
	sendMessage(sockfd, http200);
	sendMessage(sockfd, "\r\n");
	return 1;
}
map<string, string> getQueries(const string query) {
	map<string, string> result;
	deque<string> var_string;
	//query = "h1=140.113.235.166&p1=8588&f1=t1.txt&h2=&p2=&f2=&h3=&p3=&f3=&h4=&p4=&f4=&h5=&p5=&f5=";

	var_string = splitString(query, '&');
	for (UINT i = 0; i < var_string.size(); i++) {
		map<string, string> pair;
		size_t pos;

		if (var_string[i] == "")
			break;

		pos = var_string[i].find("=");
		if (pos == string::npos)
			result[var_string[i].substr(0, pos)] = "";
		else
			result[var_string[i].substr(0, pos)] = var_string[i].substr(pos + 1, string::npos);
	}

	return result;
}

/* Web */
int web_printWeb(const SOCKET sockfd) {
	const char *html = "<!DOCTYPE html><html><head><meta http-equiv='Content-Type' content='text/html; charset=big5'><title>Network Programming Homework 3</title><style>body {background-color: #336699;font-family: Courier New;font-size: 10pt;color: #FFFF99;}table{width: 100%;}td{vertical-align: top;}span.bold{font-weight: bold;}</style><script>function addColumn(name) {len = document.getElementById('head').cells.length;document.getElementById('head').insertCell(len).textContent = name;document.getElementById('content').insertCell(len).id = 'm'+len;}function addMessage(id, text, bold) {bold = bold?1:0;if(bold){text = '<span class=\"bold\">'+text+'</span>';}else{text = '<span>'+text+'</span>';}document.getElementById(id).innerHTML += text;}</script></head><body><table border='1'><tr id='head'></tr><tr id='content'></tr></table></body></html>";
	sendMessage(sockfd, html);
	return 1;
}
int web_addColumn(const SOCKET sockfd, const string name) {
	sendMessage(sockfd, "<script>addColumn('" + name + "');</script>");
	return 1;
}
int web_addMessage(const SOCKET sockfd, const int id, string text, const bool bold) {
	replaceAll(text, "'", "\\'");
	replaceAll(text, "<", "&lt;");
	replaceAll(text, ">", "&gt;");
	replaceAll(text, "\r", "");
	replaceAll(text, "\n", "<br>");
	sendMessage(sockfd, "<script>addMessage('m" + int2string(id) + "', '" + text + "', " + int2string(bold) + ");</script>\r\n");
	fflush(stdout);
	return 1;
}
int web_addMessage(const SOCKET sockfd, const int id, char* text, const bool bold) {
	string text_str(text);
	return web_addMessage(sockfd, id, text_str, bold);
}
int web_addMessage(const SOCKET sockfd, const int id, string text) {
	return web_addMessage(sockfd, id, text, 0);
}
int web_alert(const SOCKET sockfd, const string text) {
	sendMessage(sockfd, "<script>console.error('" + text + "');</script>");
	fflush(stdout);
	return 1;
}

/* RBS */
SOCKET rbs_connectRAS(int http_sockfd, int id, string host, int port) {
	WSADATA wsaData;
	SOCKET sockfd;
	struct sockaddr_in client_sin;
	
	WSAStartup(MAKEWORD(2, 0), &wsaData);
	sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	memset(&client_sin, '\0', sizeof(&client_sin));
	client_sin.sin_family = AF_INET;
	if ((client_sin.sin_addr.s_addr = inet_addr(host.c_str())) == (unsigned long)INADDR_NONE){
		struct hostent *hostp;
		hostp = gethostbyname(host.c_str());
		if (hostp == (struct hostent *)NULL){
			//cout <<info[INFO_ADDR]<<" not found\n";
		}
		else
			memcpy(&client_sin.sin_addr, hostp->h_addr, sizeof(client_sin.sin_addr));
	}
	client_sin.sin_port = htons(port);

	if (connect(sockfd, (struct sockaddr *)&client_sin, sizeof(client_sin)) == SOCKET_ERROR) {
		int errcode = WSAGetLastError();
		debugMessage("[rbs] Connection fail - %d\n", errcode);
		if (errcode == 10060)
			web_alert(http_sockfd, "h" + int2string(id) + ": Server not found");
		else if (errcode == 10061)
			web_alert(http_sockfd, "h" + int2string(id) + ": Port not found");
		WSACleanup();

		return -1;
	}

	return sockfd;
}
deque<rbs_client_t> prepareClientInfo(HWND hwnd, SOCKET http_sockfd, map<string, string> queries) {
	deque<rbs_client_t> result;

	for (int i = 1; i <= 5; i++) {
		rbs_client_t info;

		if (queries.find("h" + int2string(i)) == queries.end() || queries["h" + int2string(i)] == "")
			break;
		info.id = i-1;
		info.host = queries["h" + int2string(i)];
		info.port = atoi(queries["p" + int2string(i)].c_str());
		info.file_name = queries["f" + int2string(i)];

		web_addColumn(http_sockfd, info.host);

		if (fopen_s(&info.fp, info.file_name.c_str(), "r")) {
			web_alert(http_sockfd, "h" + int2string(i) + ": Cannot open file");
			continue;
		}

		if ((info.sockfd = rbs_connectRAS(http_sockfd, i, info.host, info.port)) == -1) {
			//web_alert(http_sockfd, "h" + int2string(i) + ": Connection fail");
			continue;
		}

		WSAAsyncSelect(info.sockfd, hwnd, WM_RBS_NOTIFY, FD_CLOSE | FD_READ);
		result.push_back(info);
	}

	return result;
}
int rbs_connect(HWND hwnd, http_client_t &http_client) {
	// Prepare
	responeHeader(http_client.sockfd);
	web_printWeb(http_client.sockfd);

	http_client.rbs_clients = prepareClientInfo(hwnd, http_client.sockfd, getQueries(http_client.header.query));
		
	if (http_client.rbs_clients.size() < 1)
		httpd_clear(http_client);

	return 1;
}
int rbs_clear(int http_id, int rbs_id) {
	closesocket(http_clients[http_id].rbs_clients[rbs_id].sockfd);
	fclose(http_clients[http_id].rbs_clients[rbs_id].fp);
	http_clients[http_id].rbs_clients.erase(http_clients[http_id].rbs_clients.begin() + rbs_id);

	debugMessage("[rbs] Exit \n");
	return 1;
}
int rbs_main(http_client_t http_client, rbs_client_t rbs_client) {
	string recv_message;
	
	if (recvMessage(rbs_client.sockfd, recv_message) <= 0)
		return 0;
	web_addMessage(http_client.sockfd, rbs_client.id, recv_message);

	if (containString(recv_message, "% ")) {
		string line;
		if (getline(line, rbs_client.fp) == -1) {
			web_alert(rbs_client.sockfd, "EOF<br>\n");
			return 0;
		}
		sendMessage(rbs_client.sockfd, line);
		web_addMessage(http_client.sockfd, rbs_client.id, line, 1);
	}
	return 1;
}

/* Event */
SOCKET rbs_read(int http_id, int rbs_id) {
	http_client_t http_client = http_clients[http_id];
	rbs_client_t rbs_client = http_client.rbs_clients[rbs_id];
	
	if (rbs_main(http_client, rbs_client) <= 0)
		rbs_clear(http_id, rbs_id);
	
	if (http_clients[http_id].rbs_clients.size() < 1)
		httpd_clear(http_clients[http_id]);

	return 1;
}
int rbs_close(int http_id, int rbs_id) {
	rbs_clear(http_id, rbs_id);
	if (http_clients[http_id].rbs_clients.size() < 1)
		httpd_clear(http_clients[http_id]);

	return 1;
}

//=================================================================
//	HTTP Daemon
//=================================================================

/* Prtocol */
int httpd_recvHeader(const int sockfd, ReqHeader_t &header, const bool first) {
	string recv_message;
	deque<string> tmp;

	if (!recvLine(sockfd, recv_message))
		return -1;
	// Wait for end of header (blank line)
	if (recv_message == "")	{
		header.finish = 1;
		return 1;
	}

	if (first) {
		deque<string> tmp2;

		tmp = splitString(recv_message, ' ');
		if (tmp.size() != 3)	return -1;

		tmp2 = divideString("." + tmp[1], "?");

		header.method = tmp[0];
		header.request = tmp2[0];
		header.query = tmp2[1];
		header.version = tmp[2];
	}
	else {
		tmp = divideString(recv_message, ": ");
		if (tmp.size() != 2)	return -1;

		header.data[tmp[0]] = tmp[1];
	}

	return 1;
}

int httpd_respond400(const int sockfd) {
	sendMessage(sockfd, "HTTP/1.0 400 BAD REQUEST\r\n");
	sendMessage(sockfd, "Content-type: text/html\r\n");
	sendMessage(sockfd, "\r\n");
	sendMessage(sockfd, "<h1>400 - Bad Request</h1>\r\n");
	sendMessage(sockfd, "<hr>\r\n");
	sendMessage(sockfd, "<p>Well... What a bad request, huh?</p>\r\n");

	return 1;
}
int httpd_respond403(const int sockfd) {
	sendMessage(sockfd, "HTTP/1.0 403 FORBIDDEN\r\n");
	sendMessage(sockfd, "Content-type: text/html\r\n");
	sendMessage(sockfd, "\r\n");
	sendMessage(sockfd, "<h1>403 - Forbidden</h1>\r\n");
	sendMessage(sockfd, "<hr>\r\n");
	sendMessage(sockfd, "<p>U can't touch this.</p>\r\n");

	return 1;
}
int httpd_respond404(const int sockfd) {
	sendMessage(sockfd, "HTTP/1.0 404 NOT FOUND\r\n");
	sendMessage(sockfd, "Content-type: text/html\r\n");
	sendMessage(sockfd, "\r\n");
	sendMessage(sockfd, "<h1>404 - Not Found</h1>\r\n");
	sendMessage(sockfd, "<hr>\r\n");
	sendMessage(sockfd, "<p>You can't see me.<br>P.S. Directory counts as 404 by now.</p>\r\n");

	return 1;
}
int httpd_respond500(const int sockfd) {
	sendMessage(sockfd, "HTTP/1.0 500 Internal Server Error\r\n");
	sendMessage(sockfd, "Content-type: text/html\r\n");
	sendMessage(sockfd, "\r\n");
	sendMessage(sockfd, "<h1>500 - Internal Server Error</h1>\r\n");
	sendMessage(sockfd, "<hr>\r\n");
	sendMessage(sockfd, "<p>CGI without X permission?</p>\r\n");

	return 1;
}

/* httpd */
int httpd_requestCGI(http_client_t &client, HWND hwnd) {
	sendMessage(client.sockfd, "HTTP/1.0 200 OK\r\n");
	rbs_connect(hwnd, client);
	return 1;
}
int httpd_requestText(const int sockfd, ReqHeader_t header) {
	string line;
	FILE* fp;

	if (fopen_s(&fp, header.request.c_str(), "r")) {
		httpd_respond500(sockfd);
		return 0;
	}

	sendMessage(sockfd, "HTTP/1.0 200 OK\r\n");
	sendMessage(sockfd, "Content-type: text/html\r\n");
	sendMessage(sockfd, "\r\n");

	while (getline(line, fp))
		sendMessage(sockfd, line);
	
	fclose(fp);

	return 1;
}

int httpd_handleStatus(const int sockfd, const int status) {
	if (status == 400) {
		httpd_respond400(sockfd);
		return 0;
	}
	else if (status == 403) {
		httpd_respond403(sockfd);
		return 0;
	}
	else if (status == 404) {
		httpd_respond404(sockfd);
		return 0;
	}
	else if (status == 500) {
		httpd_respond500(sockfd);
		return 0;
	}

	return 1;
}
int httpd_CheckStatus(ReqHeader_t header) {
	
	// Special case
	if (header.request == "./hw3.cgi")
		return 200;
	
	if (!fileExists(header.request))
		return 404;		// 404 (file not found)
	if (isdir(header.request))
		return 404;		// 404 (directory counts as 404 by now)

	return 200;
}
int httpd_clear(http_client_t client) {
	closesocket(client.sockfd);
	http_clients.erase(http_clients.begin() + http_sock2id(client.sockfd));

	debugMessage("[httpd] Exit \n");
	return 1;
}
int httpd_main(http_client_t &client, HWND hwnd) {
	debugMessage("[httpd] Request %s ? %s\n", client.header.request.c_str(), client.header.query.c_str());

	// 0 = bad status, 1 = good status
	if (httpd_handleStatus(client.sockfd, httpd_CheckStatus(client.header))) {
		if (client.header.request == "./hw3.cgi")
			httpd_requestCGI(client, hwnd);
		else {
			httpd_requestText(client.sockfd, client.header);
			httpd_clear(client);
		}
	}

	return 1;
}

/* Event */
int http_initiate() {
	SetCurrentDirectoryA("./www/");
	return 1;
}
SOCKET http_listen(HWND hwnd, HWND hwndEdit) {
	WSADATA wsaData;
	SOCKET msock;
	static struct sockaddr_in sa;
	int err;


	WSAStartup(MAKEWORD(2, 0), &wsaData);

	//create master socket
	msock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (msock == INVALID_SOCKET) {
		EditPrintf(hwndEdit, TEXT("=== Error: create socket error ===\r\n"));
		WSACleanup();
		return TRUE;
	}

	err = WSAAsyncSelect(msock, hwnd, WM_HTTP_NOTIFY, FD_ACCEPT | FD_CLOSE | FD_READ);

	if (err == SOCKET_ERROR) {
		EditPrintf(hwndEdit, TEXT("=== Error: select error ===\r\n"));
		closesocket(msock);
		WSACleanup();
		return TRUE;
	}

	//fill the address info about server
	sa.sin_family = AF_INET;
	sa.sin_port = htons(SERVER_PORT);
	sa.sin_addr.s_addr = INADDR_ANY;

	//bind socket
	err = bind(msock, (LPSOCKADDR)&sa, sizeof(struct sockaddr));

	if (err == SOCKET_ERROR) {
		EditPrintf(hwndEdit, TEXT("=== Error: binding error ===\r\n"));
		WSACleanup();
		return FALSE;
	}

	err = listen(msock, 2);

	if (err == SOCKET_ERROR) {
		EditPrintf(hwndEdit, TEXT("=== Error: listen error ===\r\n"));
		WSACleanup();
		return FALSE;
	}
	else {
		EditPrintf(hwndEdit, TEXT("=== Server START ===\r\n"));
	}

	return msock;
}
SOCKET http_accept(const SOCKET msock) {
	http_client_t client;
	client.sockfd = accept(msock, NULL, NULL);
	http_clients.push_back(client);

	return client.sockfd;
}
int http_read(http_client_t &client) {
	string message;

	// Recive header
	if (!client.header.finish) {
		httpd_recvHeader(client.sockfd, client.header, (client.header.request == "") ? 1 : 0);
		
		if (client.header.finish)
			return 2;
	}
	else{
		// Dump junk
		string recv_message;
		recvLine(client.sockfd, recv_message);
	}
	return 1;
}

//=================================================================
//	Main
//=================================================================
BOOL CALLBACK MainDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam) {
	static HWND hwndEdit;
	static SOCKET http_msock;

	switch(Message) {
		case WM_INITDIALOG:
			hwndEdit = GetDlgItem(hwnd, IDC_RESULT);
			http_initiate();
			break;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
				case ID_LISTEN:
					http_msock = http_listen(hwnd, hwndEdit);
					break;

				case ID_EXIT:
					EndDialog(hwnd, 0);
					break;
			};
			break;

		case WM_CLOSE:
			EndDialog(hwnd, 0);
			break;

		case WM_HTTP_NOTIFY:
			switch( WSAGETSELECTEVENT(lParam) ) {
				case FD_ACCEPT:
					http_accept(http_msock);

					EditPrintf(hwndEdit, TEXT("=== Accept one new client, List size:%d ===\r\n"), http_clients.size());
					break;

				case FD_READ:
					if (http_sock2id(wParam) >= 0) {
						if(http_read(http_clients[http_sock2id(wParam)]) == 2)
							httpd_main(http_clients[http_sock2id(wParam)], hwnd);
					}
					break;

				case FD_CLOSE:
					if (http_sock2id(wParam) >= 0)
						httpd_clear(http_clients[http_sock2id(wParam)]);
					break;
			};
			break;

		case WM_RBS_NOTIFY:
			switch (WSAGETSELECTEVENT(lParam)) {
				case FD_READ:
					if (rbs_sock2id(wParam) >= 0) {
						rbs_read(rbs_sock2pid(wParam), rbs_sock2id(wParam));
					}
					
					break;

				case FD_CLOSE:
					if (rbs_sock2id(wParam) >= 0) {
						rbs_close(rbs_sock2pid(wParam), rbs_sock2id(wParam));
					}
					
					break;
			};
			break;
		
		default:
			return FALSE;


	};

	return TRUE;
}

int EditPrintf(HWND hwndEdit, TCHAR * szFormat, ...) {
	TCHAR   szBuffer[1024];
	va_list pArgList;

	va_start(pArgList, szFormat);
	wvsprintf(szBuffer, szFormat, pArgList);
	va_end(pArgList);

	SendMessage(hwndEdit, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
	SendMessage(hwndEdit, EM_REPLACESEL, FALSE, (LPARAM)szBuffer);
	SendMessage(hwndEdit, EM_SCROLLCARET, 0, 0);
	return SendMessage(hwndEdit, EM_GETLINECOUNT, 0, 0);
}