***SERVER :

gcc server.c -o s -lpthread
./s 127.0.0.1




***CLIENT :

gcc `pkg-config gtk+-2.0 --cflags` client.c -o client `pkg-config gtk+-2.0 --libs`
./client 127.0.0.1



***REQUIREMENT :
VLC MEDIA PLAYER
