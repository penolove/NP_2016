#ifndef __NPIPE_ELEMENT_H__
#define __NPIPE_ELEMENT_H__
class  N_Pipe_element{
    public :
        N_Pipe_element(int number,int fdin ,int fdout){
            queue_remains=number;
            fd_out=fdout;
            fd_in=fdin;
        };
        int queue_remains;
        int fd_out;
        int fd_in; 
};
#endif
