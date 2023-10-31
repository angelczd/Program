#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#define INITIAL_CAPACITY 10
#define MONITOR_INTERVAL_MILLISECONDS 500

// 结构体用于存储format常量字符串及其打印次数
typedef struct FormatEntry {
    const char* format;
    int count;
} FormatEntry;

typedef struct ThreadData {
    FormatEntry* entries;
    int capacity;
    int size;
    int version;
} ThreadData;

pthread_key_t threadDataKey; // 线程特定数据键

int globalVersion = 0; // 全局版本号

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // 互斥锁

// 线程特定数据的析构函数
void deleteThreadData(void* ptr) {
    ThreadData* data = (ThreadData*)ptr;
    free(data->entries);
    free(data);
}

// 向threadEntries数组中添加或更新format常量字符串的打印次数
void updateFormatEntry(const char* format) {
    ThreadData* data = (ThreadData*)pthread_getspecific(threadDataKey);

    // 检查版本号
    if (data->version != globalVersion) {
        printf("========== Filtered Prints from Thread %ld ==========\n", pthread_self());
        for (int i = 0; i < data->size; i++) {
            if (data->entries[i].count > 1) {
                printf("%s: %d times\n", data->entries[i].format, data->entries[i].count);
            }
        }
        printf("=====================================\n");
        data->size = 0;
        data->version = globalVersion;
    }

    // 更新线程局部存储的FormatEntry
    for (int i = 0; i < data->size; i++) {
        if (data->entries[i].format == format) {
            data->entries[i].count++;
            return;
        }
    }
    if (data->size == data->capacity) {
        // 扩展内存容量
        data->capacity *= 2;
        data->entries = (FormatEntry*)realloc(data->entries, sizeof(FormatEntry) * data->capacity);
        if (data->entries == NULL) {
            perror("Memory reallocation failed");
            exit(1);
        }
    }
    data->entries[data->size].format = format;
    data->entries[data->size].count = 1;
    data->size++;
}

// 检查并输出过滤后的打印
void checkAndPrintFiltered(int signum) {
    pthread_mutex_lock(&mutex);
    globalVersion++; // 增加全局版本号
    pthread_mutex_unlock(&mutex);
}

// 重定义printf函数，实现监测和过滤
int myPrintf(const char* format, ...) {
    updateFormatEntry(format);

    va_list args;
    va_start(args, format);
    int result = vprintf(format, args);
    va_end(args);

    return result;
}

void* threadFunc(void* arg) {
    ThreadData* data = (ThreadData*)malloc(sizeof(ThreadData));
    data->entries = (FormatEntry*)malloc(sizeof(FormatEntry) * INITIAL_CAPACITY);
    data->capacity = INITIAL_CAPACITY;
    data->size = 0;
    data->version = 0;
    pthread_setspecific(threadDataKey, data);

    while (1) {
        myPrintf("This is a monitored print from thread %ld\n", (long)arg);
        usleep(100); // 小睡一段时间以降低CPU使用率
    }
    return NULL;
}

int main() {
    // 创建线程特定数据键
    pthread_key_create(&threadDataKey, deleteThreadData);

    // 重定向printf函数
    printf("========== Custom printf monitoring started ==========\n");
    printf("Use myPrintf() for monitored printing.\n");

    // 创建多个线程
    pthread_t threads[10];
    for (long i = 0; i < 10; i++) {
        pthread_create(&threads[i], NULL, threadFunc, (void*)i);
    }

    // 设置定时器，每500毫秒调用一次checkAndPrintFiltered
    struct itimerval timer;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = MONITOR_INTERVAL_MILLISECONDS * 1000;
    timer.it_interval = timer.it_value;
    setitimer(ITIMER_REAL, &timer, NULL);

    // 设置SIGALRM信号的处理函数
    signal(SIGALRM, checkAndPrintFiltered);

    // 等待所有线程结束
    for (int i = 0; i < 10; i++) {
        pthread_join(threads[i], NULL);
    }

    // 恢复printf函数
    printf("========== Monitoring ended ==========\n");

    return 0;
}