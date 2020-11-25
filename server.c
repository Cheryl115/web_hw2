#include <sys/socket.h>
#include <strings.h>
#include <string.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <sys/time.h>
#include "tictactoe.h"
#include <limits.h> /* for FOPEN_MAX */
#define SERV_PORT 8888
#define MAXLINE 1024
#define LISTENQ 1024
#define INFTIM (-1) /* infinite poll timeout */

struct client_status {
    int user_id;
    int room;
};

struct room {
    int board[3][3];
    int p1;
    int p2;
    int turn;
};

int sockfd, maxi;
struct pollfd client[FOPEN_MAX];
struct client_status client_status[FOPEN_MAX];
int num_room;
struct room gameroom[FOPEN_MAX];
int total_turn_count = 0;

char *user[] = {"Cheryl", "Player0", "Player1"};
char *pass[] = {"password", "player0", "player1"};

int createSocket(struct sockaddr_in *servaddr)
{
    int socket_fd;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(servaddr, sizeof(*servaddr));
    servaddr -> sin_family = AF_INET;
    servaddr -> sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr -> sin_port = htons(SERV_PORT);

    if (bind(socket_fd, (const struct sockaddr *)servaddr, sizeof(*servaddr)) < 0) {
        perror("Failed to bind");
        exit(1);
    }

    return socket_fd;

}

void initClient(struct pollfd client[], int *maxi, int listenfd)
{
    client[0].fd = listenfd;
    client[0].events = POLLRDNORM;
    for (int i = 1; i < FOPEN_MAX; i++)
        client[i].fd = -1; /* -1 indicates available entry */
    *maxi = 0;              /* max index into client[] array */
}

const char *getClientAddress(struct sockaddr_in* cliaddr)
{
    static char ip_buf[64];

    getnameinfo((struct sockaddr *)cliaddr,
                sizeof(*cliaddr),
                ip_buf, 64, 0, 0,
                NI_NUMERICHOST);

    return ip_buf;
}

int checkLogin(char *username, char *password)
{
    for (int i = 0; i < 3; i++) {
        if (strcmp(user[i], username) == 0) {
            return strcmp(pass[i], password) == 0 ? i + 1 : 0;
        }
    }
    return 0;
}

void listUser()
{
    int uid;
    char buf[PACKET_LEN - 2 * sizeof(int)] = {0};
    for (int i = 1; i <= maxi; i++) {
        if (client[i].fd >= 0 && (uid = client_status[i].user_id) != -1) {
            //printf("uid = %d, i = %d\n", uid, i);
            strncat(buf, user[uid], PACKET_LEN - 2 * sizeof(int) - strlen(buf));
            strncat(buf, ",", PACKET_LEN - 2 * sizeof(int) - strlen(buf));
        }
    }
    //printf("buf=%s\n", buf);
    printf("online players: %s\n", buf);
    sendListResponse(sockfd, buf);
}

void inviteUser(char *user_to, int connect_from)
{
    int uid;
    char buf[1024];
    struct msg_proto *msg = (struct msg_proto *)buf;
    char *user_from = user[client_status[connect_from].user_id];
    struct msg_game update_msg;
    printf("player [%s] invited player [%s]\n", user_from, user_to);
    int target = -1;
    for (int i = 1; i <= maxi; i++) {
        if (client[i].fd >= 0 && (uid = client_status[i].user_id) != -1) {
            if (strcmp(user[uid], user_to) == 0)
            {
                target = i;
                break;
            }
        }
    }
    if (target == -1) {
        perror("User not found!\n");
    }
    //printf("t=%d\n", target);
    sendInviteToUser(client[target].fd, user_from);
    getMessage(client[target].fd, buf, 1024);
    if (strcmp(msg->buf, "Y") == 0) {
        gameroom[++num_room] = (struct room){
            .board = {0},
            .p1 = connect_from,
            .p2 = target,
            .turn = 1
        };
        memset(gameroom[num_room].board, 0, 9 * sizeof(int));
        client_status[connect_from].room = num_room;
        client_status[target].room = num_room;
        update_msg = (struct msg_game)
        {
            .type = OX_GAME_SLOT,
            .row = -1,
            .col = -1,
            .turn = 1,
            .buf = { 0 }
        };
        memcpy(update_msg.board, gameroom[num_room].board, 9 * sizeof(int));
        sendGameSlot(client[connect_from].fd, update_msg);
        update_msg.turn = 0;
        sendGameSlot(client[target].fd, update_msg);
    }
}

int checkBoard(int board[3][3])
{
    // row
    for (int i = 0; i < 3; i++) {
        if (board[i][0] && board[i][0] == board[i][1] && board[i][1] == board[i][2])
            return board[i][0];
    }

    // column
    for (int i = 0; i < 3; i++) {
        if (board[0][i] && board[0][i] == board[1][i] && board[1][i] == board[2][i])
            return board[0][i];
    }

    // upper left to lower right
    if (board[0][0] && board[0][0] == board[1][1] && board[1][1] == board[2][2])
        return board[0][0];

    // upper right to lower left
    if (board[0][2] && board[0][2] == board[1][1] && board[1][1] == board[2][0])
        return board[0][2];

    return 0;
}

void updateRoom(struct msg_game *msg, int connect_id)
{
    struct msg_game update_msg;
    int num_room = client_status[connect_id].room;
    int p1, p2;
    int isEnd = 0;
    p1 = gameroom[num_room].p1;
    p2 = gameroom[num_room].p2;

    if (gameroom[num_room].turn != (p1 == connect_id ? 1 : 2)) {
        printf("error player\n");
        return;
    }
    
    gameroom[num_room].board[msg->row][msg->col] = p1 == connect_id ? 1 : 2;
    gameroom[num_room].turn = p1 == connect_id ? 2 : 1;

    if ((isEnd = checkBoard(gameroom[num_room].board))) {
        isEnd = -isEnd;
        client_status[p1].room = -1;
        client_status[p2].room = -1;
    }
    
    int isFull = 1;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (gameroom[num_room].board[i][j] == 0) {
                isFull = 0;
                break;
            }
        }
        if (!isFull) break;
    }

    if (isFull == 1) {
        update_msg = (struct msg_game)
        {
            .type = OX_GAME_SLOT,
            .row = -1,
            .col = -1,
            .turn = -(isFull+1),
            .buf = { 0 }
        };
    }
    else {
        update_msg = (struct msg_game)
        {
            .type = OX_GAME_SLOT,
            .row = -1,
            .col = -1,
            .turn = isEnd,
            .buf = { 0 }
        };
    }
    
    memcpy(update_msg.board, gameroom[num_room].board, 9 * sizeof(int));
    if (isEnd) {
        sendGameSlot(client[p1].fd, update_msg);
        sendGameSlot(client[p2].fd, update_msg);
    } else if (isFull) {
        sendGameSlot(client[p1].fd, update_msg);
        sendGameSlot(client[p2].fd, update_msg);
    } else if (p1 == connect_id) {
        sendGameSlot(client[p1].fd, update_msg);
        update_msg.turn = 2;
        sendGameSlot(client[p2].fd, update_msg);
    } else {
        sendGameSlot(client[p2].fd, update_msg);
        update_msg.turn = 1;
        sendGameSlot(client[p1].fd, update_msg);
    }
}

void parseMessage(struct msg_proto *msg, int connect_id)
{
    if (msg->type == OX_LOGIN)
    {
        // loginAccount();
        int res;
        char username[100];
        char password[100];
        getAccount((struct msg_login *)msg, username, password, 99);
        printf("Username: %s\n", username);
        printf("Password: %s\n", password);
        res = checkLogin(username, password);
        if (res) {
            sendLoginResponse(sockfd, res, "Login successful!");
            // printf("res = %d\n", res);
            client_status[connect_id].user_id = res - 1;
        }
        else
            sendLoginResponse(sockfd, res, "Login failed");
    } else if (msg->type == OX_LIST_USER) {
        listUser();
    } else if (msg->type == OX_INVITE_USER) {
        inviteUser(msg->buf, connect_id);
    } else if (msg->type == OX_GAME_SLOT) {
        updateRoom((struct msg_game*)msg, connect_id);
    }
}

int main(int argc, char **argv)
{
    int i, listenfd, connfd;
    int nready;
    int type;
    ssize_t n;
    char buf[MAXLINE];
    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr;

    
    // init server
    listenfd = createSocket(&servaddr);
    listen(listenfd, LISTENQ);

    // init client array
    initClient(client, &maxi, listenfd);
    printf("Server is listening on port %d\n", SERV_PORT);

    while (1)
    {
        nready = poll(client, maxi + 1, INFTIM);

        /* new client connection */
        if (client[0].revents & POLLRDNORM)
        {
            clilen = sizeof(cliaddr);
            connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);

            printf("New client from %s\n", getClientAddress(&cliaddr));

            for (i = 1; i < FOPEN_MAX; i++)
                if (client[i].fd < 0) {
                    client[i].fd = connfd; /* save descriptor */
                    client_status[i].user_id = -1;
                    break;
                }

            if (i == FOPEN_MAX) {
                perror("Too many clients\n");
                exit(1);
            }
            client[i].events = POLLRDNORM;
            if (i > maxi)
                maxi = i; /* max index in client[] array */

            if (--nready <= 0)
                continue; /* no more readable descriptors */
        }

        /* check all clients for data */
        for (i = 1; i <= maxi; i++) {
            if ((sockfd = client[i].fd) < 0)
                continue;
            if (client[i].revents & (POLLRDNORM | POLLERR)) {
                if ((n = read(sockfd, buf, MAXLINE)) < 0) {
                    if (n == ECONNRESET) {
                        /* for connection reset by client */
                        printf("client[%d] aborted connection\n", i);
                        close(sockfd);
                        client[i].fd = -1;
                        client_status[i].user_id = -1;
                    }
                    else {
                        perror("read error");
                        exit(1);
                    }
                }
                else if (n == 0) {
                    /* connection closed by client */
                    printf("client[%d] closed connection\n", i);
                    close(sockfd);
                    client[i].fd = -1;
                    client_status[i].user_id = -1;
                }
                else {
                    /* check client message */
                    // printf("n=%zd\n", n);
                    parseMessage((struct msg_proto *)buf, i);
                }
                if (--nready <= 0)
                    break; /* no more readable descriptors */
            }
        }
    }
}