#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h> // select 함수 사용

#define BUF_SIZE 1024
#define SERVER_IP "127.0.0.1"
#define PORT 8080

void error_handling(char *message);

int main(int argc, char *argv[]) {
    int sock;
    char buf[BUF_SIZE];
    int str_len;
    struct sockaddr_in serv_addr;

    // select 관련 변수 정의
    fd_set reads, temps;
    int fd_max;

    // 1. 클라이언트 소켓 생성 (TCP)
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        error_handling("socket() error");

    // 2. 서버 주소 정보 설정
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    serv_addr.sin_port = htons(PORT);

    // 3. 서버에 연결 요청 (Connect)
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");
    else
        puts("Connected to chat server. Start typing...\n");

    // 4. select 감시 준비: 키보드 입력(0, stdin)과 서버 소켓(sock)을 감시
    FD_ZERO(&reads);
    FD_SET(0, &reads); // 0은 표준 입력(키보드)
    FD_SET(sock, &reads);
    fd_max = sock;

    // 5. 메인 루프: I/O 이벤트 감시 및 처리
    while (1) {
        temps = reads; // 원본 집합 복사

        // select 호출: 이벤트 대기
        if (select(fd_max + 1, &temps, 0, 0, NULL) == -1)
            error_handling("select() error");

        // 6. 이벤트 발생 확인 및 처리
        if (FD_ISSET(0, &temps)) { // A. 표준 입력(키보드) 이벤트 발생
            if (fgets(buf, BUF_SIZE, stdin) == NULL) continue;
            
            // 'q' 입력 시 종료
            if (strcmp(buf, "q\n") == 0) break;

            // 메시지 서버로 전송
            write(sock, buf, strlen(buf));
        }

        if (FD_ISSET(sock, &temps)) { // B. 서버 소켓으로부터 데이터 수신 이벤트 발생
            str_len = read(sock, buf, BUF_SIZE - 1);
            
            if (str_len == 0) { // 서버 연결 종료
                puts("Server closed connection.");
                break;
            }
            if (str_len == -1)
                error_handling("read() error");

            buf[str_len] = '\0';
            printf("[Message]: %s", buf); // 서버가 보낸 메시지(다른 클라이언트의 메시지) 출력
        }
    }

    // 7. 소켓 닫기
    close(sock);
    return 0;
}

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}