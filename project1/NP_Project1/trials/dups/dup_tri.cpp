#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
using namespace std;
int main(){
        FILE * pFile;
        pFile = fopen ("trial.txt","w");
        dup2( fileno(pFile), STDOUT_FILENO);
        printf("kerker\n");
        printf("kerker\n");
        printf("kerker\n");
        printf("kerker\n");
        return 0;
}
