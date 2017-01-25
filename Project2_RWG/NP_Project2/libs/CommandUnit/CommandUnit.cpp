#include "CommandUnit.h"
#include <iostream>
#include <string>
#include <vector>
#include "../split.h"

using namespace std;
CommandUnit::CommandUnit(){
	command_type=1;
	n_pipe=0;
};


vector<CommandUnit> Command_parse(string command){
    vector<string> command_split;
    split(command,' ',command_split);
    //cout<< "vector size " << command_split.size()<<endl;
	vector <CommandUnit> CommandUnit_vec;
    CommandUnit CUtemp;
    for( int i =0 ;i<command_split.size();i++){
        string str_temp = command_split.at(i);
        if(str_temp.length()>0){
            switch(str_temp[0]){
                case '|':
                    cout<<"pipe"<<endl;
                    //remain to implement
                    continue;
                case '>':
                    cout<<"pipe2file"<<endl;
                    //remain to implement
                    continue;
                default :
                    CUtemp.commands.push_back(str_temp);
            }
        }
        if(i==command_split.size()-1){
            CUtemp.command_type=1;
            CommandUnit_vec.push_back(CUtemp);
            CUtemp = CommandUnit();
        }
    }

    return CommandUnit_vec;
};

