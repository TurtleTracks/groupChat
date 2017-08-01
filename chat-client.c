#include "gui.h"
#include "hashmap.h"
#include "queue.h"
#include "msg_header.h"
#include "util.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <ctype.h>


#define BLOCK_TIME 10000
#define WIDTH 400
#define HEIGHT 500
#define MAX_MSG_SIZE 256
#define NUMBER_OF_MSGS 10

#define CFILE_FORMAT    "uid%d_%d_%d.file"

/* maps from group id to queue*/
map_t group_mapping;

/* messages that need to be sent to the server */
threadqueue_t outgoing_message_queue;

threadqueue_t group_request_queue;

uint16_t my_uid;

void update_messages(chatbox_t* chatbox)
{
    int i;
    for (i=0; i < NUMBER_OF_MSGS; ++i)
    {
        text_clear_helper(chatbox, HEIGHT - (20*i) - 100, i);
        text_draw_helper(chatbox, HEIGHT - (20*i) - 100, i);
    }
}

void* gui_group_function(void* arg){
    uint8_t group_gid = (uint8_t)*(long int*)arg;
    long int sockfd   = *(long int*)(arg+1);
    free(arg);
    threadqueue_t* gid_queue = NULL;
    int r = hashmap_get(group_mapping,(uint64_t)group_gid,(any_t*)&gid_queue);
    if (r != MAP_OK){
        printf("ERROR: gui group %d does not have a queue\n",group_gid);
    }
    struct threadmsg msg;
    struct timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = BLOCK_TIME;

    chatbox_t* chatbox = chatbox_init();
    int msg_size;
    mainhdr_t* header;
    char msg_to_send[MAX_MSG_SIZE];
     
    while(1)
    {
        msg_size = chatbox_read_input(chatbox, msg_to_send);
        if (msg_size)
        {
            char *invite   = "/invite "; 
            char *download = "/download ";
            char *sendf    = "/send_file ";
            if('/' == msg_to_send[0] && msg_size >= strlen(invite))
            {
                int num_inputs = 0;
                char **inputs = NULL;
                
                num_inputs = split(msg_to_send, ' ', &inputs);
                if(!strncmp(invite, msg_to_send, strlen(invite)))
                {
                    if ( num_inputs != 2 )
                    {
                        printf("invite format: invite username\n");
                        continue;
                    }
                    header = create_and_fill_hdr(INVITE, group_gid, my_uid, msg_size - strlen(invite) );
                    strncpy( header->payload, inputs[1], header->len ); 
                    thread_queue_add(&outgoing_message_queue, (void *)header, INVITE);
                }
                else if(msg_size >= strlen(download) && !strncmp(download, msg_to_send, strlen(download)))
                {
                    char filename[100];
                    memset( filename, '\0', 100);

                    if ( num_inputs < 2 )
                    {
                        printf("download format: download file_name\n");
                        continue;
                    }
                    strncpy( filename, inputs[1], msg_size - strlen(download)); //inputs[1] contain '\n'
                    header = create_and_fill_hdr(DOWNLOAD_FILE, group_gid, my_uid, strlen(filename) );
                    strncpy( header->payload, filename, strlen(filename));
                    thread_queue_add(&outgoing_message_queue, (void *)header, DOWNLOAD_FILE);
                }
                else if(msg_size >= strlen(sendf) && !strncmp(sendf, msg_to_send, strlen(sendf)) && msg_size >= strlen(sendf))
                {
                    printf("%s\n", msg_to_send);
                    char filename[100];
                    memset( filename, '\0', 100);

                    if ( num_inputs < 2 )
                    {
                        printf("send file format: send_file file_name\n");
                        continue;
                    }
                    strncpy( filename, inputs[1], msg_size - strlen(sendf));

                    send_file( sockfd, group_gid, my_uid, filename );
                }
            }
            else{
                header = malloc(sizeof(mainhdr_t) + msg_size);
                header->cmd = SEND_CHAT;
                header->uid = my_uid;
                header->gid = group_gid;
                header->len = msg_size;
                strncpy(header->payload, msg_to_send, msg_size);
                thread_queue_add(&outgoing_message_queue, header, 0); 
            }
        }
        if (!thread_queue_get(gid_queue, &timeout, &msg))
        {
            roll_messages(chatbox, ((mainhdr_t*)msg.data)->payload, ((mainhdr_t*)msg.data)->len);
            free(msg.data);
            update_messages(chatbox);
        }
        
    }
    return NULL;
}

int create_new_group(uint8_t gid, long int sockfd){
    threadqueue_t* gid_queue = malloc(sizeof(threadqueue_t));
    if (thread_queue_init(gid_queue) != 0)
    {
        fprintf(stderr, "Error: creating queue\n");
        return -1;
    }
    if( MAP_OK != hashmap_put(group_mapping,(uint64_t)gid,(void*)gid_queue)){
        printf("Error: with mapping gid with queue\n");
    }

    /* create a thread to handle the gui for this gid */
    pthread_t gui_group_thread;
    uint8_t* args = malloc(sizeof(long int)*2);
    *args = gid;
    *(args + 1) = sockfd; 
    if (pthread_create(&gui_group_thread, NULL, gui_group_function, (void *)args))
    {
        fprintf(stderr, "Error creating gui_group_thread\n");
        return -1; 
    }

    return 0;
}

void* server_read_function(void* arg)
{
    long int sockfd = * (long int*)arg;
    uint8_t * gid = NULL;
    char filename[100];
    mainhdr_t *recv_packet = NULL;

    while ( 1)
    {
        recv_packet = read_main_header( sockfd );
        if ( NULL != recv_packet )
        {
            switch ( recv_packet->cmd )
            {
                case( SEND_CHAT ):
                    //TODO queue to different group thread
                    // right now just displaying in stdout
                    printf("Group(%d): %s\n", recv_packet->gid, recv_packet->payload);

                    threadqueue_t* group_queue = NULL;
                    
                    if (hashmap_get(group_mapping,recv_packet->gid,(any_t*)&group_queue) == MAP_OK)
                    {
                        thread_queue_add(group_queue, recv_packet, SEND_CHAT);
                        // The GUI thread will free teh packet when its done
                        recv_packet = NULL;
                    } else {
                        printf("Group Mapping could not find the gid (%d)\n",recv_packet->gid);
                    }
                    break;

                case ( INVITE ):
                    printf("received an group invitation from (%d) to group(%d)\n", recv_packet->uid, recv_packet->gid);
                    gid = (uint8_t *) malloc(1);
                    *gid = recv_packet->gid;
                    thread_queue_add( &group_request_queue, gid, 0);
                    break;

                case( ACTIVE_LIST ):
                    printf("Active user: %s\n", recv_packet->payload);
                    break;

                case( CREATE_GROUP ):
                    if ( recv_packet->payload[0] == 0 )
                    {
                        if (create_new_group(recv_packet->gid, sockfd) != 0){
                            printf("Error creating Group (%d) in server read\n",recv_packet->gid);
                        } else {
                            printf("Group(%d) created successfully\n", recv_packet->gid);
                        }
                    }
                    else
                    {
                        printf("Failed to create group\n");
                    }
                    break;

                case( SEND_FILE ):

                    memset(filename, '\0', 100);
                    // recv_packet->uid actually indicate the file id
                    snprintf(filename, 100, CFILE_FORMAT, my_uid, recv_packet->gid, recv_packet->uid);

                    // SEND_FILE command need to manually receive payload
                    printf("Client downloading file, byte to read = %d\n", recv_packet->len);
                    receive_to_file( sockfd, recv_packet->len, filename );

                    break;

                default:
                    printf("Client received unknow command from server\n");

            } // End switch
            if ( recv_packet )
            {
                free( recv_packet );
                recv_packet = NULL;
            }

        }
        else
        {
            // received failed or connection closed, exit main loop
            printf("Exiting server read thread\n");
            break;
        }
    }    
    return NULL;
}

void* server_write_function(void* arg)
{
    long int sockfd = * (long int*)arg;
    struct threadmsg msg;
    // while(thread_queue_length(&outgoing_message_queue) > 0)
    while (1)
    {
        thread_queue_get(&outgoing_message_queue, NULL, &msg);
        mainhdr_t* hdr = (mainhdr_t*) msg.data;

        send_main_header(hdr,sockfd);
        printf("Header command %d\n", hdr->cmd);
        free(hdr);
    }
    return NULL;
}



void handle_user_input(int sockfd){
    while(1){
        char *line = NULL;
        size_t len = 0;
        ssize_t read = 0;
        mainhdr_t *send_packet = NULL;
        
        // Main thread checks all incoming group requests first
        printf("checking active invitations\n");
        while(thread_queue_length(&group_request_queue) > 0){
            printf("within active invitation\n");
            struct threadmsg group_invite;
            thread_queue_get(&group_request_queue, NULL, &group_invite);

            // format of threadmsg
            // data = uint8_t gid, msgType = unused, qlength = unused
            uint8_t invite_gid = *(uint8_t *)(group_invite.data);

            // free the group request malloc
            free(group_invite.data);

            printf("Would you like to join group %d? Y/N \n",invite_gid);
            char answer;
            scanf("%c", &answer);
            answer = tolower(answer);
            if (answer == 'y')
            {
                // send the response to server
                send_packet = create_and_fill_hdr(INVITE_RESP, invite_gid, my_uid, 1);
                send_packet->payload[0] = 'a';
                thread_queue_add(&outgoing_message_queue, (void *)send_packet, INVITE_RESP);
                // map gid to gui thread/gui queue
                // start the thread
                if (create_new_group(invite_gid, sockfd) != 0){
                    printf("Error creating Group (%d) in user input\n",invite_gid);
                } else {
                    printf("You have joined group %d!\n",invite_gid);
                }
            }
            if (answer == 'n')
            {
                // send the response to server
                send_packet = create_and_fill_hdr(INVITE_RESP, invite_gid, my_uid, 1);
                send_packet->payload[0] = 'd';
                thread_queue_add(&outgoing_message_queue, (void *)send_packet, INVITE_RESP);
                printf("You have declined group %d\n!",invite_gid);
            }
        }

        printf("enter a line:\n");
        read = getline(&line, &len, stdin);

        if (read > 0){

            int num_inputs = 0;
            char **inputs = NULL;

            num_inputs = split(line, ' ', &inputs);

            if (num_inputs > 0){
                // switch on command types
                char* command = inputs[0];
                if (strcmp(command,"new_group\n") == 0||strcmp(command,"new_group") == 0){
                    send_packet = create_and_fill_hdr(CREATE_GROUP, 0, my_uid,0);
                    thread_queue_add(&outgoing_message_queue, (void *)send_packet, CREATE_GROUP);
                    printf("queued a new group request\n");
                    continue;
                }
                if (strcmp(command,"active_list\n") == 0||strcmp(command,"active_list") == 0){
                    send_packet = create_and_fill_hdr(ACTIVE_LIST, 0, my_uid,0);
                    thread_queue_add(&outgoing_message_queue, (void *)send_packet, ACTIVE_LIST);
                    printf("queued user list request\n");
                    continue;
                }
///////////////////////////////////////////////
                if ( strcmp(command,"invite\n") == 0||strcmp(command,"invite") == 0)
                {
                    if ( num_inputs != 3 )
                    {
                        printf("invite format: invite gid username\n");
                        continue;
                    }
                    int gid;
                    sscanf (inputs[1], "%d", &gid);
                    send_packet = create_and_fill_hdr(INVITE, gid, my_uid, strlen(inputs[2]) - 1 );
                    strncpy( send_packet->payload, inputs[2], send_packet->len ); 
                    thread_queue_add(&outgoing_message_queue, (void *)send_packet, INVITE);
                }

                if ( strcmp(command,"send\n") == 0||strcmp(command,"send") == 0)
                {
                    int gid;
                    if ( num_inputs < 3 )
                    {
                        printf("send format: send gid message\n");
                        continue;
                    }
                    sscanf (inputs[1], "%d", &gid);
                    line = strchr( line, ' ');
                    line++;
                    line = strchr( line, ' ');
                    line++;

                    send_packet = create_and_fill_hdr(SEND_CHAT, gid, my_uid, strlen(line) - 1 );
                    strncpy( send_packet->payload, line, strlen(line) - 1 );
                    thread_queue_add(&outgoing_message_queue, (void *)send_packet, SEND_CHAT);
                }

                if ( strcmp(command,"send_file\n") == 0||strcmp(command,"send_file") == 0)
                {
                    int gid;
                    char filename[100];
                    memset( filename, '\0', 100);

                    if ( num_inputs < 3 )
                    {
                        printf("send file format: send_file gid file_name\n");
                        continue;
                    }
                    strncpy( filename, inputs[2], strlen(inputs[2])-1);
                    sscanf (inputs[1], "%d", &gid);

                    send_file( sockfd, gid, my_uid, filename );
                }

                if ( strcmp(command,"download\n") == 0||strcmp(command,"download") == 0)
                {
                    int gid;
                    char filename[100];
                    memset( filename, '\0', 100);

                    if ( num_inputs < 3 )
                    {
                        printf("download format: download gid file_name\n");
                        continue;
                    }
                    strncpy( filename, inputs[2], strlen(inputs[2])-1);
                    sscanf (inputs[1], "%d", &gid);
                    send_packet = create_and_fill_hdr(DOWNLOAD_FILE, gid, my_uid, strlen(filename) );
                    strncpy( send_packet->payload, filename, strlen(filename));
                    thread_queue_add(&outgoing_message_queue, (void *)send_packet, DOWNLOAD_FILE);
                }

////////////////////////////////////////////////

            }
            int i = 0;
            for (i = 0;i<num_inputs;i++){
                free(inputs[i]);
            }
        }
        else {
            printf("we are freeing the line\n");
            free(line);
        }
    }
}

int main(int argc, char *argv[])
{

    if (argc != 3)
    {
        printf("\n Usage: %s <server_ip_address> <server_port>\n", argv[0]);
        return -1;
    }

    long int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;

    if(sockfd < 0)
    {
        fprintf(stderr, "Error: Could not create socket \n");
        return -1;
    }
    
    memset(&serv_addr, '\0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0)
    {
        fprintf(stderr, "inet_pton error occured\n");
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        fprintf(stderr, "Error: Could not connect to server\n");
        return -1;
    }

    /* Initialize global data structures */
    group_mapping = hashmap_new();
    if (thread_queue_init(&outgoing_message_queue) != 0)
    {
        fprintf(stderr, "Error: creating queue\n");
        return -1;
    }

    /* Initialize the global group request queue */
    if (thread_queue_init(&group_request_queue) != 0)
    {
        fprintf(stderr, "Error: creating queue\n");
        return -1;
    }

    printf("Welcome to the chat application\n");
    printf("Do you have an account [Y/N]\n");
    char answer;
    /* Authentication */
    char username[MAX_CHAR_USERID];
    char password[MAX_CHAR_USERPASS];
    memset(&username, '\0', MAX_CHAR_USERID);
    memset(&password, '\0', MAX_CHAR_USERPASS);

    while(1)
    {
        scanf("%c", &answer);
        answer = tolower(answer);
        if (answer == 'y')
        {
            printf("Please type your username: ");
            scanf("%7s", username);

            printf("Okay %s, please type your password: ", username);
            scanf("%127s", password);
            mainhdr_t* hdr = fill_username_password(username, password);
            hdr->cmd = USR_LOGIN;
            send_main_header(hdr, sockfd);
            free(hdr);
            mainhdr_t* response_header = read_main_header(sockfd);
            // parse response
            if (response_header->cmd == USR_LOGIN)
            {
                uint8_t payload_r = *(uint8_t*)response_header->payload;
                // success
                if (payload_r == 0)
                {
                    my_uid = response_header->uid;
                    // break so it can continue to chat app
                    printf("You have logged in successfully.\n");
                    free( response_header );
                    break;
                }
                else
                {
                    // failure will be an invalid input and you'll try again
                    printf("Username or password is wrong, please try again.\n");
                }
                free( response_header );
            }

        }
        else if (answer == 'n')
        {
            printf("Please create a username (1-7 characters): ");
            scanf("%7s", username);
            
            printf("Okay %s, please create your password: ", username);
            scanf("%127s", password);
            mainhdr_t* hdr = fill_username_password(username, password);
            hdr->cmd = USR_CREATE;
            send_main_header(hdr, sockfd);
            free(hdr);
            mainhdr_t* response_header = read_main_header(sockfd);
            // parse response
            if (response_header->cmd == USR_CREATE)
            {
                uint8_t payload_r = *(uint8_t*)response_header->payload;
                // success
                if (payload_r == 0)
                {
                    //my_uid = response_header->uid;
                    // break so it can continue to chat app
                    printf("You have successfully created an account.\n");

                    free( response_header );
                    // Now login to newly created account
                    hdr = fill_username_password(username, password);
                    hdr->cmd = USR_LOGIN;
                    send_main_header(hdr, sockfd);
                    free(hdr);

                    response_header = read_main_header(sockfd);
                    payload_r = *(uint8_t*)response_header->payload;
                    if (payload_r == 0)
                    {
                        my_uid = response_header->uid;
                        printf("You have logged in successfully.\n");
                        free( response_header );
                        break;
                    }

                    free( response_header );
                }
                printf("Username was taken or invalid.\n");
            }
        }
        else
        {
            printf("You have entered an invalid input, please enter Y if you have an account or N if you don't\n");
        }
    }

    printf("Welcome %s!\n",username);

    /* create a thread which will read from the socket connected with the server */
    pthread_t server_read_thread; 
    if (pthread_create(&server_read_thread, NULL, server_read_function, (void *)&sockfd))
    {
        fprintf(stderr, "Error creating server_read_thread\n");
        return -1; 
    } 

    // /* create a thread which will read from outgoing_message_queue and send to server */
    pthread_t server_write_thread;
    if (pthread_create(&server_write_thread, NULL, server_write_function, (void *)&sockfd))
    {
        fprintf(stderr, "Error creating server_write_thread\n");
        return -1; 
    }

    while(1)
    {
        //prompt user input
        handle_user_input(sockfd);
    }
    
    return 0; 
}
