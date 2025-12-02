#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

// --- 전역 동기화 요소 및 플래그 ---

// 0: 부모(Parent) 차례, 1: 자식(Child) 차례를 나타내는 이진 플래그
// 프로그램 시작은 부모 차례로 가정합니다.
int turn = 0; 

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;      // 상호 배제 잠금
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;        // 동기화를 위한 조건변수

// --- 자식 쓰레드 루틴 ---
void *child_thread(void *arg) {
    long count = 0;
    
    // 자식의 차례(turn == 1)가 될 때까지 반복
    while (count < 10) { // 10회 반복 후 종료 예시
        // 1. 뮤텍스 잠금: 공유 변수 'turn' 접근 시작
        pthread_mutex_lock(&lock);

        // 2. 조건 확인: 'turn'이 자식의 차례(1)가 아닐 경우 대기
        // while 루프는 Spurious Wakeup을 처리하고 조건 재확인
        while (turn != 1) {
            // 조건변수 대기: 뮤텍스를 해제하고 신호를 기다림
            pthread_cond_wait(&cond, &lock);
        }

        // 3. 임계 영역 (작업 수행): 자식 차례일 때 메시지 출력
        printf("child: hello child\n");
        fflush(stdout); // 즉시 출력 보장
        
        // 4. 플래그 변경 및 동기화: 부모(0) 차례로 변경
        turn = 0;

        // 5. 조건변수 신호: 대기 중인 다른 쓰레드(부모)를 깨움
        pthread_cond_signal(&cond);
        
        // 6. 뮤텍스 해제: 임계 영역 이탈
        pthread_mutex_unlock(&lock);
        
        // 1초 대기 (출력 간격을 1초로 맞춤)
        sleep(1); 
        count++;
    }
    printf("Child thread finished.\n");
    return NULL;
}

// --- 부모 쓰레드 루틴 (main 함수) ---
int main() {
    pthread_t tid;
    long count = 0;
    int status;
    
    printf("--- 부모-자식 쓰레드 교대 출력 시작 ---\n");

    // 자식 쓰레드 생성
    status = pthread_create(&tid, NULL, child_thread, NULL);
    if (status != 0) {
        fprintf(stderr, "Error creating child thread: %d\n", status);
        return 1;
    }

    // 부모의 차례(turn == 0)가 될 때까지 반복
    while (count < 10) { // 10회 반복 후 종료 예시
        // 1. 뮤텍스 잠금: 공유 변수 'turn' 접근 시작
        pthread_mutex_lock(&lock);

        // 2. 조건 확인: 'turn'이 부모의 차례(0)가 아닐 경우 대기
        while (turn != 0) {
            // 조건변수 대기: 뮤텍스를 해제하고 신호를 기다림
            pthread_cond_wait(&cond, &lock);
        }

        // 3. 임계 영역 (작업 수행): 부모 차례일 때 메시지 출력
        printf("parent: hello parent\n");
        fflush(stdout);
        
        // 4. 플래그 변경 및 동기화: 자식(1) 차례로 변경
        turn = 1;

        // 5. 조건변수 신호: 대기 중인 다른 쓰레드(자식)를 깨움
        pthread_cond_signal(&cond);
        
        // 6. 뮤텍스 해제: 임계 영역 이탈
        pthread_mutex_unlock(&lock);
        
        // 1초 대기 (출력 간격을 1초로 맞춤)
        sleep(1); 
        count++;
    }
    printf("Parent thread finished.\n");

    // 자식 쓰레드가 종료될 때까지 기다림
    pthread_join(tid, NULL);

    // 뮤텍스 및 조건변수 파괴
    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cond);

    printf("--- 프로그램 종료 ---\n");
    return 0;
}