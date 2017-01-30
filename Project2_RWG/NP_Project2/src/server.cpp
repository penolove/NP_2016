#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <string>
#include <algorithm> // find

#include "../libs/ras.h"
#include "../libs/CommandUnit/CommandUnit.h"
#include "../libs/split.h"
#include "../libs/N_Pipe_element.h"

//shm library
#include <sys/ipc.h>
#include <sys/shm.h>
//network address
#include <arpa/inet.h>
//signal
#include <signal.h>

using namespace std;
#define _DEBUG          1

#define DEFAULT_SERVER_PORT     9081
#define CLIENTS_MAX     31

#define MAX_LINE 15000
#define MAX_BUFFER 256
#define CLIENT_MAX 31  

//structure that used to save users online
struct UserTable {
    pid_t pid;
    int user_id;
    bool isUsed;
    int sockFd;
    char address[30];
    char nickname[MAX_BUFFER];
    char messages[CLIENT_MAX][MAX_BUFFER];
    int msg_senderId;
};


//--------------global variables:------------- 
UserTable *gShmUT = NULL;
int gMyId;
vector<string> ras_cmds;
//--------------global variables:------------- 


// ========= InitEvir function block-start =========
void InitEnvir(){
    //====== ras command =====
    ras_cmds.push_back("who");
    ras_cmds.push_back("name");
    ras_cmds.push_back("tell");
    ras_cmds.push_back("yell");

    //====== enviroment order ======
    ras_cmds.push_back("setenv");
    ras_cmds.push_back("printenv");
}

void InitUserTable(UserTable &userTable){
    //call by reference and init the table
    userTable.user_id=1;
    userTable.isUsed =false;
    sprintf( userTable.address, "");
    sprintf( userTable.nickname, "");
}

// ========= copy function from true true block-strat =========

void exit_message( const char* line ){
    perror( line );
    exit( EXIT_FAILURE );
} // exit_message()

void send_message( int socketFd, string line ){
    if( write( socketFd, line.c_str(), line.size() ) == -1 ) 
        exit_message( "[Server] Close a client because write error: " );
} // send_message()



// ========= ras_series function block-start =================

void ras_who(){
    //function that need global variable gShmUT
    int childfd=gShmUT[gMyId].sockFd;
    char buffer [200]; 
    //send title to current childfd
    sprintf( buffer, "<ID>\t<nickname>\t<IP/port>\t<indicate me>\n" );
    send_message( gShmUT[gMyId].sockFd , (string)buffer );
    //check who is online
    for(int id = 1 ;id < 31 ; id++ ){
        if(gShmUT[id].isUsed){
            if(id==gMyId){
                sprintf (buffer, "%d\t%s\t%s\t%s\n",id,\
                        gShmUT[id].nickname,gShmUT[id].address,"<-me");
            }else{
                sprintf (buffer, "%d\t%s\t%s\t\n",id,\
                        gShmUT[id].nickname,gShmUT[id].address);
            }
            send_message( childfd, (string)buffer ) ;
        }
    };
}

void ras_name( string name_new ){
    sprintf(gShmUT[gMyId].nickname, "%s", name_new.c_str());
    if(_DEBUG) cout<< "[Client] : user "<< gMyId \
        << " is renamed to " << gShmUT[gMyId].nickname<<endl;

    /* need to implement brocast
     *  *** User from 140.113.215.62/1201 is named 'student5'. ***
     */
} 

void ras_tell(int target_id , string messages ){
    //check if online
    if(gShmUT[target_id].isUsed){
        sprintf(gShmUT[target_id].messages[gMyId], "%s\n", messages.c_str());
        gShmUT[target_id].msg_senderId=gMyId;
        //signal to that person
        kill( gShmUT[target_id].pid, SIGUSR1 );
        if(_DEBUG) cout<< "[Client]  user 0 "<< gMyId \
            << " sent messages to " << target_id<<endl;
    }else{
        char buffer[200];
        sprintf( buffer, "Hey, user %d is not online\n", target_id );
        send_message( gShmUT[gMyId].sockFd , (string)buffer );
    }
} 

void ras_broadcast(string messages){
    char buffer [MAX_BUFFER]; 
    if(_DEBUG) cout<<"[Client] user : "<< gMyId <<" ras_broadcast called"<<endl;
    for(int id = 1 ;id < 31 ; id++ ){
        if(gShmUT[id].isUsed){
            sprintf(buffer, "%s", messages.c_str());
            ras_tell(id, (string)buffer);
        }
    };
}

void ras_tell_MsgHandler(int sigNum){
    if(_DEBUG) cout<<"[Client] user : "<<gMyId <<" ras_tell_MsgHandler called"<<endl;
    int senderId = gShmUT[gMyId].msg_senderId ;
    send_message( gShmUT[gMyId].sockFd, gShmUT[gMyId].messages[senderId] );
    sprintf( gShmUT[gMyId].messages[senderId], "" );
} 

void execute_ras_cmds(vector<string> commands){
    int childfd=gShmUT[gMyId].sockFd;
    if(commands.at(0)=="who"){
        if(commands.size()==1){
            ras_who();
        }else{
            string messages = "who usage :\n"
                "who \n";
            send_message( childfd, messages ) ;
        }
    }else if (commands.at(0)=="name"){

        if(commands.size()==2){
            ras_name(commands.at(1));
        }else{
            string messages ="name usage: name <name> \n";
            send_message( childfd, messages ) ;
        }
    }else if (commands.at(0)=="tell"){

        if(commands.size()==3){
            char buffer [MAX_BUFFER]; 
            int target_id=atoi(commands.at(1).c_str());
            sprintf(buffer, "*** %s told you ** : %s", gShmUT[gMyId].nickname, commands.at(2).c_str());
            ras_tell(target_id,buffer);
        }else{
            string messages ="tell  <sockd> <message> \n";
            send_message( childfd, messages ) ;
        }
    }else if (commands.at(0)=="yell"){
        if(commands.size()==2){
            char buffer [MAX_BUFFER]; 
            sprintf(buffer, "*** %s yelled ** : %s",gShmUT[gMyId].nickname , commands.at(1).c_str());
            ras_broadcast((string)buffer);
        }else{
            string messages ="yell usage: yell <message> \n";
            send_message( childfd, messages ) ;
        }
    }else if (commands.at(0)=="setenv"){
        if(commands.size()==3){
            setenv(commands.at(1).c_str(),commands.at(2).c_str(),1);
        }else{
            char buffer [MAX_BUFFER]; 
            sprintf(buffer, "usage : setenv variable value");
            send_message( childfd, buffer ) ;
        }
    }else if (commands.at(0)=="printenv"){
        char buffer [MAX_BUFFER]; 
        if(commands.size()==2){
            sprintf(buffer, "%s = %s ",commands.at(1).c_str(),getenv(commands.at(1).c_str()));
            send_message( childfd, buffer ) ;
        }else{
            sprintf(buffer, "usage : printenv variable ");
            send_message( childfd, buffer ) ;
        }
    }
}


// ========= login / logout function block-start =================

int Login(int childfd,string ip_address){
    int id;
    //awesome iteration
    for( id = 1 ; gShmUT[id].isUsed && id < 31 ; id++ );

    if(id>0 && id <31){
        if(_DEBUG)      cout << "[Server] Current user id : " << id << endl;
        gShmUT[id].isUsed=true;
        gShmUT[id].sockFd=childfd;
        gShmUT[id].pid = getpid();
        sprintf(gShmUT[id].address, "%s", ip_address.c_str() );

        char buffer_nickname[MAX_BUFFER];
        sprintf(buffer_nickname, "student%d",id );
        sprintf(gShmUT[id].nickname, buffer_nickname );
        char buffer_address[MAX_BUFFER];
        sprintf(buffer_address, "User form \"%s\" is named '%s'.", ip_address.c_str(), buffer_nickname );
        ras_broadcast(buffer_address);

        //replace stdin, thus remains [server]stdout/stderr , clientfd.
        dup2(childfd,STDIN_FILENO);
        string messages = "****************************************\n"
            "** Welcome to the information server. **\n"
            "****************************************\n";
        send_message( childfd, messages ) ;
        return id;
    }else{
        return -1;
    }
}
bool Logout(){
    if(_DEBUG) cout << "[Server] user id : "<< gMyId <<" is logging out \n";
    InitUserTable(gShmUT[gMyId]);
}

// ========= normal_cmd function block-start  =================

int fileAccessUnderPATH(string fname, int mode, string& curr_path) {
    // check if file exists
    // mode F_OK (exist) ,X_OK (executable)
    vector<string> paths;
    string path = getenv("PATH");
    split(path, ':',paths);
    for(int i = 0; i < paths.size(); i ++) {
        string tmp = paths[i] + '/' + fname;
        if(access(tmp.c_str(), mode) == 0){
            curr_path=paths[i];
            return 1;    // Found file
        }
    }
    return 0;
}

void str_vec2char_arr(vector<string> commands,char * argv[] ){
    int count=0;
    for(std::vector<string>::iterator it = commands.begin(); it != commands.end(); ++it)
    {
        char *pc = new char[(*it).size()+1];
        sprintf(pc,"%s",(*it).c_str());
        argv[count]=pc;
        count+=1;    
    }
    argv[count]=NULL;
    //cout<<"[debug] length : "<< count<<endl;
}


void execute_normal_cmds(vector<string> commands){
    string execbin=commands.at(0);
    string curr_path;
    string exec_target;
    // create argv for execv 
    char * argv[1501];
    argv[0]=NULL;
    str_vec2char_arr(commands,argv);

    int child_pid=0;
    // check command exist
    if(fileAccessUnderPATH(execbin, F_OK, curr_path)){
        if(_DEBUG)  cout<<"[Client] command : "<<execbin << " exist"<<endl;
        exec_target=curr_path+"/"+execbin;
        if(execv(exec_target.c_str(),argv)==-1){
            if(_DEBUG)  cout<<"[Client] exec command : "<<execbin << " fail"<<endl;
        };
        _exit(1);
    }else{
        if(_DEBUG)  cout<<"[Client] command : "<<execbin << " not exist"<<endl;
    }
} 

int get_N_pipe_queue( vector<N_Pipe_element> &pipe_queue, int n_pipe){
    if(_DEBUG)  cout<<"[Client] get_N_pipe_queue "<<endl;
    for (int i=0; i<pipe_queue.size();i++){
        if(pipe_queue.at(i).queue_remains==n_pipe){
            return i;
        }
    }
    int pipe_next[2];
    pipe_next[0]=0;
    pipe_next[1]=0;
    if(pipe(pipe_next)<0){
        if(_DEBUG)  cout<<"[Client] somewhat fail to pipe"<<endl;
    }
    if(_DEBUG)  cout<<"[Client] create a pipe queue "<<endl;
    pipe_queue.push_back(N_Pipe_element(n_pipe,pipe_next[0],pipe_next[1]));
    return pipe_queue.size()-1;
}

int check_pipe_queue_zeros(vector<N_Pipe_element> pipe_queue){
    for (int i=0; i<pipe_queue.size();i++){
        if(pipe_queue.at(i).queue_remains==0){
            return i;
        }
    }
    return -1;
}

void Npipe_queue_handler(vector<N_Pipe_element> &pipe_queue){
        //decay the queue count;
        for (int i=0; i<pipe_queue.size();i++){
            if(_DEBUG)  cout<<"[Client] queue "<<i<<" queue_remains : " <<pipe_queue.at(i).queue_remains <<endl;
            pipe_queue.at(i).queue_remains-=1;
        }
        //clean the queue remains <0
        
        for (vector<N_Pipe_element>::iterator it=pipe_queue.begin(); it!=pipe_queue.end();) {
            if((*it).queue_remains<0) { 
                if(_DEBUG) cout<<"[Client] delete queue_remains : " <<(*it).queue_remains <<endl;
                it = pipe_queue.erase(it);
            }else { 
                ++it;
            }
        }
}

void ras_wrapper(){
    //i.e. succefully login
    char * line =NULL;
    size_t len =0;
    ssize_t read;
    int child_pid;
    //pipe_queue on this client process
    vector<N_Pipe_element> pipe_queue;

    //read stdin from childfd
    while((read=getline(&line,&len,stdin))!=-1){
        if (line[read - 1] == '\n') {
            //handle \n newline char
            line[read - 1] = '\0';
            --read;
        }

        for (int i=0;i<read;i++){
            //handle \r messages 
            if(line[i]=='\r'){
                line[i]=' ';
            }
        }

        string commands(line);
        vector <CommandUnit> CUV= Command_parse(commands);
        if(_DEBUG) cout << "[Client] Commands size : " << CUV.size()<<endl;

        //praent need to handle next pipe and previous pipe
        int pipe_prev[2];
        pipe_prev[0]=0; //replace pipe_in
        pipe_prev[1]=0; //replace pipe_out
        int pipe_next[2];
        pipe_next[0]=0;
        pipe_next[1]=0;

        // iterate the CommandUnit Vector 
        // e.g. cat xxx.txt | number
        // means CUV size is 2 , first is  "cat xxx.txt" , sec is "number"
        for (int i=0; i<CUV.size();i++){
            if(_DEBUG) cout << "[Client] runing action : " << CUV.at(i).commands.at(0)<<endl;
            string execbin=CUV.at(i).commands.at(0);
            switch(CUV.at(i).command_type){
                case 1:
                    if (find(ras_cmds.begin(), ras_cmds.end(), execbin.c_str()) != ras_cmds.end()){
                        if(_DEBUG)cout<<"[Client] execute : ras_cmds"<<endl;
                        execute_ras_cmds(CUV.at(i).commands); 
                    }else{
                        if(_DEBUG)cout<<"[Client] execute : normal_cmds"<<endl;
                        if((child_pid=fork())==1){
                            if(_DEBUG)cout<<"[Client.ras_wrapper] fork a process for normal_cmds"<<endl;
                            _exit(1);
                        }else if(child_pid==0) {// children process
                            
                            //childfd handdling
                            int check_result = check_pipe_queue_zeros(pipe_queue);
                            if(check_result == -1){
                                if(_DEBUG)cout<<"[Client.redirect pipe]"<<endl;
                                if(pipe_prev[0]!=0){
                                    dup2(pipe_prev[0],STDIN_FILENO);
                                }
                            }else{
                                if(_DEBUG)cout<<"[Client.redirect] check_result : "<<check_result<<endl;
                                int fdin = pipe_queue.at(check_result).fd_in;   
                                dup2(fdin,STDIN_FILENO);
                            }
                            
                            execute_normal_cmds(CUV.at(i).commands); 
                        }else{// the parent process
                            if(pipe_prev[0]!=0){
                                close(pipe_prev[0]);
                            }
                            if(pipe_prev[1]!=0){
                                close(pipe_prev[1]);
                            }
                            wait(child_pid);
                        }
                    }
                    break;
                case 2:
                    if (find(ras_cmds.begin(), ras_cmds.end(), execbin.c_str()) != ras_cmds.end()){
                        if(_DEBUG)  cout<<"[Client] ras_cmd needn't pipe"<<endl;
                    }else{
                        if(_DEBUG)  cout<<"[Client] execute : normal_cmds_pipe"<<endl;
                        //create pipe
                        if(pipe(pipe_next)<0) {
                            if(_DEBUG)  cout<<"[Client] somewhat fail to pipe"<<endl;
                        }
                        
                        int fdout;
                        //handles pipe or n_pipe 
                        if(CUV.at(i).n_pipe==0){
                            fdout=pipe_next[1];
                        }else{
                            int target_pipe=get_N_pipe_queue(pipe_queue,CUV.at(i).n_pipe);
                            fdout=pipe_queue.at(target_pipe).fd_out;
                        }
                        
                        if((child_pid=fork())==1) {
                            if(_DEBUG)  cout<<"[Client.ras_wrapper] fork a process for normal_cmds"<<endl;
                            _exit(1);
                        }else if(child_pid==0) {// children process
                            
                            //child_fd_handling
                            if(_DEBUG)  cout<<"[Client] checkpipe_queue"<<endl;
                            int check_result = check_pipe_queue_zeros(pipe_queue);
                            if(check_result == -1){
                                if(pipe_prev[0]!=0){
                                    dup2(pipe_prev[0],STDIN_FILENO);
                                }
                            }else{
                                int fdin = pipe_queue.at(check_result).fd_in;   
                                dup2(fdin,STDIN_FILENO);
                            }
                            
                            if(_DEBUG)  cout<<"[Client] n_pipe handle"<<endl;
                            
                            if(fdout!=0){
                                dup2(fdout,STDOUT_FILENO);
                            }
                            execute_normal_cmds(CUV.at(i).commands); 
                        }else {// the parent process
                            wait(child_pid);
                            // create a pipe(two) , close two.
                            if(pipe_prev[0]!=0){
                                close(pipe_prev[0]);
                            }
                            if(pipe_next[1]!=0){
                                close(pipe_next[1]);
                            }
                            pipe_prev[0]=pipe_next[0];
                            pipe_prev[1]=pipe_next[1];
                        }
                    }
                    break;
                case 3:
                    //normal_cmd pipe 2 file
                    break;
            }
        }
        if(_DEBUG)  cout<<"[Client] pipe_queue : " <<pipe_queue.size() <<endl;
        Npipe_queue_handler(pipe_queue);
    }
}


// ========= create server listening on server__port ===========
int connectSocket( int server_port ){
    struct sockaddr_in serverAddr ;
    int serverSocketFd ;

    /* Open a TCP socket */
    if( ( serverSocketFd = socket( AF_INET, SOCK_STREAM, 0 ) ) < 0 )
        exit_message( "[Server] Can't open socket! " );

    // Reuse address
    int ture = 1;
    if( setsockopt( serverSocketFd, SOL_SOCKET, SO_REUSEADDR, &ture, sizeof(ture)) < 0 ) 
        exit_message( "[Server] Setsockopt failed! " );

    /* Bind local address */
    serverAddr.sin_family = AF_INET ; /* address family: AF_INET */
    serverAddr.sin_port = htons( server_port );   /* port in network byte order */
    serverAddr.sin_addr.s_addr = htonl( INADDR_ANY );   /* internet address */
    if( ( bind( serverSocketFd, (struct sockaddr*)& serverAddr, sizeof( serverAddr )  ) ) < 0 )
        exit_message( "[Server] Cant't bind local address! " );

    /* Listen for client */
    if( ( listen( serverSocketFd, CLIENT_MAX ) ) < 0 )
        exit_message( "[Server] There are too many people in server! " );

    return serverSocketFd ;
} // connectSocket()


int main(int argc, char *argv[]) {
    InitEnvir();
    int sockfd, childfd, pid, server_port = DEFAULT_SERVER_PORT;
    unsigned int cli_len;
    struct sockaddr_in  cli_addr;

    //ras::helloWorld();

    //---------------SHM---------------------------------//
    int shmid = 0;

    // Create the segment.
    // using key: IPC_PRIVATE, and give size CLIENTS_MAX * sizeof(UT)
    if(( shmid = shmget( IPC_PRIVATE,  CLIENTS_MAX * sizeof(UserTable)\
                    , IPC_CREAT | 0666 ) ) < 0 ){
        perror( "shmget" );
        exit(1);
    }
    // Attach the segment to the data space.
    if( ( gShmUT = (UserTable *) shmat( shmid, NULL, 0 ) ) == NULL ){
        perror( "shmat" );
        exit(1);
    }
    //initialize user table
    for (int i=0;i<31;i++){
        InitUserTable(gShmUT[i]);
    }
    //------------------event handler---------------------//    
    signal(SIGUSR1, ras_tell_MsgHandler ); 	// message process


    //------------------server time----------------------//
    // start server listening on server_port
    sockfd=connectSocket( server_port );

    // Listen
    if(listen(sockfd, CLIENTS_MAX) < 0) {
        cout << "[Server] Failed to listen\n";
    }
    // Accept
    while(1) {
        if(_DEBUG) cout << "[Server] Waiting for connections on "\
            << server_port << "...\n";
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
            setenv("PATH", "bin:.", 1); 
            string commands;
            close(sockfd);
            if(_DEBUG)      cout << "[Server] Connected\n";

            //get current accept address
            char ip[20], address[30];
            inet_ntop( AF_INET, &cli_addr.sin_addr, ip, sizeof(ip) );
            sprintf( address, "%s/%d", ip, ntohs( cli_addr.sin_port ) );
            if(_DEBUG)  cout << "[Clinet] accept address : "<< address<<endl;
            // login to server,
            // - which will be assigned to one of gShmUT 
            // - replace stdin with childfd 
            gMyId=Login(childfd,(string)address );
            if(gMyId!=-1){
                //i.e. succefully login

                //call ras_wrapper which handles
                //- commands parse
                //- ras command execute
                //- normal command excute
                //- pipe execution
                ras_wrapper();

                Logout();
            }
            exit(0);
        }else{//parent process
            if(_DEBUG)      cout << "[Server] current accept fd : "<< childfd <<"\n";
            close(childfd);
        }
    }
    return 0;
}          





