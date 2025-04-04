#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <arpa/inet.h>

// 에러 발생 시 메시지를 출력하고 종료하는 함수
void error(const char *msg) {
    perror(msg);
    exit(1);
}

// 파일 확장자에 따라 Content-Type을 반환하는 함수
const char* get_content_type(const char *filename) {
    const char *ext = strrchr(filename, '.'); // 확장자 추출
    if (!ext) return "application/octet-stream"; // 확장자가 없는 경우 기본값

    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0)
        return "text/html";
    else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
        return "image/jpeg";
    else if (strcmp(ext, ".gif") == 0)
        return "image/gif";
    else if (strcmp(ext, ".mp3") == 0)
        return "audio/mpeg";
    else if (strcmp(ext, ".pdf") == 0)
        return "application/pdf";
    else
        return "application/octet-stream";
}

// 클라이언트 요청을 처리하는 함수
void handle_client(int newsockfd) {
    char buffer[2048]; // 요청 메시지를 담을 버퍼
    bzero(buffer, sizeof(buffer)); // 버퍼 초기화

    // 클라이언트로부터 HTTP 요청 읽기
    read(newsockfd, buffer, sizeof(buffer) - 1);

    // 요청 메시지 출력 (Part A)
    printf("===== Received HTTP Request =====\n%s\n", buffer);

    char method[10], path[1024];
    sscanf(buffer, "%s %s", method, path); // 요청 메서드와 경로 파싱

    // GET 메서드만 처리
    if (strcmp(method, "GET") != 0) {
        const char *error_msg = "HTTP/1.1 501 Not Implemented\r\n\r\n";
        write(newsockfd, error_msg, strlen(error_msg));
        close(newsockfd);
        return;
    }

    // 파일 경로에서 '/' 제거하고 파일명 추출
    char *filename = path + 1;
    if (strlen(filename) == 0)
        filename = "index.html"; // 기본 파일은 index.html

    // 요청한 파일 열기
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        // 파일이 존재하지 않을 경우 404 응답
        const char *not_found = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n"
                                "<html><body><h1>404 Not Found</h1></body></html>";
        write(newsockfd, not_found, strlen(not_found));
    } else {
        // 파일 크기 구하기
        struct stat st;
        fstat(fd, &st);
        int filesize = st.st_size;

        // 파일에 맞는 Content-Type 결정
        const char *content_type = get_content_type(filename);

        // 응답 헤더 구성
        char header[1024];
        sprintf(header,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: %s\r\n"
                "Content-Length: %d\r\n"
                "Connection: close\r\n\r\n",
                content_type, filesize);
        write(newsockfd, header, strlen(header)); // 응답 헤더 전송

        // 파일 내용을 버퍼로 읽어서 클라이언트에 전송
        char file_buffer[4096];
        int bytes;
        while ((bytes = read(fd, file_buffer, sizeof(file_buffer))) > 0)
            write(newsockfd, file_buffer, bytes);

        close(fd);
    }

    close(newsockfd); // 연결 종료
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int portno = atoi(argv[1]); // 포트 번호 변환
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); // TCP 소켓 생성
    if (sockfd < 0)
        error("ERROR opening socket");

    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen = sizeof(cli_addr);

    // 서버 주소 정보 초기화
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno); // 포트 번호 네트워크 바이트 오더로 설정

    // 소켓에 주소 바인딩
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    listen(sockfd, 5); // 클라이언트 연결 대기
    printf("HTTP Server running on port %d...\n", portno);

    // 클라이언트 연결을 반복해서 처리
    while (1) {
        int newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) error("ERROR on accept");

        // 자식 프로세스를 생성해 클라이언트 요청 처리
        if (fork() == 0) {
            close(sockfd); // 자식 프로세스는 원래 소켓 닫음
            handle_client(newsockfd); // 요청 처리
            exit(0);
        } else {
            close(newsockfd); // 부모는 자식에게 소켓 넘겨주고 닫음
        }
    }

    close(sockfd); // 서버 종료 시 소켓 닫기
    return 0;
}
