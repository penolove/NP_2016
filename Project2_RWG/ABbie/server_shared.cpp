#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h> // shared_memory
#include <sys/shm.h> // shared_memory
#include <sys/types.h>
#include <sys/socket.h> 
#include <sys/wait.h>
#include <unistd.h>  // access 
#include <signal.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <sys/stat.h>    
#include <fcntl.h>

#include <iostream>
#include <deque>
#include <string>

using namespace std;

#define DEFAULT_PORT 8080 
#define CLIENT_MAX 31  
#define MAX_LINE 15000
#define MAX_BUFFER 256
#define DEBUG 0 

typedef enum 
{
	NONE,
	REDIRECT,
	PIPE, 
	NUMBER_PIPE,
	ERROR_NUMBER_PIPE,
	READ_FIFO,
	WRITE_FIFO,
	RAS_CMD 
} cmd_Type ;

struct fd_table
{
	int num ;
	int pipe_fd[2];
};

struct user_table
{
	int id ;
	int sockFd ;
	pid_t pid ;
	char nickname[MAX_BUFFER] ;
	char address[30] ;
	bool used ;
	char message[CLIENT_MAX][MAX_BUFFER] ;
	int receiverId ;
	int senderId ;
	char fifoName[CLIENT_MAX][MAX_BUFFER] ;
	int readFifoFd[CLIENT_MAX];
};

struct user_table* gUsers = NULL ;	
int gUserNum = 0 ;
int gShmid ;
int gMyId ;
void logout(int);
void exit_message( const char* line )
{
	perror( line );
	exit( EXIT_FAILURE );
} // exit_message()

int read_line( int socketFd, char *line )
{
	int size ; 
	char temp ;

	if( read( socketFd, &temp, 1 ) < 1 ) return -1;
	for( size = 0 ; temp != '\n' ; size++ ) 
	{
		line[size] = temp ;
		if( read( socketFd, &temp, 1 ) < 1 ) return -1;
	} // for

	line[size] = '\0' ;
	return size ;	
} // read_message()

void send_message( int socketFd, string line )
{
	if( write( socketFd, line.c_str(), line.size() ) == -1 ) 
		exit_message( "[Server] Close a client because write error: " );	//fsync( socketFd );
} // send_message()

cmd_Type get_type( char* token )
{
	if( strcmp( token, "printenv" ) == 0 || strcmp( token, "setenv" ) == 0 || strcmp( token, "exit" ) == 0 ||
			strcmp( token, "who" ) == 0 || strcmp( token, "name" ) == 0 || 
			strcmp( token, "yell" ) == 0 || strcmp( token, "tell" ) == 0 )  return RAS_CMD ;
	else if( strcmp( token, "|" ) == 0 ) return PIPE ;
	else if( strcmp( token, ">" ) == 0 ) return REDIRECT ;
	else if( token[0] == '|' || token[0] == '!' || token[0] == '>' || token[0] == '<' ) 
	{ 
		int i ;
		for( i = 1 ; isdigit( token[i]) && i < strlen( token ) ; i++ );
		if( i == strlen( token ) && i > 1 ) 
		{
			return token[0] == '|' ? NUMBER_PIPE : 
				( token[0] == '!' ? ERROR_NUMBER_PIPE :
				  ( token[0] == '>' ? WRITE_FIFO : READ_FIFO ) ) ;
		} else return NONE ;	
	} else return NONE ;
} // get_type()

void split_line( deque< deque<char*> > &commands, char *line, string delim )
{
	deque<char*> one_cmd ;
	commands.clear();
	one_cmd.clear();
	cmd_Type tokenType = NONE ;

	int messageNum = -1 ;
	char *token = strtok( line, delim.c_str() );
	while( token )
	{
		tokenType = get_type( token );
		if( tokenType == NONE || tokenType == RAS_CMD || tokenType == READ_FIFO || tokenType == WRITE_FIFO ) 
		{
			one_cmd.push_back( token );
			messageNum = messageNum == -1 ? 
				( strcmp( token, "yell" ) == 0 ? 1 : strcmp( token, "tell" ) == 0 ? 2 : 0 ) 
				: messageNum ;
		} else 
		{
			if( !one_cmd.empty() ) commands.push_back( one_cmd );
			one_cmd.clear();
			one_cmd.push_back( token ); 
			commands.push_back( one_cmd );
			one_cmd.clear();
		} // else 

		if( messageNum == 1 ) token = strtok( NULL, "\r\n" );
		else token = strtok( NULL, delim.c_str() );

		messageNum--;
	} // while

	if ( !one_cmd.empty() ) commands.push_back( one_cmd );
} // split_line()

int check_syntax( int socketFd, deque< deque<char*> > commands )
{
	int size = commands.size();
	int error = false ;
	cmd_Type type_cmd, type_link ;
	for( int i = 0 ; i < size && !error ; i+=2 )
	{ 
		type_cmd = get_type( commands[i][0] ) ;
		type_link = i+1 < size ? get_type( commands[i+1][0] ) : NONE ; 	
		if( type_cmd == RAS_CMD && i != 0  ) error = true ;
		else if( type_cmd != RAS_CMD && type_cmd != NONE ) error = true ; 
		else if( ( type_link == ERROR_NUMBER_PIPE || type_link == NUMBER_PIPE ) && i+2 != size ) error = true ;
		else if( type_link == REDIRECT && ( i+3 != size || commands[i+2].size() != 1 ) ) error = true ;
		else if( type_link == PIPE && i+2 == size ) error = true ;
	} // for

	if( error ){
		send_message( socketFd, "Syntax Error!\n" );
		return false ;
	} else return true ;
} // check_syntax()

int check_cmd_exist( char* command, int mode )
{
	char* env = getenv( "PATH" ) ;
	char* path = (char*)malloc( MAX_BUFFER * sizeof(char) );
	char* tmp =  (char*)malloc( strlen( env ) * sizeof(char) );
	strcpy( tmp, env );
	tmp = strtok( tmp, ":" );
	while( tmp )
	{
		sprintf( path, "%s/%s", tmp, command );
		if( access( path, mode ) == 0 ) {
			free( path );
			free( tmp );
			return true ;
		} // if
		tmp = strtok( NULL, ":" );
	} // while

	free( path );
	free( tmp );
	return false ;
} // check_cmd_exist()

int check_pipe_exist( deque<fd_table> fd_records, int number, int &exist_fd_in, int &exist_fd_out )
{
	for( int i = 0 ; i < fd_records.size() ; i++ ){
		if( fd_records[i].num == number )
		{
			exist_fd_in = fd_records[i].pipe_fd[0];
			exist_fd_out = fd_records[i].pipe_fd[1];
			return true;
		} // if	
	} // for

	return false ;
} // check_pipe_exist()

int get_number( char* command )
{
	int size = strlen( command ) ;
	char* number = (char*)malloc( size * sizeof(char) ) ;
	strncpy( number, command+1, size ); // means skip the first char \'|\' or \'!\'
	number[size-1] = '\0' ;
	return atoi( number );
} // get_number()

char** parse_argvs( deque<char*> commands )
{
	char** argvs = (char**)malloc( ( commands.size()+1 ) * sizeof(char*) );
	int i ;
	for( i = 0 ; !commands.empty() && get_type( commands[0] ) != READ_FIFO && get_type( commands[0] ) != WRITE_FIFO ; commands.pop_front(), i++ )
	{
		argvs[i] = (char*)malloc( MAX_BUFFER * sizeof(char));
		strcpy( argvs[i], commands.front() );
	} // for

	argvs[i] = NULL ;
	return argvs ;
} // parse_argvs()

void fd_handler( deque<fd_table> &fd_records, int &fd_in )
{
	int temp = 0 ;
	int find = false ;
	if( DEBUG ) cout << "fd_table start:\n" ;
	for( int i = 0 ; i < fd_records.size() ; i++ )
	{
		if( DEBUG ) printf( "i:%d, num:%d, in:%d, out:%d\n", i, fd_records[i].num, fd_records[i].pipe_fd[0], fd_records[i].pipe_fd[1]  ) ;
		fd_records[i].num--;
		if( fd_records[i].num == 0 ) 
		{
			if( DEBUG ) printf( "[-] fd_handler close: %d\n", fd_records[i].pipe_fd[1] );
			close( fd_records[i].pipe_fd[1] );
			fd_in = fd_records[i].pipe_fd[0] ;
			temp = i ;
			find = true ;
		} // if
	} // for

	if( find ) fd_records.erase( fd_records.begin() + temp );
	return ;
} // fd_handler()

void killzombie( int sigNum )
{
	int status, id;
	while(waitpid(0, &status, WNOHANG) > 0) {
		if( ( id = WEXITSTATUS(status) ) > 0 )
		{
			for( int i = 1 ; i < 31 ; i++ )
			{
				if( !gUsers[i].used || i == id ) continue ;
				// close fifo to me 
				if( strcmp( gUsers[i].fifoName[id], "" ) != 0 ) 
				{
					unlink( gUsers[i].fifoName[id] );
					strcpy( gUsers[i].fifoName[id], "" );
				} // if
				// close fifo to other 
				if( strcmp( gUsers[id].fifoName[i], "" ) != 0 )
				{
					unlink( gUsers[id].fifoName[i] );
					strcpy( gUsers[id].fifoName[i], "" );
				} // if
			} // for
				
			close( gUsers[id].sockFd );
		} // if
	} // while
} // killzombie

void message_handler( int sigNum )
{	
	int senderId = gUsers[gMyId].senderId ;
	send_message( gUsers[gMyId].sockFd, gUsers[gMyId].message[senderId] );
	strcpy( gUsers[gMyId].message[senderId], "" );
} // message_handler()

void fifo_handler( int sigNum )
{
	int senderId = gUsers[gMyId].senderId ;
	if( ( gUsers[gMyId].readFifoFd[senderId] = open( gUsers[senderId].fifoName[gMyId], O_NONBLOCK | O_RDONLY ) ) < 0 )
		exit_message( "Error : open fifo for read error " );
	gUsers[gMyId].senderId = 0 ;
} // fifo_handler()

void clear( int sigNum )
{
	printf( "clear signal\n" );
	// detach and clear a shared memory
	if( shmctl( gShmid, IPC_RMID, (struct shmid_ds *)0 ) < 0 ) exit_message( "[Server] shmctl error!\n" );
	exit(EXIT_FAILURE);
} // clear 

void rascmd_who( int id )
{
	int receiverFd = gUsers[id].sockFd ;
	char message[MAX_BUFFER] ;
	strcpy( message, "<ID>\t<nickname>\t<IP/port>\t<indicate me>\n" );
	send_message( gUsers[id].sockFd, message );
	for( int i = 1 ; i < 31 ; i++ )
	{
		if( gUsers[i].used )
		{
			sprintf( message, "%d\t%s\t%s\t%s\n", 
					gUsers[i].id, gUsers[i].nickname, gUsers[i].address, i == id ? "<- me" : ""  );
			send_message( receiverFd, message );
		} // if
	} // for
} // cmd_who()

void ras_send( int senderId, char* message, int receiverId )
{ 
	/* broadcast */
	if( receiverId == 0 )
	{
		for( int id = 1 ; id < 31 ; id++ ) 
		{
			if( !gUsers[id].used ) continue ;
			strcpy( gUsers[id].message[senderId], message ) ;
			gUsers[id].senderId = senderId ; // zero means broadcast
			kill( gUsers[id].pid, SIGUSR1 );
		} // for

		/* wait to sned */
	    for( int id = 1 ; id < 31 ; id++ ) 
		{
			if( !gUsers[id].used ) continue ;
			while( strcmp( gUsers[id].message[senderId], "" ) != 0 ) ;
		} // for
	} else /* secret talk*/
	{
		strcpy( gUsers[receiverId].message[senderId], message ) ;
		gUsers[receiverId].senderId = senderId ; // zero means broadcast
		// call parent to talk to everybody
		kill( gUsers[receiverId].pid, SIGUSR1 ) ;
		
		/* wait to sned */
		while( strcmp( gUsers[receiverId].message[senderId], "" ) != 0 ) ;
	} // else 
} // ras_send()

void rascmd_yell( int id, char* message )
{
	char messageTmp[MAX_BUFFER] ;
	sprintf( messageTmp, "*** %s yelled ***: %s\n", gUsers[id].nickname, message ) ;
	ras_send( id, messageTmp, 0 );
} // cmd_yell()

void rascmd_name( int id, char name[MAX_BUFFER] )
{
	char message[MAX_BUFFER] ;
	/* check if name is exist */
	for( int i = 1 ; i < 31 ; i++ ) 
	{
		if( gUsers[i].used && strcmp ( name, gUsers[i].nickname ) == 0 )
		{
			sprintf( message, "*** User \'%s\' already exists. ***\n", name );
			send_message( gUsers[id].sockFd, message );
			return ;
		} //if
	} // for 
	
	sprintf( message, "*** User from %s is named '%s'. ***\n", gUsers[id].address, name );
	strcpy( gUsers[id].nickname, name ) ;

	// yell to everybody
	ras_send( id, message, 0 );
} // cmd_name()

void rascmd_tell( int id, int receiverId, char* message )
{
	if( !gUsers[receiverId].used || receiverId > 30 || receiverId < 1 ){
		char temp[MAX_BUFFER];
		sprintf( temp, "*** Error: user #%d does not exist yet. ***\n", receiverId );
		send_message( gUsers[id].sockFd, temp );
	} else 
	{
		char messageTmp[MAX_BUFFER];
		sprintf( messageTmp, "*** %s told you ***: %s\n", gUsers[id].nickname, message ) ;
		ras_send( id, messageTmp, receiverId );
	} // else 
} // cmd_tell()

int ras_cmd( int id, int socketFd, deque< deque<char*> > argvs )
{ // printenv, setenv, exit
	if( strcmp( argvs[0][0], "printenv" ) == 0 ){ 
		if( argvs.size() != 1 || argvs[0].size() != 2 ) 
			send_message( socketFd, "[Server] Usage: printenv [VARIABLE]\n" );
		else {
			char *message = (char*)malloc( MAX_BUFFER * sizeof(char)) ;
			sprintf( message, "%s=%s\n", argvs[0][1], getenv( argvs[0][1] ) );
			send_message( socketFd, message );
			free( message );
		} // else	
	} else if( strcmp( argvs[0][0], "setenv" ) == 0 ){
		if( argvs.size() != 1 || argvs[0].size() != 3 ) 
			send_message( socketFd, "[Server] Usage: printenv [VARIABLE] [VALUE]\n" );
		else if( setenv( argvs[0][1], argvs[0][2], 1 ) == -1 )
			send_message( socketFd, "[Server] Setenv error!\n" );
	} else if( strcmp( argvs[0][0], "exit" ) == 0 ) {
		if( argvs.size() != 1 )	send_message( socketFd, "[Server] Usage: exit\n" );
		else return 2 ;
	} else if ( strcmp( argvs[0][0], "who" ) == 0 ) {
		if( argvs.size() != 1 || argvs[0].size() != 1 ) 
			send_message( socketFd, "[Server] Usage: who\n" );
		else rascmd_who( id );
	} else if ( strcmp( argvs[0][0], "name" ) == 0 ) {
		if( argvs.size() != 1 || argvs[0].size() != 2 ) 
			send_message( socketFd, "[Server] Usage: name <name>\n" );
		else rascmd_name( id, argvs[0][1] );
	} else if ( strcmp( argvs[0][0], "yell" ) == 0 ) {
		if( argvs.size() != 1 || argvs[0].size() != 2 ) 
			send_message( socketFd, "[Server] Usage: yell <message>\n" );
		else rascmd_yell( id, argvs[0][1] );
	} else if ( strcmp( argvs[0][0], "tell" ) == 0 ) {
		if( argvs.size() != 1 || argvs[0].size() != 3 ) 
			send_message( socketFd, "[Server] Usage: tell <sockd> <message>\n" );
		else rascmd_tell( id, atoi( argvs[0][1] ), argvs[0][2] );
	} // else if	

	return 1;
} // ras_cmd()

void read_fifo( int receiverId, int senderId, int &fd_in )
{
	fd_in = gUsers[receiverId].readFifoFd[senderId] ;
} // read_fifo()

void write_fifo( int senderId, int receiverId, int &fd_out, int &fd_err )
{
	/* Call receiver to open first */
    gUsers[receiverId].senderId = senderId ;	
	kill( gUsers[receiverId].pid, SIGUSR2 );
	while( gUsers[receiverId].readFifoFd[senderId] == 0 ) ;

	/* Now you can write to fifo immediately */
	if( ( fd_out = fd_err  = open( gUsers[senderId].fifoName[receiverId], O_WRONLY ) ) < 0 )   
		exit_message( "[Write] Open fifo for write  error" );
} // write_fifo()

char* make_line( deque<char*> command )
{
	char *line = (char*)malloc( sizeof(char) * MAX_BUFFER ) ;
	strcpy( line, command[0] );
	for( command.pop_front() ; !command.empty() ; command.pop_front() )
	{
		strcat( line, " " );
		strcat( line, command[0] );
	} // for

	return line ;
} // make_line()

int cmd_fifo( int id, deque<char*> command, int &senderId, int &receiverId, int &fd_in  )
{
	char temp[MAX_BUFFER] ;
	cmd_Type link_type ;
	for( int i = 2 ; command.size() > 1 && i >= 1 ; i-- )
	{
		link_type = get_type( command[command.size()-i] );
		if( link_type == READ_FIFO ) 
		{
			senderId = get_number( command[command.size()-i] ) ;
			if( strcmp( gUsers[senderId].fifoName[id], "" ) == 0 ) // fifo not exist
			{
				sprintf( temp, "*** Error: the pipe #%d->#%d does not exist yet. ***\n", senderId, id );
				send_message( gUsers[id].sockFd, temp );
				return false ;
			} else 
			{
				sprintf( temp, "*** %s (#%d) just received from %s (#%d) by \'%s\' ***\n",
								gUsers[id].nickname, id, gUsers[senderId].nickname, senderId, make_line( command ) );
				ras_send( id, temp, 0 );
			} // else
		} else if( link_type == WRITE_FIFO )
		{
			receiverId = get_number( command[command.size()-i] ) ;
			if( !gUsers[receiverId].used )
			{ 
				sprintf( temp, "*** Error: user #%d does not exist yet. ***\n", receiverId );
				send_message( gUsers[id].sockFd, temp );
				return false ;
			} else if( strcmp( gUsers[id].fifoName[receiverId], "" ) == 0 ) // fifo not exist, create
			{ 
				sprintf( temp, "/tmp/fifo_from_%d_to_%d", id, receiverId );
				strcpy( gUsers[id].fifoName[receiverId], temp );
				if( mkfifo( temp , 0666 ) < 0 ) exit_message( "Fifo error\n" );
				sprintf( temp, "*** %s (#%d) just piped \'%s\' to %s (#%d) ***\n",
							gUsers[id].nickname, id, make_line( command ), gUsers[receiverId].nickname, receiverId );
				ras_send( id, temp, 0 );
			} else // fife exist, error
			{
				sprintf( temp, "*** Error: the pipe #%d->#%d already exists. ***\n", id, receiverId );
				send_message( gUsers[id].sockFd, temp );
				return false ;
			} // else 
		} // else if
	} // for

	return true ;
} // cmd_fifo()

int normal_cmd( int id,int childFd, deque< deque<char*> > commands, deque<fd_table> &fd_records )
{ 	
	// under path
	char** argvs ;
	int pipe_fd[2] = {0,0} ;
	int child_pid, fd_in = childFd, fd_out, fd_err, senderId, receiverId ;
	char *actionmes, temp[MAX_BUFFER] ;
	cmd_Type link_type ;
	deque<char*> command_temp ;

	fd_handler( fd_records, fd_in );
	while( !commands.empty() ){
		// initialize
		fd_out = fd_err = childFd ;
		senderId = receiverId = 0 ;

		// check if command is exist
		if( !check_cmd_exist( commands.front()[0], X_OK ) )
		{
			send_message( childFd, "Unknown command: [" ) ;
			send_message( childFd, commands.front()[0] ) ;
			send_message( childFd, "].\n" ) ;
			return 1;
		} // if

		// prepare arguments
		argvs = parse_argvs( commands.front() );

		// check fifo
		if( !cmd_fifo( id, commands.front(), senderId, receiverId, fd_in ) ) return 1 ;
		commands.pop_front(); // pop commands

		// check if next cmd need to be connected
		link_type = NONE ;
		if( !commands.empty() ) { // setting fd_out and fd_err
			link_type = get_type( commands.front()[0] ) ;
			if( link_type == PIPE )
			{
				if( pipe( pipe_fd ) < 0 ) 
					exit_message( "[Server] Can't pipe\n[Server] Close a client!\n" );
				else {
					fd_out = pipe_fd[1] ;
					if( DEBUG ) printf( "[+] NEW PIPE:%d %d\n", pipe_fd[0], pipe_fd[1] );
				} // else 
			} else if( link_type == NUMBER_PIPE || link_type == ERROR_NUMBER_PIPE )
			{
				fd_table temp ;
				temp.num = get_number( commands.front()[0] );;
				if( !check_pipe_exist( fd_records, temp.num, pipe_fd[0], pipe_fd[1] ) )
				{
					if( pipe( pipe_fd ) < 0 ) 
						exit_message( "[Server] Can't pipe\n[Server] Close a client!\n" );
					temp.pipe_fd[0] = pipe_fd[0] ;
					temp.pipe_fd[1] = pipe_fd[1] ;
					fd_records.push_back( temp );
					if( DEBUG ) printf( "[+] NEW PIPE:%d %d\n", pipe_fd[0], pipe_fd[1] );
				} // if

				fd_out = pipe_fd[1];
			} // else if 

			commands.pop_front(); // pop link 
			if( link_type == REDIRECT )
			{
				FILE * file_fd = fopen( commands.front()[0], "w" );
				fd_out = fileno( file_fd );
				commands.pop_front(); // pop file name 
			} // else if 

			fd_err = link_type == ERROR_NUMBER_PIPE ? pipe_fd[1] : childFd ;
		} // if

		if( senderId > 0 ) read_fifo( id, senderId, fd_in ) ;
		if( receiverId > 0 ) write_fifo( id, receiverId, fd_out, fd_err );
		// New Process
		if( ( child_pid = fork() ) == -1 ) {
			exit_message( "[Server] Can't fork\n[Server] Close a client!\n" );
		}else if( child_pid == 0 ){ // child
			if( DEBUG ) printf( "-\nlink_type:%d\nfd_in:%d, fd_out:%d, fd_err:%d\n"
					"childFd:%d, pipe[0]:%d, pipe[1]:%d \n-\n"
					, link_type, fd_in, fd_out, fd_err, childFd, pipe_fd[0], pipe_fd[1] );	

			dup2( fd_in, STDIN_FILENO );
			dup2( fd_out, STDOUT_FILENO );
			dup2( fd_err, STDERR_FILENO );
			close( fd_in );
			close( fd_out );
			close( fd_err );
			execvp( argvs[0], argvs );
			exit(EXIT_FAILURE);
		}else{ // parent
			// close pipe
			if( fd_in != childFd ) close( fd_in ) ;	
			if( link_type != NUMBER_PIPE && link_type != ERROR_NUMBER_PIPE && fd_out != childFd ) close( fd_out ) ;
			if( link_type != NUMBER_PIPE && link_type != ERROR_NUMBER_PIPE && fd_err != childFd ) close( fd_err ) ;
			
			if( link_type == PIPE ) fd_in = pipe_fd[0] ;

			wait(&child_pid);
			if( senderId != 0 ) 
			{
				unlink( gUsers[senderId].fifoName[id] );
				strcpy( gUsers[senderId].fifoName[id], "" );
			} // if
			// setting next run input 
		} // else 
	} // while

	return 1;
} // normal_cmd()

int execute_cmd( int id, int socketFd , deque< deque<char*> > commands, deque<fd_table> &fd_records )
{	
	// check syntax
	if( !check_syntax( socketFd, commands ) ) return 0; // syntax_error
	if( get_type( commands[0][0] ) == RAS_CMD ) return ras_cmd( id, socketFd, commands ) ;
	else return normal_cmd( id, socketFd, commands, fd_records );
} // execute_cmd()

void show_cmds( deque<deque<char*> > cmds )
{
	cout << "commands : " << cmds.size() << "\n" ;
	for( int i = 0 ; i < cmds.size() ; i++ )
	{
		for( int j = 0 ; j < cmds[i].size() ; j++ )
			printf( "%d:\"%s\"  " , j, cmds[i][j] ) ;
		cout << endl ;
	}
} // show_cmds()

int ras( int id, int socketFd ) 
{
	int size ;
	char line[MAX_LINE];
	deque< deque<char*> > commands;
	deque<fd_table> fd_records ;

	string messages = "****************************************\n"
		"** Welcome to the information server. **\n"
		"****************************************\n";
	send_message( socketFd, messages ) ;

	// let enerybody know 
	char message[MAX_BUFFER] ;
	sprintf( message, "*** User \'%s\' entered from %s. ***\n" , gUsers[id].nickname, gUsers[id].address  );
	ras_send( id, message, 0 );

	fd_records.clear();
	while( 1 )
	{
		if( DEBUG ) cout << "****************************************\n" ;	
		send_message( socketFd, "% " ) ; // write error
		if( read_line( socketFd, line ) < 0 ) return 0 ; // read error
		printf( "%d:%s\n", id,line );
		split_line( commands, line, " \n\r" ) ;
		//show_cmds( commands );
		if( commands.empty() ) continue ; // garbage command 
		if( execute_cmd( id, socketFd, commands, fd_records ) == 2 ) 
		{
			logout( id );
			printf( "[Server] Close a client! \n" );
			return id; // 'exit'
		} // if
	} // while
} // ras()


int connectSocket( int server_port )
{
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

int login( const int clientFd, const char address[25] )
{
	int id ;
	for( id = 1 ; gUsers[id].used && id < 31 ; id++ );
	/* initial */
	gUsers[id].id = id;
	gUsers[id].sockFd = clientFd ;
	strcpy( gUsers[id].nickname, "(no name)" );
	strcpy( gUsers[id].address, address ) ;
	gUsers[id].used = true ;
	for( int i = 1 ; i < 31 ; i++ ) 
		strcpy( gUsers[id].fifoName[i], "" );
	return id ;
} // login()

void logout( int id )
{
	char message[MAX_BUFFER] ;
	sprintf( message, "*** User \'%s\' left. ***\n" , gUsers[id].nickname  );
	ras_send( id, message, 0 );

	for( int i = 1 ; i < 31 ; i++ )
	{
		if( i == id || !gUsers[i].used ) continue ;
		// close fifo to me 
		if( strcmp( gUsers[i].fifoName[id], "" ) != 0 ) 
		{
			unlink( gUsers[i].fifoName[id] );
			strcpy( gUsers[i].fifoName[id], "" );
		} // if
		// close fifo to other 
		if( strcmp( gUsers[id].fifoName[i], "" ) != 0 )
		{
			unlink( gUsers[id].fifoName[i] );
			strcpy( gUsers[id].fifoName[i], "" );
		} // if
	} // for
	close( gUsers[id].sockFd );
	gUsers[id].used = false ;
	if( shmdt( gUsers ) < 0 ) exit_message( "[Server] shmdt error!\n" );
	//if( shmctl( gShmid, IPC_RMID, (struct shmid_ds *)0 ) < 0 ) exit_message( "[Server] shmctl error!\n" );
} // logout()

int main(int argc, char *argv[])
{
	int serverSocketFd, clientSocketFd, childPid, server_port = DEFAULT_PORT ;
	unsigned int client_len = sizeof(struct sockaddr);
	struct sockaddr_in clientAddr ;
	char ip[20], address[30];

	if( argc == 2 ) server_port  = atoi( argv[1] );
	else if( argc > 2 ) 
	{
		printf( "Usage %s [port]\n", argv[0] );
		exit(EXIT_FAILURE);  
	} // else if

	chdir( "./ras/" );
	setenv( "PATH", "bin:.", 1 ) ; 
	if( DEBUG )
	{	char buf[80];
		getcwd(buf, sizeof(buf));
		printf("current working directory : %s\n", buf);
	} // if

	// get a new shared memory
	if ( ( gShmid = shmget( IPC_PRIVATE, CLIENT_MAX * sizeof(user_table), IPC_CREAT | 0666 ) ) < 0 ) 
		exit_message("[Server] shmget error!\n");

	// attach a shared memory to the \"server\"
	if ( ( gUsers = (user_table*)shmat( gShmid, NULL, 0 ) ) == NULL )
		exit_message("[Server] shmat error!\n");

	// initial user table
	for( int id = 1 ; id < 31 ; id++ )
	{
		gUsers[id].used = false ;
		for( int i = 1 ; i < 31 ; i++ )
		{
			strcpy( gUsers[id].message[i], "" );
			gUsers[id].readFifoFd[i] = 0 ;
			strcpy( gUsers[id].fifoName[i], "" );
		} // for
	} // for

	serverSocketFd = connectSocket( server_port ); 
	printf( "Start to listen on port: %d\n", server_port );

	signal(SIGCHLD, killzombie ); 	        // zombie process
	signal(SIGINT, clear );			// cleat shared memory 
	signal(SIGUSR1, message_handler ); 	// message process
	signal(SIGUSR2, fifo_handler ); 	// message process
	// Connect with client 
	while( 1 )
	{
		if( ( clientSocketFd = accept( serverSocketFd , (struct sockaddr*)& clientAddr , &client_len ) ) < 0 )
			perror( "[Server] Accept error! " );

		inet_ntop( AF_INET, &clientAddr.sin_addr, ip, sizeof(ip) );
		sprintf( address, "%s/%d", ip, ntohs( clientAddr.sin_port ) );

		if( ( gMyId = login( clientSocketFd, address ) ) == -1 )
		{
			send_message( clientSocketFd, "[Server] please connect later...\n" );
			close( clientSocketFd );
			continue ;
		} // if

		if( ( childPid = fork() ) < 0 ) perror( "[Server] Fork error! " );
		else if( childPid == 0 ) // Child Process
		{
			gUsers[gMyId].pid = getpid();
			printf( "[Server] Connect a client( %s )( pid = %d )! \n", address, gUsers[gMyId].pid );
			// close other fds 
			close( serverSocketFd );
			exit(ras( gMyId, clientSocketFd )) ;
		} // else if
		else close( clientSocketFd ) ;
	} // while

	/* close socket */
	close( serverSocketFd );

	// detach and clear a shared memory
	if( shmdt( gUsers ) < 0 ) exit_message( "[Server] shmdt error!\n" );
	if( shmctl( gShmid, IPC_RMID, (struct shmid_ds *)0 ) < 0 ) exit_message( "[Server] shmctl error!\n" );
	exit(EXIT_SUCCESS);
} // main()
