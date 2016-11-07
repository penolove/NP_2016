#include "CommandParse.h"
#include <iostream>
#include <vector>
using namespace std;
int main(){
	CommandParse w=CommandParse();
	cout<<"x is :"<< w.x<<endl;
	vector <CommandParse> q;
	q.push_back(w);
	q.push_back(w);
	q.push_back(w);
	cout<<"the queue size is "<< q.size()<<endl;
	q.at(0).x=1;
	q.at(1).x=2;
	q.at(2).x=3;
	cout<<"q(0) is :"<< q.at(0).x<<endl;
	cout<<"q(1) is :"<< q.at(1).x<<endl;
	cout<<"q(2) is :"<< q.at(2).x<<endl;
	return 0;
}
