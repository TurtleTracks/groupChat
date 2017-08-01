#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "util.h"

int send_file( int sockfd, uint8_t gid, uint16_t uid, char *filename )
{
    FILE * read_fd;
    int file_size;
    char buff[1024];
    int byte_read = 0;
    mainhdr_t * send_packet = NULL;

    read_fd = fopen(filename, "r");
    if (read_fd)
    {
        fseek( read_fd, 0L, SEEK_END);
        file_size = ftell( read_fd );
        fseek( read_fd, 0L, SEEK_SET);
        printf("file size = %d\n", file_size);

        send_packet = create_and_fill_hdr(SEND_FILE, gid, uid, 0 );
        send_packet->len = htonl(file_size);
        send_packet->uid = htons(uid);

        // send header directly rather than through queue
        write(sockfd, send_packet, sizeof(mainhdr_t));
        free( send_packet );
        while( file_size > 0 )
        {
            byte_read = fread( buff, 1, sizeof(buff), read_fd );
            file_size -= byte_read;
            write(sockfd, buff, byte_read);
        }

        fclose( read_fd );
        return 0;
    }
    else 
    {
        printf("failed to open file %s to send\n", filename);
        return -1; 
    }
}

int receive_to_file( int sockfd, int len, char * filename )
{
    int byte_to_read = len;
    int recv_bytes = 0;
    char buff[1024];
    FILE * write_fd = fopen(filename, "w");

    if ( write_fd )
    {
        while ( byte_to_read > 0 )
        {
            printf("byte_to_read = %d, trying to read\n", byte_to_read);
            recv_bytes = read( sockfd, buff, byte_to_read < sizeof(buff) ? byte_to_read : sizeof(buff) );
            byte_to_read = byte_to_read - recv_bytes;
            printf("received %d bytes, remaining %d\n", recv_bytes, byte_to_read);

            fwrite( buff, 1, recv_bytes, write_fd );
        }
        fclose(write_fd);
        return 0;
    }
    else
    {
        printf("Failed to open file %s to write\n", filename);
        return -1;
    }
}

int group_add_tail( group_node_t *head, uint16_t uid )
{
    group_node_t * current = head;
    /* find the end of the link */
    while (current->next != NULL) {
        current = current->next;
    }
    
    current->next = malloc(sizeof(group_node_t));
    if ( current->next )
    {
        current->next->uid = uid;
        current->next->next = NULL;
        return 0;
    }
    else
    {
         printf("Linked list malloc failed!!\n");
         return -1;
    }
}

int send_main_header(mainhdr_t* hdr, int fd)
{
    int bytes_to_send = sizeof(mainhdr_t) + hdr->len;
    hdr->uid = htons(hdr->uid);
    hdr->len = htonl(hdr->len);
    if(send_wrapper(fd, (char *)hdr, bytes_to_send)) return 1;
    else return 0;
}

mainhdr_t * create_and_fill_hdr( uint8_t cmd, uint8_t gid, uint16_t uid, uint32_t len )
{
    mainhdr_t * hdr = NULL;

    // +1 for safety
    hdr = ( mainhdr_t * ) malloc( sizeof( mainhdr_t ) + len + 1);

    if (hdr)
    {
        memset( hdr->payload, '\0', len+1);
        // Note that hton() calls are being handled by the send_main_header() wrapper
        hdr->cmd = cmd;
        hdr->gid = gid;
        hdr->uid = uid;
        hdr->len = len;
    }
    else
    {
        printf("Debug: malloc failed?????\n");
    }

    return hdr;
}

mainhdr_t* fill_username_password(char *username, char *password)
{
    int username_length = strlen(username);
    int password_length = strlen(password);
    mainhdr_t* hdr = (mainhdr_t *)malloc(sizeof(mainhdr_t) + username_length + password_length + 1);
    hdr->len = username_length + password_length + 1;
    strncpy(hdr->payload, username, username_length);
    hdr->payload[username_length] = ':';
    strncpy(hdr->payload + username_length + 1, password, password_length);
    return hdr;
}

mainhdr_t* read_main_header(int fd)
{
    mainhdr_t basic_hdr;
    mainhdr_t *hdr;
    if (recv_wrapper(fd, (char *)&basic_hdr, sizeof(basic_hdr)))
    {
        basic_hdr.uid = ntohs(basic_hdr.uid);
        basic_hdr.len = ntohl(basic_hdr.len);
        if ( basic_hdr.cmd != SEND_FILE )
        {
            // +1 for safety
            hdr = (mainhdr_t*)malloc(sizeof(mainhdr_t) + basic_hdr.len + 1);
            memset(hdr->payload, '\0', basic_hdr.len + 1);
        }
        else
        {
            hdr = (mainhdr_t*)malloc(sizeof(mainhdr_t));
        }
        hdr->cmd = basic_hdr.cmd;
        hdr->gid = basic_hdr.gid;
        hdr->uid = basic_hdr.uid;
        hdr->len = basic_hdr.len;
        if ( hdr->len != 0 && hdr->cmd != SEND_FILE)
        {
            if (recv_wrapper(fd, hdr->payload, hdr->len))
            {
                return hdr;
            }
            else
            {
                // avoid memory leak when recv failed
                free( hdr );
                return NULL;
            }
        }
        else
        {
            return hdr;
        }
    }
    else return NULL;
}

int recv_wrapper(int fd, char *buff, int num_bytes)
{
    int bytes_received;
    int tmp = read(fd, buff, num_bytes);
    if (tmp == 0 || tmp == -1) return tmp;
    bytes_received = tmp;
    while (bytes_received < num_bytes)
    {
        tmp = read(fd, buff + bytes_received, num_bytes - bytes_received);
        if (tmp == 0 || tmp == -1) return tmp;
        bytes_received += tmp;
        
    }
    return 1;
}

int send_wrapper(int fd, char *buff, int num_bytes)
{
    int bytes_sent;
    int tmp = write(fd, buff, num_bytes);
    if (tmp == 0 || tmp == -1) return tmp;
    bytes_sent = tmp;
    while (bytes_sent < num_bytes)
    {
        tmp = read(fd, buff + bytes_sent, num_bytes - bytes_sent);
        if (tmp == 0 || tmp == -1) return tmp;
        bytes_sent += tmp;
    }
    return 0;

}

int parse_username_pw( char * dest_uname, char * dest_pw, char * payload, int len )
{
   char * delim = strchr( payload, ':' );
   char * uname = payload;
   char * pw = delim + 1;
   int uname_len = 0;
   int pw_len = 0;
   int ret = -1;

   uname_len = delim - payload;  
   pw_len = len - uname_len - 1;

   if ( uname_len > MAX_CHAR_USERID || pw_len > MAX_CHAR_USERPASS )
   {
      printf("username or password too large!!!!\n");
      ret =  -1;
   }
   else
   {
      strncpy( dest_uname, uname, uname_len );
      strncpy( dest_pw, pw, pw_len );
      ret = 0;
   }
   return ret;
}

int split (char *str, char c, char ***arr)
{
    int count = 1;
    int token_len = 1;
    int i = 0;
    char *p;
    char *t;

    p = str;
    while (*p != '\0')
    {
        if (*p == c)
            count++;
        p++;
    }

    *arr = (char**) malloc(sizeof(char*) * count);
    if (*arr == NULL)
        exit(1);

    p = str;
    while (*p != '\0')
    {
        if (*p == c)
        {
            (*arr)[i] = (char*) malloc( sizeof(char) * token_len );
            if ((*arr)[i] == NULL)
                exit(1);

            token_len = 0;
            i++;
        }
        p++;
        token_len++;
    }
    (*arr)[i] = (char*) malloc( sizeof(char) * token_len );
    if ((*arr)[i] == NULL)
        exit(1);

    i = 0;
    p = str;
    t = ((*arr)[i]);
    while (*p != '\0')
    {
        if (*p != c && *p != '\0')
        {
            *t = *p;
            t++;
        }
        else
        {
            *t = '\0';
            i++;
            t = ((*arr)[i]);
        }
        p++;
    }

    return count;
}
// return 0 on success, -1 on failure
int check_header_invite( mainhdr_t * packet )
{
    int ret = 0;
    if ( packet->uid <= 0)
    {
        printf("User must first login to invite other user\n");
        ret = -1;
    }

    if ( packet->gid < 0)
    {
        printf("User must provided a valid gid to invite user\n");
        ret = -1;
    }
                    
    if ( packet->len > MAX_CHAR_USERID-1 )
    {
        printf("The username privided by the inviter is too long\n");
        ret = -1;
    }
    return ret;
}

// return 0 on success, -1 on failure
int check_header_chat( mainhdr_t * packet )
{
    int ret = 0;
    if ( packet->uid <= 0)
    {
        printf("User must first login to send chat to other user\n");
        ret = -1;
    }

    if ( packet->gid < 0)
    {
        printf("User must provided a valid gid to send chat to user\n");
        ret = -1;
    }
    return ret;
}                     
