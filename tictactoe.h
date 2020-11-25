#define PACKET_LEN 1024
#define OX_LOGIN 1
#define OX_LOGIN_RESPONSE 2
#define OX_LIST_USER 3
#define OX_LIST_USER_RESPONSE 4
#define OX_INVITE_USER 5
#define OX_INVITE_USER_RESPONSE 6
#define OX_GAME_START 7
#define OX_GAME_SLOT 8
#define OX_RESPONSE 200

struct msg_proto {
    int type;
    char buf[PACKET_LEN - 2 * sizeof(int)];
};

struct msg_login {
    int type;
    int username_len;
    int password_len;
    char buf[PACKET_LEN - 3 * sizeof(int)];
};

struct msg_login_response {
    int type;
    int result;
    char buf[PACKET_LEN - 2 * sizeof(int)];
};

struct msg_game
{
    int type;
    int row;
    int col;
    int turn;
    int board[3][3];
    char buf[PACKET_LEN - 13 * sizeof(int)];
};

void sendAccount(int socket, char *username, char *password)
{
    struct msg_login msg;
    msg.type = OX_LOGIN;
    msg.username_len = strlen(username);
    msg.password_len = strlen(password);
    strcpy(msg.buf, username);
    strcat(msg.buf, password);
    send(socket, (char*)&msg, PACKET_LEN, 0);
}

void sendListRequest(int socket)
{
    struct msg_proto msg;
    msg.type = OX_LIST_USER;
    send(socket, (char *)&msg, PACKET_LEN, 0);
}

void sendListResponse(int socket, char *buf)
{
    struct msg_proto msg;
    msg.type = OX_LIST_USER_RESPONSE;
    strncpy(msg.buf, buf, PACKET_LEN - 2 * sizeof(int));
    send(socket, (char *)&msg, PACKET_LEN, 0);
}

void sendInviteRequest(int socket, char *username)
/* client -> server */
{
    struct msg_proto msg;
    msg.type = OX_INVITE_USER;
    strncpy(msg.buf, username, PACKET_LEN - 2 * sizeof(int));
    send(socket, (char *)&msg, PACKET_LEN, 0);
}

void sendInviteToUser(int socket, char *invite_from)
/* server -> client */
{
    struct msg_proto msg;
    msg.type = OX_INVITE_USER;
    strncpy(msg.buf, invite_from, PACKET_LEN - 2 * sizeof(int));    
    send(socket, (char *)&msg, PACKET_LEN, 0);
}

void sendInviteReply(int socket, char *answer)
{
    struct msg_proto msg;
    msg.type = OX_INVITE_USER_RESPONSE;
    strncpy(msg.buf, answer, PACKET_LEN - 2 * sizeof(int));
    send(socket, (char *)&msg, PACKET_LEN, 0);
}

void sendGameStart(int socket)
{
    struct msg_proto msg;
    msg.type = OX_GAME_START;
    send(socket, (char *)&msg, PACKET_LEN, 0);
}

void sendGameSlot(int socket, struct msg_game msg)
{
    send(socket, (char *)&msg, PACKET_LEN, 0);
}

void getAccount(struct msg_login *msg, char *username, char *password, int len)
{
    // printf("%d,%d\n", msg->username_len, msg->password_len);
    if (msg->username_len >= len) {
        printf("error: len\n");
    }
    strncpy(username, msg->buf, msg->username_len);
    username[msg->username_len] = '\0';

    if (msg->password_len >= len) {
        printf("error: len\n");
    }
    strncpy(password, msg->buf + msg->username_len, msg->password_len);
    password[msg->password_len] = '\0';
}

void sendLoginResponse(int socket, int res, char *answer)
{
    struct msg_login_response msg;
    msg.type = OX_LOGIN_RESPONSE;
    msg.result = res;
    strncpy(msg.buf, answer, PACKET_LEN - 2 * sizeof(int));
    send(socket, (char *)&msg, PACKET_LEN, 0);
}

void getMessage(int socket, char *buf, int buf_len)
{
    int read_len;
    struct msg_proto res;
    read_len = read(socket, (char *)&res, buf_len);
    if (read_len != PACKET_LEN)
        printf("test");

    memcpy(buf, &res, buf_len);
}

