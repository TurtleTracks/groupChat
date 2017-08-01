#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gui.h"
#include <X11/keysym.h>
#include "xcb_keysyms.h"

// static buffer for clearing text 
const static char blankbuffer[] = {[0 ... MAX_MSG_SIZE-2] = ' ', [MAX_MSG_SIZE-1] = 0};
/* Chat Window Struct */
struct chatbox
{
        xcb_screen_iterator_t screen_iter;
        xcb_connection_t     *c;
        const xcb_setup_t    *setup;
        xcb_screen_t         *screen;
        xcb_generic_event_t  *e;
        xcb_generic_error_t  *error;
        xcb_void_cookie_t     cookie_window;
        xcb_void_cookie_t     cookie_map;
        xcb_window_t          window;
        uint32_t              mask;
        uint32_t              values[2];
        int              screen_number;
    unsigned char buffer_len;
        char input_buffer[MAX_MSG_SIZE];
        char msgs[NUMBER_OF_MSGS][MAX_MSG_SIZE];
};

static xcb_gc_t
gc_font_get (xcb_connection_t *c,
                         xcb_screen_t     *screen,
                         xcb_window_t      window,
                         const char       *font_name)
{
        uint32_t             value_list[3];
        xcb_void_cookie_t    cookie_font;
        xcb_void_cookie_t    cookie_gc;
        xcb_generic_error_t *error;
        xcb_font_t           font;
        xcb_gcontext_t       gc;
        uint32_t             mask;

        font = xcb_generate_id (c);
        cookie_font = xcb_open_font_checked (c, font,
                                                                           strlen (font_name),
                                                                           font_name);

        error = xcb_request_check (c, cookie_font);
        if (error) {
                fprintf (stderr, "ERROR: can't open font : %d\n", error->error_code);
                xcb_disconnect (c);
                return -1;
        }

        gc = xcb_generate_id (c);
        mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT;
        value_list[0] = screen->black_pixel;
        value_list[1] = screen->white_pixel;
        value_list[2] = font;
        cookie_gc = xcb_create_gc_checked (c, gc, window, mask, value_list);
        error = xcb_request_check (c, cookie_gc);
        if (error) {
                fprintf (stderr, "ERROR: can't create gc : %d\n", error->error_code);
                xcb_disconnect (c);
                exit (-1);
        }

        cookie_font = xcb_close_font_checked (c, font);
        error = xcb_request_check (c, cookie_font);
        if (error) {
                fprintf (stderr, "ERROR: can't close font : %d\n", error->error_code);
                xcb_disconnect (c);
                exit (-1);
        }

        return gc;
}

static void
text_draw (xcb_connection_t *c,
                   xcb_screen_t     *screen,
                   xcb_window_t      window,
                   int16_t           x1,
                   int16_t           y1,
                   const char       *label)
{
        xcb_void_cookie_t    cookie_gc;
        xcb_void_cookie_t    cookie_text;
        xcb_generic_error_t *error;
        xcb_gcontext_t       gc;
        uint8_t              length;

        length = strlen (label);

        gc = gc_font_get(c, screen, window, "7x13");

        cookie_text = xcb_image_text_8_checked (c, length, window, gc,
                                                                                        x1,
                                                                                        y1, label);
        error = xcb_request_check (c, cookie_text);
        if (error) 
        {
                fprintf (stderr, "ERROR: can't paste text : %d\n", error->error_code);
                xcb_disconnect (c);
                exit (-1);
        }

        cookie_gc = xcb_free_gc (c, gc);
        error = xcb_request_check (c, cookie_gc);
        if (error) {
                fprintf (stderr, "ERROR: can't free gc : %d\n", error->error_code);
                xcb_disconnect (c);
                exit (-1);
        }
}

void text_clear_helper(chatbox_t* chatbox, int height, int boxnum)
{
	text_draw(chatbox->c, chatbox->screen, chatbox->window, 10, height, blankbuffer);
}
void text_draw_helper(chatbox_t* chatbox, int height, int boxnum)
{
    text_draw(chatbox->c, chatbox->screen, chatbox->window, 10, height, chatbox->msgs[boxnum]); 
}

void input_draw_helper(chatbox_t* chatbox, const char * buffer)
{
	text_draw(chatbox->c, chatbox->screen, chatbox->window, 10, HEIGHT - 25, buffer); 
}

int setupWindow(chatbox_t* chatbox)
{
        /* getting the connection */
        chatbox->c = xcb_connect (NULL, &chatbox->screen_number);

        if (!chatbox->c) 
        {
                fprintf (stderr, "ERROR: can't connect to an X server\n");
                return -1;
        }

        /* getting the current screen */
        chatbox->setup = xcb_get_setup (chatbox->c);

        chatbox->screen = NULL;
        chatbox->screen_iter = xcb_setup_roots_iterator (chatbox->setup);
        for (; chatbox->screen_iter.rem != 0; 
        --chatbox->screen_number, xcb_screen_next (&chatbox->screen_iter))
                if (chatbox->screen_number == 0)
                {
                        chatbox->screen = chatbox->screen_iter.data;
                        break;
                }
        if (!chatbox->screen) 
        {
                fprintf (stderr, "ERROR: can't get the current screen\n");
                xcb_disconnect (chatbox->c);
                return -1;
        }

        /* creating the window */
        chatbox->window = xcb_generate_id (chatbox->c);
        chatbox->mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
        chatbox->values[0] = chatbox->screen->white_pixel;
        chatbox->values[1] =
                XCB_EVENT_MASK_KEY_RELEASE |
                XCB_EVENT_MASK_BUTTON_PRESS |
                XCB_EVENT_MASK_EXPOSURE |
                XCB_EVENT_MASK_POINTER_MOTION;
        chatbox->cookie_window = xcb_create_window_checked (chatbox->c, chatbox->screen->root_depth,
                                                            chatbox->window, chatbox->screen->root,
                                                            20, 200, WIDTH, HEIGHT, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                                            chatbox->screen->root_visual, chatbox->mask, chatbox->values);
        chatbox->cookie_map = xcb_map_window_checked (chatbox->c, chatbox->window);

        /* error managing */
        chatbox->error = xcb_request_check (chatbox->c, chatbox->cookie_window);
        if (chatbox->error) 
        {
                fprintf (stderr, "ERROR: can't create window : %d\n", chatbox->error->error_code);
                xcb_disconnect (chatbox->c);
                return -1;
        }
        chatbox->error = xcb_request_check (chatbox->c, chatbox->cookie_map);
        if (chatbox->error) 
        {
                fprintf (stderr, "ERROR: can't map window : %d\n", chatbox->error->error_code);
                xcb_disconnect (chatbox->c);
                return -1;
        }
    return 0;
        
}

chatbox_t* chatbox_init ()
{
    chatbox_t *chatbox = (chatbox_t*)malloc(sizeof(chatbox_t));
    memset(chatbox, 0, sizeof(chatbox_t));
    setupWindow(chatbox); 
        
    xcb_flush(chatbox->c);
    return chatbox;
}

int chatbox_read_input(chatbox_t* chatbox, char msg_to_send[MAX_MSG_SIZE])
{
	int msg_size = 0;
	chatbox->e = xcb_poll_for_event(chatbox->c);

	char *text;

	text = "Press ESC key to exit...";
	if (chatbox->e) 
	{
		switch (chatbox->e->response_type & ~0x80) 
		{
			case XCB_EXPOSE: 
			{
                                text_draw(chatbox->c, chatbox->screen, chatbox->window, 10, HEIGHT - 10, text);
				text_draw(chatbox->c, chatbox->screen, chatbox->window, 10, HEIGHT - 25, chatbox->input_buffer);
				break;
			}
			case XCB_KEY_RELEASE: 
			{
				xcb_key_release_event_t *ev;

				ev = (xcb_key_release_event_t *)chatbox->e;
				char ascii;
				xcb_key_symbols_t* s = xcb_key_symbols_alloc(chatbox->c);
				xcb_keysym_t keysymbol = xcb_key_release_lookup_keysym(s, ev, ev->state & 1 || ev->state & 2);

				switch (keysymbol)
				{ 
					case XK_Escape:
						free (chatbox->e);
						xcb_disconnect (chatbox->c);
						return 0;
					case XK_BackSpace:
						if(chatbox->buffer_len > 0)
							chatbox->input_buffer[--chatbox->buffer_len] = 0;
						input_draw_helper(chatbox, blankbuffer);
						input_draw_helper(chatbox, chatbox->input_buffer);
						break;
					case XK_Return:
						msg_size = chatbox->buffer_len;
                                                strncpy(msg_to_send, chatbox->input_buffer, chatbox->buffer_len);
						memset(chatbox->input_buffer, 0, sizeof(chatbox->input_buffer));
						input_draw_helper(chatbox, blankbuffer);
						chatbox->buffer_len = 0; 
						return msg_size;
					default:
						ascii = (char)keysymbol;
						if(chatbox->buffer_len < 255 && ascii > 0) 
						{
							chatbox->input_buffer[chatbox->buffer_len++] = ascii;
							input_draw_helper(chatbox, chatbox->input_buffer);
						}
						chatbox->input_buffer[chatbox->buffer_len] = '\0';
						break;
				}
			}
		}
		free (chatbox->e);
	}
  return 0;
}

void roll_messages(chatbox_t* chatbox, char* payload, int len)
{
    int i;
    for (i=NUMBER_OF_MSGS-1; i >= 1; --i)
    { 		
        memcpy(chatbox->msgs[i], chatbox->msgs[i-1], MAX_MSG_SIZE); 
    }
	memset(chatbox->msgs[0], 0, MAX_MSG_SIZE);		
    memcpy(chatbox->msgs[0], payload, len);
}

