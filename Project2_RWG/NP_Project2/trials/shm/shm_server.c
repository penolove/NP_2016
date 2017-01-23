#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
 
#define SHMSZ 17
 
int main()
{
    int shmid = 0, i = 0;
    key_t key;
    char *shm = NULL;
     
    // Segment key.
     
    key = 8888;
     
    // Create the segment.
     
    if( ( shmid = shmget( key, SHMSZ, IPC_CREAT | 0666 ) ) < 0 )
    {
        perror( "shmget" );
        exit(1);
    }
     
    // Attach the segment to the data space.
     
    if( ( shm = shmat( shmid, NULL, 0 ) ) == (char *)-1 )
    {
        perror( "shmat" );
        exit(1);
    }
     
    // Initialization.
     
    memset( shm, 0, SHMSZ );
     
    // Wait other processes to change the memory content.
    // *shm == 0, nothing.
    // *shm == 1, something changed.
    // *shm == 2, bye.
     
    while( *shm != 2 )
    {
        if( *shm == 1 )
        {
            for( i = 1 ; i < 17 ; i++ )
            {
                printf( "%02x ", *( shm + i ) );
            }
            printf( "\n" );
             
            memset( shm, 0, SHMSZ ); 
        }
         
        sleep(1);
    }
     
    return 0;
} 
