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
                    if(str_temp.length()==1){
                        cout<<"[CommandUnit.Command_parse] pipe"<<endl;
                        CUtemp.command_type=2;
                        CUtemp.n_pipe=0;
                        CommandUnit_vec.push_back(CUtemp);
                        CUtemp = CommandUnit();
                    }else if (str_temp.length()==2){
                        CUtemp.command_type=2;
                        CUtemp.n_pipe=str_temp[1]-'0';
                        cout<<"[CommandUnit.Command_parse] N_pipe : N-" << CUtemp.n_pipe<<endl;
                        CommandUnit_vec.push_back(CUtemp);
                        CUtemp = CommandUnit();
                    }
                    continue;
                case '!':
                    if(str_temp.length()==1){
                        cout<<"[CommandUnit.Command_parse] err_pipe need't to implement"<<endl;
                    }else if (str_temp.length()==2){
                        CUtemp.command_type=3;
                        CUtemp.n_pipe=str_temp[1]-'0';
                        cout<<"[CommandUnit.Command_parse] err_N_pipe  : N-" << CUtemp.n_pipe<<endl;
                        CommandUnit_vec.push_back(CUtemp);
                        CUtemp = CommandUnit();
                    }
                    continue;
                case '>':
                    cout<<"[CommandUnit.Command_parse] pipe2file"<<endl;
                    CUtemp.command_type=4;
                    CUtemp.n_pipe=0;
                    CommandUnit_vec.push_back(CUtemp);
                    CUtemp = CommandUnit();
                    //remain to implement
                    continue;
                default :
                    CUtemp.commands.push_back(str_temp);
            }
        }
        if(i==command_split.size()-1 && CUtemp.commands.size()>0 ){
            CUtemp.command_type=1;
            CommandUnit_vec.push_back(CUtemp);
            CUtemp = CommandUnit();
        }
    }

    return CommandUnit_vec;
};

