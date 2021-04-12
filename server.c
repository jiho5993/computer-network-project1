#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <signal.h>
#include <fcntl.h>

#define BUF_SIZE                65536
#define NEW_LOG_LINE            "========================== REQUEST\n"
#define HEADER_FORMAT           "HTTP/1.1 %d %s\nContent-Length: %ld\nContent-Type: %s\n\n"
#define NOT_FOUNT_CONTENT       "<h1>404 Not Found</h1>\n"
#define SERVER_ERROR_CONTENT    "<h1>500 Internal Server Error</h1>\n"

int serv_sock;
int clnt_sock;

int log_fd;

void error(char *msg);
void signal_handler(int signum);
void fill_header(char *header, int status, long len, char *type);
void find_mime(char *ct_type, char *uri);
void handle_404(int clnt_sock);
void handle_500(int clnt_sock);
void http_handler(int clnt_sock);

int main(int argc, char **argv) {
    // port번호가 없으면
    if(argc != 2) {
        error("no port");
    }

    // 새로운 log를 적기 위해 기존에 있던 log.out file을 삭제해주고 새로 만든다.
    if(remove("log.out") == -1) {
        perror("fail remove log.out");
    }
    log_fd = open("log.out", O_CREAT | O_RDWR, 0644);

    struct sockaddr_in serv_addr;
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size;

    // 서버가 SIGINT signal을 받아도 정상적으로 소켓을 닫을 수 있도록 sigaction을 설정한다.
    struct sigaction act;

    act.sa_handler = signal_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGINT, &act, NULL);

    // TCP연결 소켓 생성
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1) {
        error("[ERR]socket error");
    }

    // "bind error : Address already in use"를 방지하기 위한 코드
    int on = 1;
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    // 주소를 초기화한 후 IP주소와 port지정
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;                 // type : ipv4
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // ip address
    serv_addr.sin_port = htons(atoi(argv[1]));      // port

    // socket과 server 주소를 binding
    if(bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        error("bind error");
    }

    // 연결 대기열 5개 생성
    if(listen(serv_sock, 5) == -1) {
        error("listen error");
    }

    while(1) {
        printf("[INFO] waiting...\n");

        // client로부터 연결이 오면 수락
        clnt_addr_size = sizeof(clnt_addr);
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
        if(clnt_sock == -1) {
            error("accept error");
        }

        http_handler(clnt_sock);

        close(clnt_sock);
    }
    close(log_fd);
    close(serv_sock);

    return 0;
}

void error(char *msg) {
    char *show = "[ERR]";
    strcat(show, msg);

    close(log_fd);
    perror(show);
    exit(1);
}

void signal_handler(int signum) {
    close(log_fd);
    close(clnt_sock);
    close(serv_sock);
    printf("\nsigaction : %d\n", signum);
    exit(0);
}

void fill_header(char *header, int status, long len, char *type) {
    char status_text[40];
    switch(status) {
        case 200:
            strcpy(status_text, "OK");
            break;
        case 404:
            strcpy(status_text, "Not Found");
            break;
        case 500:
        default:
            strcpy(status_text, "Internal Server Error");
    }
    sprintf(header, HEADER_FORMAT, status, status_text, len, type);
}

void find_mime(char *ct_type, char *uri) {
    char *ext = strrchr(uri, '.');

    if(!strcmp(ext, ".html")) {
        strcpy(ct_type, "text/html");
    } else if(!strcmp(ext, ".gif")) {
        strcpy(ct_type, "image/gif");
    } else if(!strcmp(ext, ".jpeg")) {
        strcpy(ct_type, "image/jpeg");
    } else if(!strcmp(ext, ".mp3")) {
        strcpy(ct_type, "audio/mpeg");
    } else if(!strcmp(ext, ".pdf")) {
        strcpy(ct_type, "application/pdf");
    }
}

void handle_404(int clnt_sock) {
    char header[BUF_SIZE];
    fill_header(header, 404, sizeof(NOT_FOUNT_CONTENT), "text/html");
    
    write(clnt_sock, header, strlen(header));
    write(clnt_sock, NOT_FOUNT_CONTENT, sizeof(NOT_FOUNT_CONTENT));
}

void handle_500(int clnt_sock) {
    char header[BUF_SIZE];
    fill_header(header, 500, sizeof(SERVER_ERROR_CONTENT), "text/html");
    
    write(clnt_sock, header, strlen(header));
    write(clnt_sock, SERVER_ERROR_CONTENT, sizeof(SERVER_ERROR_CONTENT));
}

void http_handler(int clnt_sock) {
    char header[BUF_SIZE];
    char buf[BUF_SIZE];

    memset(header, 0, sizeof(header));
    memset(buf, 0, sizeof(buf));

    if(read(clnt_sock, buf, BUF_SIZE) == -1) {
        handle_500(clnt_sock);
        error("read error");
    }

    // printf("%s\n", buf);
    write(log_fd, NEW_LOG_LINE, strlen(NEW_LOG_LINE));
    write(log_fd, buf, strlen(buf));

    char *method = strtok(buf, " ");
    char *uri = strtok(NULL, " ");
    if(method == NULL || uri == NULL) {
        handle_500(clnt_sock);
        error("method or uri error");
    }

    printf("[INFO] Handling Request : method=%s, URI=%s\n", method, uri);

    char safe_uri[BUF_SIZE];
    char *local_uri;
    struct stat st;

    strcpy(safe_uri, uri);
    if(!strcmp(safe_uri, "/")) strcpy(safe_uri, "/index.html");

    local_uri = safe_uri + 1;
    if(stat(local_uri, &st) < 0) {
        handle_404(clnt_sock);
        return;
    }

    int fd = open(local_uri, O_RDONLY);
    if(fd == -1) {
        handle_500(clnt_sock);
        error("failed to open file");
    }

    int ct_len = st.st_size;
    char ct_type[40];
    find_mime(ct_type, local_uri);
    fill_header(header, 200, ct_len, ct_type);
    write(clnt_sock, header, strlen(header));

    int nread;
    while((nread = read(fd, buf, BUF_SIZE)) > 0) {
        write(clnt_sock, buf, nread);
    }
}