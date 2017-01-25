# naive share memory , helloworld
a simple socket server , records how many connection used to connect to server

```
make
```

server:
```
./shm_server
```


client(you should connect more process)
```
nc 127.0.0.1 9081
```
and input numbers(any) , will return the counts(shm int).

