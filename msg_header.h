#ifndef _MSG_HEADER_H_
#define _MSG_HEADER_H_ 1

#define MAX_CHAR_USERID 8
#define MAX_CHAR_USERPASS 128

/** 
 * C->S    cmd=USR_CREATE, gid=0, uid=0, payload="username:password"
 * S->C    cmd=USR_CREATE, gid=0, uid=0, payload= 0 on success and non-zero with error code on failure
 */
#define USR_CREATE	1 //payload format username:password

/** 
 * C->S    cmd=USR_LOGIN, gid=0, uid=0, payload="username:password"
 * S->C    cmd=USR_LOGIN, gid=0, uid=client_uid or 0 on failure, payload= 0 on success and non-zero with error code on failure
 */
#define USR_LOGIN	2 //payload format username:password

/** 
 * C->S    cmd=SEND_CHAT, gid=gid_to_send, uid=client_uid, payload="string of message"
 * S->C    cmd=SEND_CHAT, gid=gid_to_send, uid=sender_uid, payload="sender_name:" + "string of message"
 */
#define SEND_CHAT	3

/**
 * TODO Figure it out later 
 */
#define SEND_FILE	4
#define DOWNLOAD_FILE	5

/** 
 * C->S    cmd=INVITE, gid=gid_to_invite, uid=sender_uid, payload="recipient's name" //TODO or uid??
 * S->C (To recipient not sender)    cmd=INVITE, gid=gid_to_invite, uid=sender_uid, payload=""
 * (May not need this, lets talk about it)    S->C (To sender)    cmd=INVITE, gid=gid_to_invite, uid=sender_uid, payload=0 or success or error code on failure (e.g. no such user)
 */
#define INVITE		6

/** 
 * C->S    cmd=INVITE_RESP, gid=gid_to_accept, uid=acceptor_uid, payload="accept" or "decline"
 * TODO may want to notify the original inviter?? but the C->S payload for the above message need to include the original sender's uid (received from the invite message)
 */
#define INVITE_RESP	7
/** 
 * C->S    cmd=ACTIVE_LIST, gid=0, uid=sender_uid, payload=""
 * S->C    cmd=ACTIVE_LIST, gid=0, uid=sender_uid, payload="usrname1,usrname2,username3..." (client just display the entire string to stdout)
 */
#define ACTIVE_LIST	8

/** 
 * C->S    cmd=CREATE_GROUP, gid=0, uid=sender_uid, payload=""
 * S->S    cmd=CREATE_GROUP, gid=new_gid, uid=sender_uid, payload=0 or success and error code on faiure
 */
#define CREATE_GROUP	9

typedef struct
{
    uint8_t cmd;
    uint8_t gid; 
    uint16_t uid; 
    uint32_t len; 
    char payload[]; 
} mainhdr_t;

#endif



