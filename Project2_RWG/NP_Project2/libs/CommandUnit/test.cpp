#include "CommandUnit.h"
#include <iostream>
#include <vector>
using namespace std;
int main(){
    /* test CommandUnit
	CommandUnit w=CommandUnit();
	cout<<"command_type is :"<< w.command_type<<endl;
	vector <CommandUnit> q;
	q.push_back(w);
	q.push_back(w);
	q.push_back(w);
	cout<<"the queue size is "<< q.size()<<endl;
	q.at(0).command_type=1;
	q.at(1).command_type=2;
	q.at(2).command_type=3;
	cout<<"q(0) is :"<< q.at(0).command_type<<endl;
	cout<<"q(1) is :"<< q.at(1).command_type<<endl;
	cout<<"q(2) is :"<< q.at(2).command_type<<endl;
    */
    string op="kerker qoo";
    vector <CommandUnit> CUV= Command_parse(op);
    for (int i=0; i<CUV.size();i++){
        cout<<"print order : ";
        for (int j=0; j<CUV.at(i).commands.size();j++){
            cout<< CUV.at(i).commands.at(j)<<" ";
        }
        cout<<endl;
    }
	return 0;
}
