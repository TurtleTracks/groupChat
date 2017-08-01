#ifndef HW2_UTIL_H
#define HW2_UTIL_H
#include <arpa/inet.h>
#include "msg_header.h"

/* struct for linked lsit */
typedef struct group_node {
    uint16_t uid;
    struct group_node * next;
} group_node_t;

/**
 *  @brief Send a file through socket
 * 
 *  @param sockfd socket to send to
 *  @param gid gid for the header
 *  @param uid uid for the header
 *  @param filename  path of file to write to
 *
 *  @return 0 on success, -1 on failure
 */
int send_file( int sockfd, uint8_t gid, uint16_t uid, char *filename );

/**
 *  @brief Write data received from socket to file
 * 
 *  @param sockfd socket to read from
 *  @param len  number of bytes to read
 *  @param filename  path of file to write to
 *
 *  @return 0 on success, -1 on failure
 */
int receive_to_file( int sockfd, int len, char * filename );

/**
 *  @brief add an uid to the end of the group list
 * 
 *  @param head head of the group list
 *  @param uid  uid to be add to group list
 *
 *  @return 0 on success, -1 on failure
 */
int group_add_tail( group_node_t *head, uint16_t uid );

/* Returns 1 if successful, 0 if not*/
int send_main_header(mainhdr_t* hdr, int fd);

/* User needs to free mainhdr*/
mainhdr_t* fill_username_password(char *username, char *password);

/* User needs to free mainhdr. 
 * returns NULL if error occurred
 *
 */
mainhdr_t* read_main_header(int fd);

/* Returns:
 *              0 if connection was closed
 *              -1 if error occured
 *              1 if read was successful
 *
 */
int recv_wrapper(int fd, char *buff, int num_bytes);


/* Returns:
 *              0 if connection was closed
 *              -1 if error occured
 *              1 if read was successful
 *
 */
int send_wrapper(int fd, char*buff, int num_bytes);

/**
 *  @param  dest_uname   location of to copy the username to
 *  @param  dest_pw      location to copy the password to
 *  @param  payload      payload received in the packet
 *  @param  len          len of payload, get from the header len field
 *
 *  @return 0 on success, non-zero on failure
 */
int parse_username_pw( char * dest_uname, char * dest_pw, char * payload, int len );

/**
 *  @brief Function to allocate new packet with mainhdr_t, caller needs to
 *         free the returned pointer after.
 * 
 *  @param cmd  cmd field for new header
 *  @param gid  gid field for new header
 *  @param uid  uid field for new header
 *  @param len  payload len for the new header, also how many
 *              bytes to allocate for the new header
 *
 *  @return malloc'ed header with desire length, NULL on failure
 */
mainhdr_t * create_and_fill_hdr( uint8_t cmd, uint8_t gid, uint16_t uid, uint32_t len );

// http://source-code-share.blogspot.com/2014/07/implementation-of-java-stringsplit.html
int split (char *str, char c, char ***arr);
/**
 *  @brief Check the header input for invite and invite response message
 * 
 *  @param packet  header to be ckecked
 *
 *  @return 0 on success, -1 on failure
 */
int check_header_invite( mainhdr_t * packet );

/**
 *  @brief Check the header input for chatmessage
 * 
 *  @param packet  header to be ckecked
 *
 *  @return 0 on success, -1 on failure
 */
int check_header_chat( mainhdr_t * packet );

#endif /* HW2_UTIL_H */
