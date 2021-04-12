#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include <fcntl.h>

int clnt_sock;

void error(char *msg);
void handler(int signum);

int main(int argc, char **argv) {
    struct sigaction act;

    act.sa_handler = handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGINT, &act, NULL);

    
    struct sockaddr_in serv_addr;
    char msg[1024];

    memset(msg, 0, sizeof(msg));

    clnt_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(clnt_sock == -1) {
        error("socket error");
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if(connect(clnt_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        error("connect error");
    }

    // if(read(clnt_sock, msg, sizeof(msg)) == -1) {
    //     error("read error");
    // }

    // printf("%s\n", msg);

    printf("input : ");
    char buf[100];
    memset(buf, 0, sizeof(buf));
    fgets(buf, 100, stdin);
    if(write(clnt_sock, buf, 100-1) == -1) {
        error("write error");
    }

    char new_buf[100];
    memset(new_buf, 0, sizeof(new_buf));
    if(read(clnt_sock, new_buf, 100) == -1) {
        error("read error");
    }
    printf("get : %s\n", new_buf);

    close(clnt_sock);

    return 0;
}

void error(char *msg) {
    perror(msg);
    exit(1);
}

void handler(int signum) {
    close(clnt_sock);
    printf("sigaction : %d\n", signum);
    exit(0);
}