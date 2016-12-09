make ;

on server:
./ras

on client:
nc 127.0.0.1 9081
(the client in testdata)
client 127.0.0.1 9081
client 127.0.0.1 9081 test/test1.txt




