#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>

#define BUF_SIZE 1024
#define PORT 8080
#define MAX_CLIENTS 30 // 최대 클라이언트 수

// 함수 정의
void error_handling(char *message);
void send_message_to_all(int sender_sock, char *msg, int len, int *client_socks, int *max_sock_idx);
void remove_client(int sock_fd, int *client_socks, int *max_sock_idx);

int main(int argc, char *argv[]) {
    // 1. 소켓 및 주소 변수 정의
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_size;
    
    // 2. select 관련 변수 정의
    fd_set reads, temps; // reads: 감시 대상 집합, temps: select 후 결과 집합
    int max_fd;           // 감시 대상 중 가장 큰 소켓 번호
    char buf[BUF_SIZE];
    int str_len, i;

    // 3. 연결된 클라이언트 소켓 목록 및 관리
    int client_socks[MAX_CLIENTS]; // 연결된 클라이언트 소켓 번호를 저장
    int max_sock_idx = 0;         // client_socks 배열의 유효한 크기 (다음 삽입 위치)
    
    // 배열 초기화
    for (i = 0; i < MAX_CLIENTS; i++)
        client_socks[i] = 0;

    // 4. 서버 소켓 생성 및 설정 (생략된 bind, listen 로직 추가)
    
    // 4-1. 서버 소켓 생성
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
        error_handling("socket() error");

    // 4-2. 주소 정보 초기화 및 설정
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(PORT);

    // 4-3. 소켓에 주소 할당 (Binding)
    if (bind(serv_sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() error");

    // 4-4. 연결 요청 대기 (Listening)
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");
    
    // 5. select 감시 준비
    FD_ZERO(&reads);             // reads 집합 초기화
    FD_SET(serv_sock, &reads);   // 서버 소켓을 감시 목록에 추가
    max_fd = serv_sock;

    printf("Chat Server running on port %d...\n", PORT);

    // 6. 메인 루프: I/O 이벤트 감시
    while (1) {
        temps = reads; // 원본 집합을 복사
        
        // select 호출: 이벤트를 기다림 (timeout = NULL, 무한 대기)
        if (select(max_fd + 1, &temps, 0, 0, NULL) == -1)
            error_handling("select() error");

        // 7. 이벤트 발생 확인 (max_fd까지 루프)
        for (i = 0; i < max_fd + 1; i++) {
            if (FD_ISSET(i, &temps)) { // i번 소켓에 이벤트가 발생했다면
                if (i == serv_sock) { // A. 새로운 연결 요청 (서버 소켓)
                    clnt_addr_size = sizeof(clnt_addr);
                    clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
                    
                    if (clnt_sock == -1) {
                        perror("accept() error");
                        continue;
                    }

                    // 새 클라이언트 등록
                    FD_SET(clnt_sock, &reads);
                    if (max_fd < clnt_sock) max_fd = clnt_sock; // max_fd 업데이트

                    // 클라이언트 소켓 목록에 추가
                    if (max_sock_idx < MAX_CLIENTS) {
                        client_socks[max_sock_idx++] = clnt_sock;
                        printf("New client connected: %d\n", clnt_sock);
                    } else {
                        printf("Client connection refused: Max limit reached.\n");
                        close(clnt_sock);
                        FD_CLR(clnt_sock, &reads);
                    }
                }
                else { // B. 연결된 클라이언트로부터 데이터 수신 (클라이언트 소켓)
                    str_len = read(i, buf, BUF_SIZE);
                    
                    if (str_len == 0) { // 클라이언트 연결 종료
                        // 감시 목록에서 제거
                        FD_CLR(i, &reads);
                        close(i);
                        
                        // client_socks 배열에서 해당 소켓 제거
                        remove_client(i, client_socks, &max_sock_idx);
                        
                        printf("Client disconnected: %d\n", i);
                    }
                    else { // 데이터 수신
                        // 수신된 메시지를 모든 클라이언트에게 브로드캐스트
                        send_message_to_all(i, buf, str_len, client_socks, &max_sock_idx);
                    }
                }
            }
        }
    }
    close(serv_sock);
    return 0;
}

// 8. 브로드캐스트 함수 (모든 클라이언트에게 메시지 전송)
void send_message_to_all(int sender_sock, char *msg, int len, int *client_socks, int *max_sock_idx) {
    int i;
    for (i = 0; i < *max_sock_idx; i++) {
        int target_sock = client_socks[i];
        if (target_sock > 0 && target_sock != sender_sock) {
            write(target_sock, msg, len);
        }
    }
}

// 9. 클라이언트 배열에서 소켓 제거 및 재정렬 함수
void remove_client(int sock_fd, int *client_socks, int *max_sock_idx) {
    int i;
    for (i = 0; i < *max_sock_idx; i++) {
        if (client_socks[i] == sock_fd) {
            // 해당 소켓을 배열에서 제거: 배열의 마지막 요소를 그 위치로 이동
            (*max_sock_idx)--;
            client_socks[i] = client_socks[*max_sock_idx];
            client_socks[*max_sock_idx] = 0; // 배열의 마지막 요소 초기화
            break;
        }
    }
}

// 오류 처리 함수
void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}