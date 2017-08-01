CC=gcc
CFLAGS=-I. 
Deps = hashmap.h queue.h

UNAME := $(shell uname -s) 

#all: server client gui osxgui group
all: server client gui

server: chat-server.o hashmap.o queue.o util.o
	$(CC) -o server chat-server.o hashmap.o queue.o util.o -I. -lpthread

client: 
	gcc -Wall  chat-client.c gui.c hashmap.c queue.c util.c keysyms.c -o client -lpthread `pkg-config --cflags --libs xcb x11`

client_mac:
	gcc -Wall chat-client.c gui.c hashmap.c queue.c util.c keysyms.c -o client -lpthread -I/usr/X11R6/include -L/usr/X11R6/lib -lX11 -l XCB

gui:
	gcc -Wall -nostartfiles  gui.c keysyms.c -o gui `pkg-config --cflags --libs xcb x11`

osxgui:
	gcc -Wall gui.c keysyms.c -o gui -I/usr/X11R6/include -L/usr/X11R6/lib -lX11 -l XCB

clean:
	rm -fr .DS_Store *.tar.gz *.ps *.pdf *.o *.dSYM *~ client server group chat_server_socket gui active_name.txt *.file

