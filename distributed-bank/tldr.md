cmd for mac
run from project root

(1)compile client:
clang++ client/client.cpp common/marshaller.cpp -o client/client 
(2)compile server:
clang++ server/server.cpp common/marshaller.cpp -o server/server

(3)run server
./server/server 2222 1 0.3   
(4)run client:
./client/client <ip-address> 2222 0.2

