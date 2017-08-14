# chat_server
Text-based Chat Server

Compiling chat_server:

chat_server needs xlib, xcb, and pthreads to compile. 
packages for each can be installed (in Ubuntu) with: 

`$sudo apt-get install libx11-dev libxcb1-dev libpthread-stubs0-dev`

makefile also needs pkg-config to include those libraries, install with:

`$sudo apt-get install pkg-config`

Running makefile: 
option 1: `$make `
produces necessary client and server executables, ignore compilation failures 
for other targets. 

option 2:

`$make client`

`$make server`

make each executable individually

Running server:
` ./server [port-number]`

Running client:
` ./client [server-ip] [port-number]`

How client works:
launches terminal window where you are prompted to 
log in to or create an account. from terminal you can decide to:

make a new group: 			`$new_group `

check the active users list: 		`$active_list`

accept an invitation to join a group: 	`$[Y/N]`

once you create a group or accept a group invitation, a gui window will launch
from a new thread. From there you can:

send a message

send a file:       ` /send_file [filename] `

send an invitation:	`/invite [username] `

download a file:	  `/download [filename] `

