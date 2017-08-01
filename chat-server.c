#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "queue.h"
#include "hashmap.h"
#include "msg_header.h"
#include "util.h"

#define CREDENTIAL_FILE_NAME 	"./credential.txt"
#define ACTIVE_USER_FILE_NAME   "./active_name.txt"
#define FILE_FORMAT             "%d_%d.file"

#define GROUP_NUM_MAX       255

#define ERR_GENERIC			-10
#define ERR_USERNAME_EXIST	-11
#define ERR_NEED_LOGIN      -12
#define ERR_OUT_MEM         -13

//Handle group list
static int group_cnt;
static group_node_t * groups[GROUP_NUM_MAX] = {NULL};
static int file_cnt = 1;

//Mapping user_id to client_thread_data structs
map_t active_sockets;

typedef struct client_thread_data {
    int connfd; 
    threadqueue_t message_queue;
    uint16_t uid;
    char username[MAX_CHAR_USERID];
} client_thread_data_t;


// return 0 on success, ERR_GENERIC on failure
int check_user_in_group( uint16_t uid, uint8_t gid )
{
    int ret = ERR_GENERIC;

printf( "Checking %d in group %d\n", uid, gid);
    group_node_t * current = groups[gid];
    while( NULL != current )
    {
        if (current->uid == uid)
        {
            ret = 0;
printf( "found uid(%d) in group(%d).\n", uid, gid);
            break;
        }
        current = current->next;
    }

    return ret;
}

// return 0 on success, ERR_GENERIC on failure
int add_user_to_group( uint16_t uid, uint8_t gid )
{
    // add to the end of the list
    return (group_add_tail( groups[gid], uid ));
}

// given the username and return the uid, return ERR_GENERIC on failure
int find_uid ( char *username )
{
    FILE* cre_fd = fopen(CREDENTIAL_FILE_NAME, "r");
    char line[256];
    char* uname;
    char* uid_s;
    int uid = 0;
    int ret = ERR_GENERIC;

    if ( cre_fd )
    {
        while (fgets(line, sizeof(line), cre_fd)) 
        {
            uid_s = strtok (line," ");
            sscanf (uid_s, "%d", &uid);
            uname = strtok (NULL, " ");
            if ( 0 == strncmp( username, uname, strlen(uname) ) )
            {
                printf( "Matched %s uid = %d!\n", username, uid );
                ret = uid;
                break;
            }
                
        }
        fclose (cre_fd);
    }

   return ret;

}

//given the uid, find the username, return len on success, -1 on failure
int find_username( uint16_t in_id, char * username )
{
    FILE* cre_fd = fopen(CREDENTIAL_FILE_NAME, "r");
    char line[256];
    char* uname;
    char* uid_s;
    int uid = 0;
    int ret = ERR_GENERIC;

    if ( cre_fd )
    {
        while (fgets(line, sizeof(line), cre_fd))
        {
            uid_s = strtok (line," ");
            sscanf (uid_s, "%d", &uid);
            if ( (int)in_id == uid )
            {
                uname = strtok (NULL, " ");
                strncpy( username, uname,MAX_CHAR_USERID );
                ret = strlen (username);
                break;
            }
        }
    }
    return ret;
}

// return user_id mapped to username, return ERR_GENERIC on failure
int match_user_pw( char *username, char *password )
{
    FILE* cre_fd = fopen(CREDENTIAL_FILE_NAME, "r");
    char line[256];
    char* uname;
    char* pw;
    char* uid_s;
    int uid = 0;
    int ret = ERR_GENERIC;

    if ( cre_fd )
    {
        while (fgets(line, sizeof(line), cre_fd)) 
        {
            uid_s = strtok (line," ");
            sscanf (uid_s, "%d", &uid);
            uname = strtok (NULL, " ");
            pw = strtok (NULL, " ");
            if ( 0 == strncmp( username, uname, strlen(uname) ) )
            {
                if ( 0 == strncmp( password, pw, strlen(pw)-1 ) )
                {
                    printf( "Matched %s uid = %d!\n", username, uid );
                    ret = uid;
                    break;
                }
            }
                
        }
        fclose (cre_fd);
    }

   return ret;
}

// Create usrname and password and map to new uid
int create_user ( char *username, char *password )
{
    FILE* cre_fd = fopen(CREDENTIAL_FILE_NAME, "a+");
    char line[256];
    char* uname;
    int ret = ERR_GENERIC;
    int count = 1;
    if ( cre_fd )
    {
        while (fgets(line, sizeof(line), cre_fd))
        {
            count++;
            strtok (line, " ");
            uname = strtok( NULL, " " );
            if ( 0 == strncmp( username, uname, strlen(uname) ) )
            {
               printf("username (%s) already exist\n", username);
               ret = ERR_USERNAME_EXIST;
               break;
            }
         }

         if ( ERR_USERNAME_EXIST != ret )
         {
            fprintf( cre_fd, "%d %s %s\n", count, username, password );
            ret = count;
            printf("adding %s into the %d th\n", username, count );
         }

         fclose( cre_fd );
    }

    return ret;
}

// iterate function for hashmap
int active_socket_walk(void * param, void* value)
{
    FILE* fd = (FILE*) param;

    client_thread_data_t* data = (client_thread_data_t*) value;
    printf("data->username = (%s),data->uid = %d!\n", data->username, data->uid );
    fprintf( fd, "%s,",data->username );
    return 0;
}

// Get the active user name, need to free the entry dest
// return the string allocated
char * get_active_users_str( )
{
    FILE * fd = fopen(ACTIVE_USER_FILE_NAME, "w");
    char * dest = (char*) malloc(1024);
    memset( dest, '\0', 1024 );

    hashmap_iterate( active_sockets, active_socket_walk, fd );
    fclose(fd);

    fd = fopen(ACTIVE_USER_FILE_NAME, "r");
    fgets(dest, 1024, fd);
    fclose(fd);
    return dest;
}

// Server forward receievd chat message to group
void forward_to_group( uint8_t gid, mainhdr_t * send_packet )
{
    group_node_t * head = groups[gid];
    group_node_t * current = head;
    client_thread_data_t* data = NULL;
    mainhdr_t * send = NULL;

    /* find the end of the link */
    while (current != NULL) 
    {
        if( MAP_OK !=  hashmap_get( active_sockets, (uint64_t)current->uid, (void**) &data ) )
        {
            //TODO need to adjust linked list if member already sign off
            printf("hashmap_get failed, user (uid=%d) is not online anymore\n", current->uid);
        }
        else
        {
            // found online user, forward the message
            printf("forwarding message to gid=(%d) uid=(%d).\n", gid, current->uid);
            send = (mainhdr_t*) malloc( sizeof(mainhdr_t) + send_packet->len );
            memcpy( send, send_packet, sizeof(mainhdr_t) + send_packet->len );
            send_main_header( send, data->connfd );
            free( send );
        }
 
        current = current->next;
    }
}


void *client_thread_function(void *arg)
{
    client_thread_data_t* data = (client_thread_data_t *) arg;
    /* data->uid need to be initialized before doing anything other then
     * USR_CREATE or USR_LOGIN */
    int status;
    char username[MAX_CHAR_USERID];
    char password[MAX_CHAR_USERPASS];
    int peer_id = -1;
    int len;
    char * active_str;
    client_thread_data_t* peer_data = NULL;
    mainhdr_t *recv_packet = NULL;
    mainhdr_t *send_packet = NULL;
    char filename[100];
    char **inputs = NULL;
    int file_id = 0;

    while (1)
    {
        recv_packet = read_main_header( data->connfd );

        if ( NULL != recv_packet )
        {
            switch ( recv_packet->cmd )
            {
                /***********************************
                 * USR_CREATE                      *
                 ***********************************/
                case( USR_CREATE ):
                    // Allocate response packet, header + 1 byte payload to indicate the status
                    send_packet = create_and_fill_hdr( recv_packet->cmd, 0, 0, 1 );

                    memset( username, '\0', MAX_CHAR_USERID );
                    memset( password, '\0', MAX_CHAR_USERPASS );

                    parse_username_pw( username, password, recv_packet->payload, recv_packet->len );
                    printf( "Parsed username = (%s), parsed password = (%s)\n", username, password );

                    // Try to add username and password into credential databaes
                    status = create_user( username, password );
                    if ( status > 0 )
                    {
                        // success
                        int new_uid = match_user_pw( username, password );
                        // send the new uid as well so the create if successful can login automatically
                        send_packet->uid = new_uid;
                        send_packet->payload[0] = (uint8_t) 0;
                    }
                    else
                    {
                        send_packet->payload[0] = (uint8_t) ERR_USERNAME_EXIST;
                    }
                    // Send response to the client
                    send_main_header( send_packet, data->connfd );
                    
                    break;

                /***********************************
                 * USR_LOGIN                       *
                 ***********************************/
                case( USR_LOGIN ):
                    memset( username, '\0', MAX_CHAR_USERID );
                    memset( password, '\0', MAX_CHAR_USERPASS );

                    parse_username_pw( username, password, recv_packet->payload, recv_packet->len );
                    status = match_user_pw( username, password );

                    if ( status > 0 )
                    {

                        // success, add to hashmap
                        data->uid = (uint16_t)status;
                        strncpy( data->username, username, MAX_CHAR_USERID);
                        
                        send_packet = create_and_fill_hdr( USR_LOGIN, 0, status, 1 );
                        send_packet->payload[0] = (uint8_t) 0;

                        /* save the socket info so we can access else where with uid */
                        if( MAP_OK !=  hashmap_put( active_sockets, (uint64_t)status, data ) )
                        {
                            printf("hashmap_put failed\n");
                        }

                    }
                    else
                    {
                        send_packet = create_and_fill_hdr( USR_LOGIN, 0, 0, 1 );
                        send_packet->payload[0] = (uint8_t) ERR_GENERIC;
                    }
                    // Send response to the client
                    send_main_header( send_packet, data->connfd );

                    break;

                /***********************************
                 * CREATE_GROUP                    *
                 ***********************************/
                case( CREATE_GROUP ):

                    send_packet = create_and_fill_hdr( CREATE_GROUP, 0, recv_packet->uid, 1 );
                    if ( recv_packet->uid <= 0 )
                    {
                        send_packet->gid = 0;
                        send_packet->payload[0] = (uint8_t) ERR_NEED_LOGIN;
                        printf("User need to first login before creating a group.\n");
                    }
                    else
                    {
                        while ( groups[group_cnt] != NULL )
                        {
                            group_cnt++;
                        }

                        if ( group_cnt > GROUP_NUM_MAX )
                        {
                            printf("Server cannot create new group, run out of memory\n");
                            send_packet->gid = 0;
                            send_packet->payload[0] = (uint8_t) ERR_OUT_MEM;
                        }
                        else
                        {
                            send_packet->gid = group_cnt;
                            send_packet->payload[0] = (uint8_t) 0;
                            // add the uid to the group linked list
printf("adding uid(%d) to group gid(%d).\n", recv_packet->uid, send_packet->gid);
                            groups[send_packet->gid] = (group_node_t *) malloc(sizeof (group_node_t));
                            groups[send_packet->gid]->uid = recv_packet->uid;
                            groups[send_packet->gid]->next = NULL;
                        }
                    }
                    // Send response to the client
                    send_main_header( send_packet, data->connfd );

                    break;

                /***********************************
                 * INVITE                          *
                 ***********************************/
                case( INVITE ):
                    peer_id = -1;
                    peer_data = NULL;

                    /* Input checking, should already be done on the client side
                     * But just to be safe */
                    if ( 0 != check_header_invite( recv_packet ) )
                    {
                        break;
                    }

                    if ( groups[recv_packet->gid] == NULL )
                    {
                        printf("User must provided a valid gid to invite user\n");
                        break;
                    }
                    else if ( 0 != check_user_in_group( recv_packet->uid, recv_packet->gid ))
                    {
                        printf("User must already be in the group before inviting other people\n");
                        break;
                    }

                    // input should be good when we reach this point
                    memset( username, '\0', MAX_CHAR_USERID );
                    strncpy( username, recv_packet->payload, recv_packet->len );
                    peer_id = find_uid( username );

                    if ( peer_id <= 0 )
                    {
                        printf("Cannot find such user (%s)!\n", username);
                        break;
                    }

                    // Check if other user is logged in
                    if( MAP_OK !=  hashmap_get( active_sockets, (uint64_t)peer_id, (void**) &peer_data ) )
                    {
                        printf("hashmap_get failed, user (%s) is not online\n", username);
                        break;
                    }

                    // Create and forward packet to recipient
                    send_packet = create_and_fill_hdr( INVITE, recv_packet->gid, recv_packet->uid, 0 );
                    send_main_header( send_packet, peer_data->connfd );
                    
                    break;

                /***********************************
                 * INVITE_RESP                     *
                 ***********************************/
                case( INVITE_RESP ):
                     /* Input checking, should already be done on the client side
                     * But just to be safe */
                    if ( 0 != check_header_invite( recv_packet ) )
                    {
                        break;
                    }
                   
                    if ( groups[recv_packet->gid] == NULL )
                    {
                        printf("User must provided a valid gid to join\n");
                        break;
                    }
                    else if ( 0 == check_user_in_group( recv_packet->uid, recv_packet->gid ))
                    {
                        printf("User already in the group, server do nothing\n");
                        break;
                    }

                    if ( 'a' == recv_packet->payload[0] )
                    {
                        // user accept the group invitation
                        printf("Adding user (uid=%d) to group(%d)\n", recv_packet->uid, recv_packet->gid );
                        add_user_to_group( recv_packet->uid, recv_packet->gid );
                    }
                    else
                    {
                        // User decline to join the group
                        printf("User (uid=%d) decline to join group(%d)\n", recv_packet->uid, recv_packet->gid );
                    }

                    break;

                /***********************************
                 * SEND_CHAT                       *
                 ***********************************/
                case( SEND_CHAT ):
                    /* Input checking, should already be done on the client side
                     * But just to be safe */
                    if ( 0 != check_header_chat( recv_packet ) )
                    {
                        break;
                    }

                    if ( groups[recv_packet->gid] == NULL )
                    {
                        printf("User must provided a valid gid to send chat to\n");
                        break;
                    }
                    else if ( 0 != check_user_in_group( recv_packet->uid, recv_packet->gid ))
                    {
                        printf("User must already be in the group to send\n");
                        break;
                    }

                    memset( username, '\0', MAX_CHAR_USERID );
                    len = find_username( recv_packet->uid, username );
                    if ( len <= 0 )
                    {
                        printf("Server forward chat cannot find sender's(uid = %d) username!\n", recv_packet->uid );
                    }
                    else
                    {
                        // new payload format: "sender_name:message_from_sender"
                        send_packet = create_and_fill_hdr( SEND_CHAT, recv_packet->gid, recv_packet->uid, recv_packet->len+len+1 );
                        strncpy( send_packet->payload, username, len );
                        send_packet->payload[len] = ':';
                        memcpy( &(send_packet->payload[len+1]), recv_packet->payload, recv_packet->len );

                        forward_to_group( send_packet->gid, send_packet );
                    }

                    break;

                /***********************************
                 * ACTIVE_LIST                     *
                 ***********************************/
                case( ACTIVE_LIST ):
                    active_str = get_active_users_str();

                    if (active_str)
                    {
                        len = strlen( active_str );
                        printf("Line=<%s>, len=<%d>\n", active_str, len);

                        send_packet = create_and_fill_hdr( ACTIVE_LIST, 0, recv_packet->uid, len );
                        strncpy( send_packet->payload, active_str, len );

                        send_main_header( send_packet, data->connfd );

                        free(active_str);
                        active_str = NULL;
                    }
                    else
                    {
                        send_packet = create_and_fill_hdr( ACTIVE_LIST, 0, recv_packet->uid, 24 );
                        strncpy( send_packet->payload, "Failed to get user list.",  24);

                        send_main_header( send_packet, data->connfd );

                    }

                    break;


                /***********************************
                 * SEND_FILE                       *
                 ***********************************/
                case( SEND_FILE ):

                    memset(filename, '\0', 100);
                    snprintf(filename, 100, FILE_FORMAT, recv_packet->gid, file_cnt);
                    file_cnt++;

                    // SEND_FILE command need to manually receive payload
                    printf("Server received send file command, byte to read = %d\n", recv_packet->len);
                    receive_to_file( data->connfd, recv_packet->len, filename );

                    // Notify all user in the group
                    memset( username, '\0', MAX_CHAR_USERID );
                    len = find_username( recv_packet->uid, username );

                    send_packet = create_and_fill_hdr( SEND_CHAT, recv_packet->gid, recv_packet->uid, len + strlen(filename) + 17 );
                    snprintf( send_packet->payload, send_packet->len + 1, "%s just send file %s", username, filename);

                    forward_to_group( send_packet->gid, send_packet );

                    break;

                /***********************************
                 * DOWNLOAD_FILE                   *
                 ***********************************/
                case( DOWNLOAD_FILE ):

                    memset(filename, '\0', 100);
                    snprintf(filename, 100, "./%s", recv_packet->payload);

                    if ( groups[recv_packet->gid] == NULL )
                    {
                        printf("User must provided a valid gid to download\n");
                        break;
                    }
                    else if ( 0 != check_user_in_group( recv_packet->uid, recv_packet->gid ))
                    {
                        printf("User must already be in the group to download\n");
                        break;
                    }

                    split( filename, '_', &inputs );
                    split( inputs[1], '.', &inputs );

                    sscanf (inputs[0], "%d", &file_id);
                    // kind of a hack to include the file id in the uid field
                    send_file( data->connfd, recv_packet->gid, file_id, filename ); 

                    break;

                default:
                    printf("Command (%d) is not defined!!\n", recv_packet->cmd);
                    break;
            }

            /* Reclaim malloc'ed memory */
            free( recv_packet );
            recv_packet = NULL;
            if (send_packet)
            {
                free( send_packet );
                send_packet = NULL;
            }

        }
        else
        {
            // received failed or connection closed, exit main loop
            printf("Exiting recv thread for uid(%d)\n", data->uid);
            break;
        }

    }

    // Close the socket
    close( data->connfd );
    if ( data->uid > 0 )
    {
        // user already logged in, need to remove from active list
        hashmap_remove(active_sockets, (uint64_t) data->uid);
    }
    free(data);
    pthread_exit(NULL);
}



int main(int argc, char *argv[])
{

    if (argc != 2) 
    {
        printf("Please run as ./server port_number\n");
        return -1;
    }
    else
    {
        int listenfd, connfd, rc;
        int serverPort = atoi(argv[1]);
        struct sockaddr_in serv_addr;
        int enable = 1;

        //Initialize data structures
        active_sockets = hashmap_new();
        group_cnt = 0;

        listenfd = socket(AF_INET, SOCK_STREAM, 0);
        if (listenfd < 0 )
        {
            printf("Error creating the socket!\n");
            return -1;
        }

        if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        {
            printf("setsockopt(SO_REUSEADDR) failed");
            return -1;
        }

        memset(&serv_addr, '0', sizeof(serv_addr));

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        serv_addr.sin_port = htons(serverPort);

        if ( bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
        {
            printf("Error binding the socket!\n");
            return -1;
        } 

        listen(listenfd, 20); 
        
        while(1)
        {
            pthread_t client_thread;
            client_thread_data_t* client_arg = (client_thread_data_t*) malloc(sizeof(struct client_thread_data));
            client_arg->connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);
            client_arg->uid = -1;

            if (thread_queue_init(&(client_arg->message_queue)) != 0)
            {
                fprintf(stderr, "Error initializing message queue");
                continue;
            }
            rc = pthread_create(&client_thread, NULL, client_thread_function, (void *)client_arg);
            if (rc)
            {
                fprintf(stderr, "Error; return code from pthread_create() is %d\n", rc);
            }
        }
    }
}
