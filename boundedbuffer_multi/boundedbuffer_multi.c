#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h> // sleep() 함수 사용

// --- 상수 정의 ---
#define BUFFER_SIZE 5       // 버퍼의 최대 크기
#define NUM_PRODUCERS 2     // 생산자 쓰레드 개수
#define NUM_CONSUMERS 2     // 소비자 쓰레드 개
#define MAX_ITEMS 20        // 총 생산할 아이템 개수 (프로그램 종료 조건)

// --- 전역 변수 ---
int produced_count = 0; // 실제로 생산된 아이템 총 개수 (종료 조건 확인용)
int consume_limit = MAX_ITEMS;

// --- 버퍼 구조체 정의 ---
typedef struct {
    int item[BUFFER_SIZE];    // 아이템 저장 배열
    int totalitems;           // 현재 버퍼에 있는 아이템 개수
    int in, out;              // 삽입(in) 및 제거(out) 인덱스
    pthread_mutex_t mutex;    // 상호 배제 잠금 
    pthread_cond_t full;      // 버퍼가 꽉 찼을 때 생산자 대기
    pthread_cond_t empty;     // 버퍼가 비었을 때 소비자 대기
} buffer_t;

// 버퍼 초기화 (PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER로 정적 초기화)
buffer_t bb = {
    .totalitems = 0,
    .in = 0,
    .out = 0,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .full = PTHREAD_COND_INITIALIZER,
    .empty = PTHREAD_COND_INITIALIZER
};


// --- 버퍼 관리 함수: 쓰레드 안전 보장 ---

// 버퍼에 아이템을 삽입 (생산자 역할)
void insert_item(int item, int id) {
    // 1. 뮤텍스 잠금: 임계 영역 진입
    pthread_mutex_lock(&bb.mutex);

    // 2. 조건 확인: 버퍼가 가득 찼는지 확인 (while 루프는 Spurious Wakeup 방지)
    while (bb.totalitems >= BUFFER_SIZE) {
        printf("Producer %d: Buffer is FULL. Waiting...\n", id);
        // 조건변수 대기: 뮤텍스를 해제하고 'empty' 신호를 기다림
        pthread_cond_wait(&bb.empty, &bb.mutex); 
    }

    // 3. 작업 수행: 아이템 삽입
    bb.item[bb.in] = item;
    bb.in = (bb.in + 1) % BUFFER_SIZE;
    bb.totalitems++;
    produced_count++; // 총 생산된 아이템 수 증가

    printf("Producer %d: Produced %2d. Buffer size: %d/%d\n", 
           id, item, bb.totalitems, BUFFER_SIZE);

    // 4. 조건변수 신호: 버퍼에 공간이 생겼음을 소비자에게 알림
    pthread_cond_signal(&bb.full);

    // 5. 뮤텍스 해제: 임계 영역 이탈
    pthread_mutex_unlock(&bb.mutex);
}

// 버퍼에서 아이템을 제거 (소비자 역할)
int remove_item(int *temp, int id) {
    int status = 0;
    
    // 1. 뮤텍스 잠금: 임계 영역 진입
    pthread_mutex_lock(&bb.mutex);

    // 2. 조건 확인: 버퍼가 비었는지 확인
    while (bb.totalitems <= 0) {
        // 종료 조건 검사 (생산이 완료되었고, 버퍼가 비었으면 종료)
        if (produced_count >= consume_limit && bb.totalitems == 0) {
            status = -1; // 종료 신호
            pthread_cond_broadcast(&bb.full); // 다른 소비자들을 깨워서 종료하게 함
            break;
        }
        
        printf("\tConsumer %d: Buffer is EMPTY. Waiting...\n", id);
        // 조건변수 대기: 뮤텍스를 해제하고 'full' 신호를 기다림
        pthread_cond_wait(&bb.full, &bb.mutex);
    }

    if (status != -1) {
        // 3. 작업 수행: 아이템 제거
        *temp = bb.item[bb.out];
        bb.out = (bb.out + 1) % BUFFER_SIZE;
        bb.totalitems--;

        printf("\tConsumer %d: Consumed %2d. Buffer size: %d/%d\n", 
               id, *temp, bb.totalitems, BUFFER_SIZE);

        // 4. 조건변수 신호: 버퍼에 빈 공간이 생겼음을 생산자에게 알림
        pthread_cond_signal(&bb.empty);
    }
    
    // 5. 뮤텍스 해제: 임계 영역 이탈
    pthread_mutex_unlock(&bb.mutex);
    return status;
}

// --- 쓰레드 루틴 정의 ---

void *producer_thread(void *arg) {
    int id = (int)(long)arg; // 쓰레드 ID (0부터 시작)
    int item;
    srand(time(NULL) * id); // 각 쓰레드별로 다른 시드 사용

    while (1) {
        // 총 생산 제한에 도달하면 종료
        if (produced_count >= MAX_ITEMS) break; 

        // 아이템 생성 (0~99 사이 랜덤 값)
        item = rand() % 100;
        
        insert_item(item, id);
        
        // 랜덤 대기 시간 (1~3초)
        sleep(rand() % 3 + 1); 
    }
    printf("\n>>> Producer %d finished production.\n", id);
    return NULL;
}

void *consumer_thread(void *arg) {
    int id = (int)(long)arg; // 쓰레드 ID (0부터 시작)
    int item;

    while (1) {
        if (remove_item(&item, id) == -1) break; // 종료 조건 충족 시
        
        // 아이템 소비 (실제 소비 로직)
        sleep(rand() % 3 + 1); // 랜덤 대기 시간 (1~3초)
    }
    printf("\n<<< Consumer %d finished consumption.\n", id);
    return NULL;
}


// --- 메인 함수 ---

int main() {
    pthread_t prod_tids[NUM_PRODUCERS];
    pthread_t cons_tids[NUM_CONSUMERS];
    int i;
    int status;

    printf("--- 생산자: %d개, 소비자: %d개, 버퍼 크기: %d, 총 아이템: %d ---\n\n",
           NUM_PRODUCERS, NUM_CONSUMERS, BUFFER_SIZE, MAX_ITEMS);

    // 1. 생산자 쓰레드 생성 (NUM_PRODUCERS 개)
    for (i = 0; i < NUM_PRODUCERS; i++) {
        status = pthread_create(&prod_tids[i], NULL, producer_thread, (void *)(long)i);
        if (status != 0) {
            fprintf(stderr, "Error creating producer thread %d: %d\n", i, status);
            exit(1);
        }
    }

    // 2. 소비자 쓰레드 생성 (NUM_CONSUMERS 개)
    for (i = 0; i < NUM_CONSUMERS; i++) {
        status = pthread_create(&cons_tids[i], NULL, consumer_thread, (void *)(long)i);
        if (status != 0) {
            fprintf(stderr, "Error creating consumer thread %d: %d\n", i, status);
            exit(1);
        }
    }

    // 3. 모든 생산자 쓰레드가 종료되기를 기다림
    for (i = 0; i < NUM_PRODUCERS; i++) {
        pthread_join(prod_tids[i], NULL);
    }
    printf("\n*** 모든 생산자 쓰레드가 종료되었습니다. ***\n");
    
    // 4. 생산이 끝났으므로, 남은 소비자들이 버퍼를 모두 비우고 종료하도록 signal 보냄
    // (remove_item 내부에서 최종 종료 조건 검사)
    pthread_cond_broadcast(&bb.full); 
    pthread_cond_broadcast(&bb.empty);

    // 5. 모든 소비자 쓰레드가 종료되기를 기다림
    for (i = 0; i < NUM_CONSUMERS; i++) {
        pthread_join(cons_tids[i], NULL);
    }
    printf("\n*** 모든 소비자 쓰레드가 종료되었습니다. ***\n");


    // 6. 뮤텍스 및 조건변수 파괴
    pthread_mutex_destroy(&bb.mutex);
    pthread_cond_destroy(&bb.full);
    pthread_cond_destroy(&bb.empty);

    printf("\n--- 프로그램 종료 ---\n");
    return 0;
}