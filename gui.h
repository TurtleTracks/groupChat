#pragma once 

#include <xcb/xcb.h>

#define WIDTH 400
#define HEIGHT 500
#define MAX_MSG_SIZE 256
#define NUMBER_OF_MSGS 10

struct chatbox;

typedef struct chatbox chatbox_t;

void text_clear_helper(chatbox_t* chatbox, int height, int boxnum);

void text_draw_helper(chatbox_t* chatbox, int height, int boxnum);

void input_draw_helper(chatbox_t* chatbox, const char * buffer);

int setupWindow(chatbox_t* chatbox);

chatbox_t* chatbox_init(); 

int chatbox_read_input(chatbox_t* chatbox, char msg_to_send[MAX_MSG_SIZE]);

void roll_messages(chatbox_t* chatbox, char* payload, int len);
