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

//so annoying
vector<string> commandParsing(char * command ,char * argv[]){
    string s(command);
    char delim=' ';
    vector<string> elems;
    split(s, delim, elems);
    int len=elems.size();
    for(int i=0 ;i<len;i++){
   	char *pc = new char[elems.at(i).size()+1];
   	strcpy(pc, elems.at(i).c_str());
	argv[i]=pc;
	//cout<<i<<':'<<pc<<" ";
    }
    argv[len]=NULL;
    //cout<<endl;
    return elems;
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
		commandParsing(filename,argv);
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
		cout<<"step1"<<endl;
		//check if have this order
		//ls,cat,removetag,removetag0,number
	    	string s("ls,cat,removetag,removetag0,number");
	    	vector<string> elems;
	    	split(s, ',', elems);
		cout<<"step1.5"<<endl;
		string x(argv[0]);	
		cout<<"step2"<<endl;
		if(std::find(elems.begin(), elems.end(), x) != elems.end()) {
			sprintf (buffer, "./%s",argv[0]);
			//if(execlp(buffer,filename,NULL)==-1){
			if(execv(buffer,argv)==-1){
				cout<<"hey,hey ,something wrong"<<endl;
			};
			_exit(1);
		} else {
			cout<<"sir, there are no this command."<<endl;
			_exit(1);
		}
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


