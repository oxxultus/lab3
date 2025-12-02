#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h> // 파일 처리를 위해 추가

#define PORT 8080
#define BUF_SIZE 1024

// --- 함수 원형 선언 ---
void error_handling(char *message);
void send_error(int clnt_sock, char *status);
void handle_request(int clnt_sock);
void handle_get(int clnt_sock, char *uri);
void handle_post(int clnt_sock, char *uri, char *request_body, int content_length);
void execute_cgi(int clnt_sock, char *path, char *query_string);


// --- main 함수 ---
int main(int argc, char *argv[]) {
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_size;

    // 1. 서버 소켓 생성 (TCP)
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
        error_handling("소켓 생성 오류");

    // Time-wait 상태 시에도 재사용 가능하도록 설정 (개발 편의성)
    int opt = 1;
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. 주소 정보 초기화 및 설정
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(PORT);

    // 3. 소켓에 주소 할당 (Binding)
    if (bind(serv_sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() 오류");

    // 4. 연결 요청 대기 (Listening)
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() 오류");
    
    printf("간단 웹 서버가 포트 %d에서 실행 중...\n", PORT);

    // 5. 메인 루프: 클라이언트 연결 수락
    while (1) {
        clnt_addr_size = sizeof(clnt_addr);
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
        
        if (clnt_sock == -1) {
            perror("accept() 오류");
            continue;
        }

        // 클라이언트 요청 처리
        handle_request(clnt_sock);
        
        // 소켓 닫기
        close(clnt_sock);
    }
    
    close(serv_sock);
    return 0;
}


// --- 함수 정의 ---

// 요청 처리 함수
void handle_request(int clnt_sock) {
    char buf[BUF_SIZE * 2]; // 넉넉하게 버퍼 확보
    char method[10];
    char uri[256];
    char version[10];
    int content_length = 0;
    
    // 1. 요청 메시지 수신 (헤더 및 본문 일부)
    ssize_t bytes_read = read(clnt_sock, buf, sizeof(buf) - 1);
    if (bytes_read <= 0) return;
    buf[bytes_read] = '\0';
    
    // 2. 요청 라인 파싱 (메소드, URI, 버전)
    // sscanf의 안전성을 위해 버퍼 크기를 제한하는 것이 좋으나, 교육용 예제이므로 단순화
    sscanf(buf, "%s %s %s", method, uri, version);

    // 3. Content-Length 헤더 추출 (POST 요청을 위해)
    char *len_ptr = strstr(buf, "Content-Length: ");
    if (len_ptr) {
        // len_ptr 위치에서 Content-Length 값을 추출
        sscanf(len_ptr, "Content-Length: %d", &content_length);
    }

    // 4. 요청 본문(Body) 시작 위치 찾기
    char *body_start = strstr(buf, "\r\n\r\n");
    char *post_data = (body_start) ? (body_start + 4) : NULL;
    
    printf("\n[요청 수신] %s %s (크기: %zd)\n", method, uri, bytes_read);

    // 5. 메소드별 처리 분기
    if (strcmp(method, "GET") == 0) {
        handle_get(clnt_sock, uri);
    } else if (strcmp(method, "POST") == 0) {
        handle_post(clnt_sock, uri, post_data, content_length);
    } else {
        send_error(clnt_sock, "501 Not Implemented");
    }
}

// GET 요청 처리: 정적 파일 응답
void handle_get(int clnt_sock, char *uri) {
    char file_path[256] = "."; // 현재 디렉토리를 문서 루트로 가정
    char read_buf[BUF_SIZE];
    
    // 1. URI 정규화: "/"는 기본 파일로 대체
    if (strcmp(uri, "/") == 0) {
        strcat(file_path, "/index.html"); 
    } else {
        strcat(file_path, uri);
    }
    
    // 2. 파일 열기 및 응답
    FILE *fp = fopen(file_path, "r");
    if (fp == NULL) {
        send_error(clnt_sock, "404 Not Found");
        return;
    }

    // 3. HTTP 헤더 전송 (200 OK)
    char header[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n";
    write(clnt_sock, header, strlen(header));

    // 4. 파일 내용 전송
    while (!feof(fp)) {
        size_t bytes_read = fread(read_buf, 1, BUF_SIZE, fp);
        if (bytes_read > 0) {
            write(clnt_sock, read_buf, bytes_read);
        }
    }

    fclose(fp);
    printf("[응답] GET: 200 OK (%s)\n", file_path);
}

// POST 요청 처리 및 CGI 실행
void handle_post(int clnt_sock, char *uri, char *post_data, int content_length) {
    // CGI 경로 검사
    if (strncmp(uri, "/cgi-bin/", 9) == 0) {
        char cgi_path[256] = ".";
        strcat(cgi_path, uri);
        
        printf("[POST] Content-Length: %d\n", content_length);

        // POST 데이터가 존재하면 쿼리 스트링으로 사용
        char query_string[BUF_SIZE] = "";
        if (post_data) {
            strncpy(query_string, post_data, BUF_SIZE - 1);
            query_string[BUF_SIZE - 1] = '\0';
        }

        execute_cgi(clnt_sock, cgi_path, query_string);
    } else {
        send_error(clnt_sock, "404 Not Found");
    }
}

// CGI 프로그램 실행
void execute_cgi(int clnt_sock, char *path, char *query_string) {
    int cgi_output[2];
    int pid;
    
    // 1. CGI 프로그램이 결과를 보낼 파이프 생성
    if (pipe(cgi_output) < 0) {
        send_error(clnt_sock, "500 Internal Server Error");
        perror("pipe() 오류");
        return;
    }

    // 2. 자식 프로세스 생성
    if ((pid = fork()) < 0) {
        send_error(clnt_sock, "500 Internal Server Error");
        perror("fork() 오류");
        return;
    }

    if (pid == 0) { // 자식 프로세스 (CGI 실행)
        // 3. CGI의 표준 출력(stdout)을 파이프의 쓰기 종단에 연결
        close(cgi_output[0]); // 읽기 종단 닫기
        dup2(cgi_output[1], STDOUT_FILENO); // stdout을 파이프의 쓰기 종단으로 리다이렉션
        
        // 4. CGI 환경 변수 설정
        setenv("REQUEST_METHOD", "POST", 1);
        setenv("QUERY_STRING", query_string, 1);
        
        // 5. CGI 프로그램 실행 (exec)
        execlp(path, path, NULL);
        
        // execlp 실패 시 에러 출력 및 종료
        exit(1);
        
    } else { // 부모 프로세스 (웹 서버)
        int status;
        char cgi_buf[BUF_SIZE];
        ssize_t bytes_read;
        
        // 3. 파이프의 쓰기 종단 닫기
        close(cgi_output[1]); 
        
        // 4. CGI 실행 완료 대기
        waitpid(pid, &status, 0);

        // 5. CGI 실행 결과 수신
        char header[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n";
        write(clnt_sock, header, strlen(header));
        
        printf("[응답] CGI: 200 OK (쿼리: %s)\n", query_string);

        // 6. 파이프에서 CGI 출력 결과를 읽어 클라이언트에게 전송
        while ((bytes_read = read(cgi_output[0], cgi_buf, BUF_SIZE)) > 0) {
            write(clnt_sock, cgi_buf, bytes_read);
        }
        
        close(cgi_output[0]);
    }
}

// HTTP 에러 응답 전송
void send_error(int clnt_sock, char *status) {
    char header[BUF_SIZE];
    char body[BUF_SIZE];

    // 응답 본문 생성
    sprintf(body, "<html><head><title>오류</title></head><body><h1>%s</h1><p>요청한 자원을 처리할 수 없습니다.</p></body></html>", status);
    
    // 응답 헤더 생성
    sprintf(header, "HTTP/1.1 %s\r\nContent-Type: text/html\r\nContent-Length: %zu\r\n\r\n", status, strlen(body));

    // 헤더와 본문 전송
    write(clnt_sock, header, strlen(header));
    write(clnt_sock, body, strlen(body));
    
    printf("[응답] 오류: %s\n", status);
}

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}