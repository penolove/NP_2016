make ;

on server:
./ras

on client:
nc 127.0.0.1 9081
(the client in testdata)
client 127.0.0.1 9081
client 127.0.0.1 9081 test/test1.txt


## want to add in project2
### shm
sever.cpp will create a user_table(shm).
each client connect into server will get a position in  user_table (maybe in used).

### signal
used in tell

### fifo
>n <n need implemented in fifo (open tmp file in the linux)


