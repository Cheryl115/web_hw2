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
#include <limits.h> /* for OPEN_MAX */
#include "tictactoe.h"

#define MAXLINE 4096
#define LISTENQ 1024
#define INFTIM (-1) /* infinite poll timeout */


int fd;
int isMyTurn = 0;
int board[3][3] = {0};
int run;

int createSocket(struct sockaddr_in *servaddr)
{
    int socket_fd;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(servaddr, sizeof(*servaddr));
    servaddr->sin_family = AF_INET;
    servaddr->sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr->sin_port = htons(8888);

    return socket_fd;
}

void printMenu()
{
    printf("----------------------------------------\n");
    printf("Commands:\n");
    printf("  game [USERNAME]: Play with \"USERNAME\"\n");
    printf("  ls: Get a list of online users\n");
    printf("  logout: To logout\n");
    printf("  help: List all commands you can use\n");
    printf("----------------------------------------\n\n");
}

void printBoard()
{
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (!board[i][j])
                printf("   ");
            else
                printf(" %c ", board[i][j] == 1 ? 'O' : 'X');
            if (j < 2) {
                printf("|");
            }
        }
        if (i < 2)
            printf("\n-----------\n");
        else
            printf("\n");
    }
    printf("\n\n");
}

void loginAccount()
{
    char username[100];
    char password[100];
    char *pwd;
    printf("Please enter your username: ");
    scanf("%s", username);
    pwd = getpass("Please enter your password: ");
    strncpy(password, pwd, 99);
    sendAccount(fd, username, password);
}

int getLoginResponse()
{
    int read_len;
    struct msg_login_response res;
    read_len = read(fd, (char *)&res, PACKET_LEN);
    if (read_len != PACKET_LEN)
        printf("test");
    printf("%s\n", res.buf);
    return res.result;
}

void listUser()
{
    char buf[1024];
    struct msg_proto *msg = (struct msg_proto*)buf;
    sendListRequest(fd);
}

void printUserList(char *buf)
{
    char *user;
    int i = 1;
    user = strtok(buf, ",");
    printf("----------------------------------------\n");
    printf("Online Users: \n");
    printf("%d. %s\n", i++, user);
    while ((user = strtok(NULL, ","))) {
        printf("%d. %s\n", i++, user);
    }
    printf("----------------------------------------\n\n");
}

void inviteUser()
{
    char username[100];
    scanf("%99s", username);
    printf("Waiting for %s's reply...\n\n", username);
    sendInviteRequest(fd, username);
}

void replyInvite(char *username)
{
    char buf[20];
    printf("\n%s wants to challenge you! (y/n) ", username);
    scanf("%19s", buf);
    sendInviteReply(fd, buf[0] == 'y' ? "Y" : "N");
}

void startGame()
{
    printf("\n\n------ Game Start! ------\n");
    printf("Enter 1-9 to make a move.\n");
    printf(" 1 | 2 | 3 \n");
    printf("-----------\n");
    printf(" 4 | 5 | 6 \n");
    printf("-----------\n");
    printf(" 7 | 8 | 9 \n\n");
}


void updateGame(struct msg_game *msg)
{
    memcpy(board, msg->board, 9 * sizeof(int));
    isMyTurn = msg->turn;
    if (isMyTurn == -1) {
        printf("\nGAMEOVER! \'%c\' won the game!\nPress any key to continue...\n\n>> ", isMyTurn == -1 ? 'O' : 'X');
        run = 0;
        return;
    }
    else if (isMyTurn == -2) {
        printf("The game is TIED.\nPress any key to continue...\n\n>> ");
        run = 0;
        return;
    }
    else if (isMyTurn) {
        int pos;
        struct msg_game update_msg;
        printBoard();
        printf("It's your turn!\n");
        printf("Enter position: ");
        scanf("%d", &pos);
        update_msg = (struct msg_game) {
            .type = OX_GAME_SLOT,
            .row = (pos - 1) / 3,
            .col = (pos - 1) % 3,
            .turn = 0,
            .buf = {0}};
        memset(update_msg.board, 0, 9 * sizeof(int));
        sendGameSlot(fd, update_msg);
        isMyTurn = 0;
    }
    else {
        printBoard();
    }
}

void signal_handler()
{
    char buf[1024];
    struct timeval tv;
    fd_set readfds;
    // printf("alarm\n");
    tv.tv_sec = 0;
    tv.tv_usec = 100000;

    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    select(fd + 1, &readfds, NULL, NULL, &tv);

    if (FD_ISSET(fd, &readfds)) {
        getMessage(fd, buf, 1024);
        struct msg_proto *msg = (struct msg_proto *)buf;
        if (msg->type == OX_LIST_USER_RESPONSE) {
            printUserList(msg->buf);
        } else if (msg->type == OX_INVITE_USER) {
            replyInvite(msg->buf);
        } else if (msg->type == OX_GAME_SLOT && !run) {
            startGame();
            updateGame((struct msg_game *)msg);
            run = 1;
        } else if (msg->type == OX_GAME_SLOT) {
            updateGame((struct msg_game*)msg);
        }
    }

    alarm(1);
}

int main(int argc, char **argv)
{
    int i, maxi, connfd, sockfd;
    ssize_t n;
    char buf[MAXLINE];
    int board[3][3] = {1, 2};
    struct sockaddr_in servaddr;

    // init server
    fd = createSocket(&servaddr);
    if (connect(fd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("failed to connect server.\n");
        exit(1);
    }
    printf("connected to server.\n");

    while (1)
    {
        loginAccount();
        if (getLoginResponse()) 
            /* login success */
            break;
    }

    alarm(1);
    signal(SIGALRM, signal_handler);

    printMenu();
    while (1)
    {
        printf(">> ");
        scanf("%s", buf);
        if (strcmp(buf, "ls") == 0) {
            listUser();
            sleep(1);
        } else if (strcmp(buf, "game") == 0) {
            inviteUser();
        } else if (strcmp(buf, "help") == 0) {
            printMenu();
        } else if (strcmp(buf, "logout") == 0) {
            printf("You have successfully logged out. Goodbye!\n");
            exit(0);
        }
    }
}
