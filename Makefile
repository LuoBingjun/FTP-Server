server: server.c utils.c
	gcc -o server server.c utils.c -lpthread -Wall
.PHONY : clean
clean :
	-rm server