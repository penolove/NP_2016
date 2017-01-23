#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
 
#define SHMSZ 17
 
int main()
{
    int shmid = 0, i = 0, running = 1;
    key_t key;
    char *shm = NULL;
    char buffer[(SHMSZ - 1)];
     
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
     
    // Change the shared memory content.
    // *shm == 0, wait for change.
    // *shm == 1, not processed yet.
    // *shm == 2, bye.
     
    while( running )
    {
        if( *shm == 0 )
        {
            printf( "Please enter a msg (q for quit): " );
             
            memset( buffer, 0, ( SHMSZ - 1 ) );
            fgets( buffer, ( SHMSZ - 1 ), stdin );
         
            if( buffer[0] == 'q' )
            {
                *shm = 2;
                running = 0;
            }
            else
            {
                *shm = 1;
                memcpy( shm + 1, buffer, ( SHMSZ - 1 ) );
            }
        }
         
        sleep(1);
    }
     
    return 0;
}
