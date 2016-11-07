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
    cout<<"numbers of commands :"<< command_count<<endl;
    return commands_v;
}


int createProcess(char * filename){
	char buffer[1024];
	//sprintf (buffer, "./%s",filename);
	int child_pid;
	if((child_pid=fork())==1){
		printf("I fall my people\n");
		_exit(1);
	}else if(child_pid==0) {
		char * argv[1501];
		argv[0]=NULL;
		commandParsing(filename);
//		commandParsing(filename,argv);
		// char * argv[] = {"cat", "simple_shell.cpp",NULL};
		
		/*
 		-----debug-message----
		int i=0;
		while(argv[i]!=NULL){
			cout<<argv[i]<<" ";
			i+=1;
		}
		cout<<endl;
 		-----debug-message----
		*/
		//check if have this order
		//ls,cat,removetag,removetag0,number
		/*
	    	string s("ls,cat,removetag,removetag0,number,echo");
	    	vector<string> elems;
	    	split(s, ',', elems);
		if(argv[0]!=NULL){
			string x(argv[0]);	
			if(std::find(elems.begin(), elems.end(), x) != elems.end()) {
				sprintf (buffer, "./%s",argv[0]);
				if(execv(buffer,argv)==-1){
					cout<<"hey,hey ,something wrong"<<endl;
				};
				_exit(1);
			} else {
				cout<<"unknown command :["<<argv[0]<<"]."<<endl;
				_exit(1);
			}
		}*/
	}else{
		/*	
 		-----debug-message----
		cout<<"child_pid : "<<child_pid<<endl;
		printf("I am a parent with ,pid : %d \n",getpid());
 		-----debug-message----
		*/
	}
}


int main(){
	int child_pid;
	char * line =NULL;
	size_t len =0;
	ssize_t read;
	
	printf("#");
	while((read=getline(&line,&len,stdin))!=-1){
		line[read-1]='\0';
		createProcess(line);
		//printf("%d\n",child_pid);	
		wait(&child_pid);
		// if child process end sucessfully -> child_pid; 
		// if child process cursh -> -1;
		printf("#");
	}
        return 0;
}


