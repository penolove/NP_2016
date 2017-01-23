## Linux shared memory

## Ref
http://neokentblog.blogspot.tw/2014/10/ipc-shared-memory-example.html



##shmget 4 major functions:

### shmget
// create share memory 
```
int shmget (key_t key, size_t size, int shmflg);
```
// size is the size of share memory to create
// shm flg: something like authorization
// return : succ :a valid shared memory identifier ,
//          fail : -1
```
#define SHMSZ 17
key_t key;
key=8888;
shmid = shmget( key, SHMSZ, IPC_CREAT | 0666 );
```
### shmat
//attaches the System V shared memory segment identified by
//       shmid to the address space of the calling process
```
void * shmat (int shm_id, const void *shm_addr, int shmflg);
```
//succ: shmat() returns the address of the attached shared memory segment
//fail: -1
```
shm = shmat( shmid, NULL, 0 ) );
```

### shmdt
//detaches the shared memory segment located at the address
```
int shmdt(const void * shmaddr);

ex.
gUsers = (user_table*)shmat( gShmid, NULL, 0 );
shmdt( gUsers );

```


## Example usage:
```
make
```
server
```
./shm_server
```

client
```
./shm_client
```



### ctrl

//control the shm segment , IPC_RMID:delete , IPC_STAT : get buf, IPC_SET : set buf
```
int shmctl (int shm_id, int command, struct shmid_ds *buf);

shmctl(shmid, IPC_RMID, 0);
```



