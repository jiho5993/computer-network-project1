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
#define NOT_FOUND_404           "<h1>404 Not Found</h1>\n"
#define SERVER_ERROR_500        "<h1>500 Internal Server Error</h1>\n"

int serv_sock;  // server socket
int clnt_sock;  // client socket
int log_fd;     // log.out file descriptor

void error(char *msg);                                              // 오류가 발생했을때 처리
void signal_handler(int signum);                                    // SIGINT signal을 처리
void fill_header(char *header, int status, long len, char *type);   // response header를 만듦
void find_mime(char *mime, char *uri);                              // uri를 파싱해서 MIME을 알아냄
void error_404(int clnt_sock);                                      // 404 code 처리
void error_500(int clnt_sock);                                      // 500 code 처리
char *parse_for_res(char *buf, long long *content_len);             // response 메시지를 만들기 위한 parsing 함수
void http_handler(int clnt_sock);                                   // request 메시지를 읽고, response 메시지를 만들어 처리

int main(int argc, char **argv) {
    // 인자에 port번호를 받지 못했으면
    if(argc != 2) {
        error("no port");
    }

    // 새로운 log를 적기 위해 기존에 있던 log.out file을 삭제해주고 새로 만든다.
    if(remove("log.out") == -1) {
        perror("fail remove log.out");
    }
    log_fd = open("log.out", O_CREAT | O_RDWR, 0644);

    // 소켓 주소의 틀을 server, client 각각 형성해준다.
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

    // loop를 돌면서 클라이언트의 요청을 받아오고 응답
    while(1) {
        printf("[INFO] waiting...\n");

        // client로부터 연결이 오면 수락
        clnt_addr_size = sizeof(clnt_addr);
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
        if(clnt_sock == -1) {
            error("accept error");
        }

        // 요청된 파일을 읽고, 상태 코드 200, 404, 또는 500과 그에 맞는 파일의 내용을 보냄
        http_handler(clnt_sock);

        // 다 쓴 client 소켓을 닫아준다.
        close(clnt_sock);
    }
    // 다 쓴 server 소켓과 file descriptor를 닫아준다.
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

    // 상태코드에 따라 status text를 결정한다.
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

    // 모아놓은 정보를 header에 적어서 완성한다.
    sprintf(header, HEADER_FORMAT, status, status_text, len, type);
}

void find_mime(char *mime, char *uri) {
    char *ext_name = strrchr(uri, '.');

    // 확장자 명을 이용하여 MIME을 정해준다.
    if(!strcmp(ext_name, ".html")) {
        strcpy(mime, "text/html");
    } else if(!strcmp(ext_name, ".gif")) {
        strcpy(mime, "image/gif");
    } else if(!strcmp(ext_name, ".jpeg")) {
        strcpy(mime, "image/jpeg");
    } else if(!strcmp(ext_name, ".mp3")) {
        strcpy(mime, "audio/mpeg");
    } else if(!strcmp(ext_name, ".pdf")) {
        strcpy(mime, "application/pdf");
    }
}

void error_404(int clnt_sock) {
    // header에 404 code 관련 내용을 채우고 응답
    char header[BUF_SIZE];
    fill_header(header, 404, sizeof(NOT_FOUND_404), "text/html");
	printf("%s\n", header);

    write(clnt_sock, header, strlen(header));
    write(clnt_sock, NOT_FOUND_404, sizeof(NOT_FOUND_404));
}

void error_500(int clnt_sock) {
    // header에 500 code 관련 내용을 채우고 응답
    char header[BUF_SIZE];
    fill_header(header, 500, sizeof(SERVER_ERROR_500), "text/html");
    printf("%s\n", header);

    write(clnt_sock, header, strlen(header));
    write(clnt_sock, SERVER_ERROR_500, sizeof(SERVER_ERROR_500));
}

char *parse_for_res(char *buf, long long *content_len) {
    // response message를 만들기 위해 method와 uri를 buf에서 strtok로 따로 빼낸다.
    char *method = strtok(buf, " ");    // ex) GET
    char *uri = strtok(NULL, " ");      // ex) /src/space.jpeg
    struct stat st;                     // 파일의 정보를 저장하는 구조체

    if(method == NULL || uri == NULL) {
        error_500(clnt_sock);
        error("method or uri error");
    }

    printf("[INFO] Handling Request : method=%s, URI=%s\n", method, uri);

    // 만약 uri가 root를 가리킨다면 index.html을 불러옴
    if(!strcmp(uri, "/")) {
        strcpy(uri, "/index.html");
    }

    // file의 정보를 불러온다. 해당 파일이 존재하지 않는다면 404 code를 보낸다.
    char *local_uri;    // 요청에 따른 파일을 찾기 위한 상대적 경로
    local_uri = uri + 1;
    if(stat(local_uri, &st) < 0) {
        error_404(clnt_sock);
    }

    // Content-Length에 할당할 수 있도록 file의 stat struct를 이용한다.
    *content_len = st.st_size;

    return local_uri;
}

void http_handler(int clnt_sock) {
    char header[BUF_SIZE];
    char buf[BUF_SIZE];

    memset(header, 0, sizeof(header));
    memset(buf, 0, sizeof(buf));

    // client가 요청한 request message를 buf에 저장
    if(read(clnt_sock, buf, BUF_SIZE) == -1) {
        error_500(clnt_sock);
        error("read error");
    }

    // 저장한 request message를 log.out에 작성
    printf("%s", buf);
    write(log_fd, NEW_LOG_LINE, strlen(NEW_LOG_LINE));
    write(log_fd, buf, strlen(buf));

    char mime[40];                                       // MIME (Content-Type)
    long long content_len;                               // Content-Length
    char *local_uri = parse_for_res(buf, &content_len);  // 요청에 따른 파일을 찾기 위한 상대적 경로

    // 파일의 경로를 통해 읽기 전용으로 open한다. 실패한다면 500 code를 보낸다.
    int fd = open(local_uri, O_RDONLY);
    if(fd == -1) {
        error_500(clnt_sock);
        error("failed to open file");
    }

    find_mime(mime, local_uri);                         // 파일의 종류를 알아내고, MIME을 알아낸다.
    fill_header(header, 200, content_len, mime);        // response header를 채운다. (200 status가 고정인 이유는 위에서 이미 예외처리를 했기 때문)
    write(clnt_sock, header, strlen(header));           // 채운 헤더를 client에게 응답

    printf("%s\n", header);

    // open했던 file을 BUF_SIZE씩 client에게 보내 화면으로 띄운다.
    int nread;
    while((nread = read(fd, buf, BUF_SIZE)) > 0) {
        write(clnt_sock, buf, nread);
    }
}
