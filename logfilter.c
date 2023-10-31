#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#define INITIAL_CAPACITY 1000
#define MONITOR_INTERVAL_MILLISECONDS 500

// 结构体用于存储format常量字符串及其打印次数
typedef struct FormatEntry {
    const char* format;
    int count;
} FormatEntry;

FormatEntry* entries = NULL; // 存储format常量字符串的数组
int capacity = INITIAL_CAPACITY; // 初始容量
int size = 0; // 当前存储的条目数量
struct timeval lastPrintTime; // 上次打印时间

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // 互斥锁

// 初始化entries数组
void initializeEntries() {
    entries = (FormatEntry*)malloc(sizeof(FormatEntry) * capacity);
    if (entries == NULL) {
        perror("Memory allocation failed");
        exit(1);
    }
}

// 向entries数组中添加或更新format常量字符串的打印次数
void updateFormatEntry(const char* format) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < size; i++) {
        if (entries[i].format == format) { // 直接用双等号比较
            entries[i].count++;
            pthread_mutex_unlock(&mutex);
            return;
        }
    }

    if (size == capacity) {
        // 扩展内存容量
        capacity += INITIAL_CAPACITY;
        entries = (FormatEntry*)realloc(entries, sizeof(FormatEntry) * capacity);
        if (entries == NULL) {
            perror("Memory reallocation failed");
            exit(1);
        }
    }

    entries[size].format = format;
    entries[size].count = 1;
    size++;
    pthread_mutex_unlock(&mutex);
}

// 检查并输出过滤后的打印
void checkAndPrintFiltered(int signum) {
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    long elapsedMilliseconds = (currentTime.tv_sec - lastPrintTime.tv_sec) * 1000 + (currentTime.tv_usec - lastPrintTime.tv_usec) / 1000;
    if (elapsedMilliseconds >= MONITOR_INTERVAL_MILLISECONDS) {
        pthread_mutex_lock(&mutex);
        printf("========== Filtered Prints ==========\n");
        for (int i = 0; i < size; i++) {
            if (entries[i].count > 1) {
                printf("%s: %d times\n", entries[i].format, entries[i].count);
            }
        }
        printf("=====================================\n");
        size = 0; // 重置存储的条目数量
        lastPrintTime = currentTime;
        pthread_mutex_unlock(&mutex);
    }
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

int main() {
    // 初始化entries数组
    initializeEntries();

    // 重定向printf函数
    gettimeofday(&lastPrintTime, NULL);
    printf("========== Custom printf monitoring started ==========\n");
    printf("Use myPrintf() for monitored printing.\n");

    // 使用myPrintf替代printf
    while (1) {
        myPrintf("This is a monitored print\n");
        myPrintf("This is another monitored print\n");
        myPrintf("This is a monitored print\n");
        myPrintf("This is a monitored print\n");
        usleep(100); // 小睡一段时间以降低CPU使用率
    }

    // 设置定时器，每500毫秒调用一次checkAndPrintFiltered
    struct itimerval timer;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = MONITOR_INTERVAL_MILLISECONDS * 1000;
    timer.it_interval = timer.it_value;
    setitimer(ITIMER_REAL, &timer, NULL);

    // 释放内存
    free(entries);

    // 恢复printf函数
    printf("========== Monitoring ended ==========\n");

    return 0;
}