#include <unistd.h>
#include <stdio.h>
#include <string>
#include <errno.h>
#include <sys/wait.h>
#include <iostream>
#include <sstream>//sstream for split
#include <vector> //vector
#include <string.h>// for strcpy/strncpy
#include <algorithm>
#include "libs/CommandParse.h"
using namespace std;

void repear( int killid){
	int status;
	printf("clean zoombie \n");
	while(waitpid(0,&status, WNOHANG)>0);
	//wnohang: 如果沒有已經結束的process 就return 0
	//有就 return pid
	//也就是一個收屍大師
}

void split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss;
    ss.str(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
}

int fileAccessUnderPATH(string fname, int mode) {
	vector<string> paths;
	string path = getenv("PATH");
	
	split(path, ':',paths);
	for(int i = 0; i < paths.size(); i ++) {
		string tmp = paths[i] + '/' + fname;
		if(access(tmp.c_str(), mode) == 0)
			return 1;	// Found file
	}
	return 0;
}


vector<CommandParse> commandParsing(char* command){
/* 
 * the logic is command(y) + state(x)
 * e.g. cat 123.txt |  => y=[cat,123.txt], state=1(|);
 * e.g. cat 123.txt |4 => y=[cat,123.txt], state=2(|*);
 * e.g. cat 123.txt >  => y=[cat,123.txt], state=3(>);
 * if command is empty and added state => skip
 * there may exits lots of bugs;
 */
    string s(command);
    char delim=' ';
    vector<string> elems;
    vector<CommandParse> commands_v;
    split(s, delim, elems);
    int len=elems.size();
    int command_count=0;
    CommandParse temp_elem=CommandParse();
    for(int i=0 ;i<len;i++)
    {
	if(elems.at(i).size()>0)
	{
		string temp_str= elems.at(i);
		if(temp_str[0]=='|')
		{
			if(temp_str.length()==1)
			{//1 is |
    				if(temp_elem.y.size()!=0)
				{
					temp_elem.x=1;
					command_count+=1;
					commands_v.push_back(temp_elem);
					temp_elem= CommandParse();
				}	
			}else if(temp_str.length()==2)
			{// 2 is |1 , |2 ,|3
    				if(temp_elem.y.size()!=0)
				{
					temp_elem.x=2;
				}
			}
		}else if(temp_str[0]=='>'){
			//3 is >
			temp_elem.x=3;
			if(temp_elem.y.size()!=0)
			{
				command_count+=1;
				commands_v.push_back(temp_elem);
				temp_elem= CommandParse();
			}
			//temp_elem = new CommandParse();	
		}else{
			temp_elem.y.push_back(temp_str);
		}
	}
    }
    if(temp_elem.y.size()!=0){
	    command_count+=1;
	    commands_v.push_back(temp_elem);
    }
    //cout<<"[debug] numbers of commands :"<< command_count<<endl;
    return commands_v;
}

void str_vector2array(vector<string> commands,char * argv[] ,int &order_size){
	int count=0;

	for(std::vector<string>::iterator it = commands.begin(); it != commands.end(); ++it)
	{
		char *pc = new char[(*it).size()+1];
		strcpy(pc, (*it).c_str());
		argv[count]=pc;
		count+=1;	
	}
	argv[count]=NULL;
	order_size=count;
	//cout<<"[debug] length : "<< count<<endl;
}

int createProcess(char * filename){
	int child_pid;
	int cur_cmd=0;
	int pipe_curr[2];
	pipe_curr[0]=0;
	pipe_curr[1]=0;
	int pipe_next[2];
	pipe_next[0]=0;
	pipe_next[1]=0;
	int previous_x;
	int order_size;

	string sysOrder("setenv,printenv,exit");
	vector<string> sysOrder_v;
	split(sysOrder,',',sysOrder_v);

	vector<string>::iterator itr;
	vector<CommandParse> commandParse_v= commandParsing(filename);
	while(cur_cmd<commandParse_v.size()){
		//check setenv/printenv/exit(now it's still the main process)
		char * argv[1501];
		argv[0]=NULL;
		str_vector2array((commandParse_v.at(cur_cmd)).y,argv,order_size);
		itr = find(sysOrder_v.begin(),sysOrder_v.end(),string(argv[0]));
		if(itr != sysOrder_v.end()){
			//cout<<"hihi"<<endl;
			//cout<<"[debug] sysOrder :"<<itr-sysOrder_v.begin()<<endl;
			switch(itr-sysOrder_v.begin()){
				case 0:
					if(order_size==3){
						setenv(argv[1],argv[2],1);
					}else{
						cout<<"usage : setenv variable value"<<endl;
					}
					break;
				case 1:
					if(order_size==2){
						cout << argv[1] << "=" << getenv(argv[1]) << endl;	
					}else{
						cout<<"usage : printenv variable "<<endl;
					}
					break;
				case 2:
					_exit(1);
			}	
			cur_cmd+=1;
		}else{ 
		//deal with pipe process
			if(commandParse_v.at(cur_cmd).x==1){
				if(pipe(pipe_curr) < 0) {
					cout << "[Client] Fail to create pipe\n";
					return -1;
				}
			}
			//it's time to fork;
			if((child_pid=fork())==1){
				printf("I fall my people\n");
				_exit(1);
			}else if(child_pid==0) {// children process
				if(previous_x==1 && pipe_next[0]>0){
					dup2(pipe_next[0],STDIN_FILENO);
				}
				if(commandParse_v.at(cur_cmd).x==1){
					dup2(pipe_curr[1],STDOUT_FILENO);
					dup2(pipe_curr[1],STDERR_FILENO);
				}else if(commandParse_v.at(cur_cmd).x==3){
					char * arg[1501];
					int order_size_next;
					str_vector2array((commandParse_v.at(cur_cmd+1)).y,arg,order_size_next);
					
					if(order_size_next!=2){
						cout<<"usage : order > 123.txt "<<endl;
						_exit(1);
					}
					FILE * pFile;
				//	cout<<"[debug] the vector"<<(commandParse_v.at(cur_cmd+1)).y[0]<<endl;
				//	cout<< "[debug] we ae writing to "<<arg[0]<<endl;
					pFile = fopen (arg[0],"w");
					dup2( fileno(pFile), STDOUT_FILENO);	
					dup2( fileno(pFile), STDERR_FILENO);	
				}
				string x(argv[0]);	
				if(fileAccessUnderPATH(x,F_OK)){
				//	cout<< "[debug] we ae runing "<<argv[0]<<endl;
					if(execv(argv[0],argv)==-1){
						cout<<"hey,hey ,something wrong"<<endl;
					};
					_exit(1);
				} else {
					cout<<"unknown command :["<<argv[0]<<"]."<<endl;
					_exit(1);
				}

			}else{// the parent process
				//cout<< "[debug] Parent process: I'm In"<<endl;
				if(pipe_curr[1]!=0){
					close(pipe_curr[1]);
				}
				// actually is the pipe of pervious stdin
				if(pipe_next[0]!=0){
					close(pipe_next[0]);
				}
				if(commandParse_v.at(cur_cmd).x==1){
					previous_x=1;
					pipe_next[0]=pipe_curr[0];
				}
				//cout<< "[debug] Paren process:  we are waiting child"<<endl;
				wait(&child_pid);
				//cout<< "[debug] Parent process: the child is come"<<endl;
				
				if(commandParse_v.at(cur_cmd).x==3){
					cur_cmd+=1;
				}
				cur_cmd+=1;
			}
		}
	}
}

int main(){
	int child_pid;
	char * line =NULL;
	size_t len =0;
	ssize_t read;
	setenv("PATH", "bin:.", 1); 
	
	printf("#");
	while((read=getline(&line,&len,stdin))!=-1){
		line[read-1]='\0';
		createProcess(line);
		printf("#");
	}
        return 0;
}


