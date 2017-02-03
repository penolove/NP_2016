#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
using namespace std;

int main(){
        int child_pid;
        if((child_pid=fork())==1){
                //fial Fork
                printf("I fall my people\n");
                //exit();
        }else if(child_pid==0) {
                printf("I am a child with ,pid : %d \n",getpid());
        }else{
                printf("I am a parent with ,pid : %d \n",getpid());
                wait(&child_pid);
        }
        return 0;
}
