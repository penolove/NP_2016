#ifndef __COMMANDUNIT_H__
#define __COMMANDUNIT_H__

#include <vector> //vector
#include <string> //string
using namespace std;
class CommandUnit {
	public :
		CommandUnit();
        //currently consider 
        //1: normal_cmd/ras_cmd
        //---remain to implement---
        //3: normal_cmd stdout pipe 
        //4: normal_cmd stderr pipe
        //---remain to implement---
		int command_type;
		int n_pipe;
		vector<string> commands;
};

vector<CommandUnit> Command_parse(string command);

#endif
