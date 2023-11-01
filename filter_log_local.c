#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>

#define INITIAL_CAPACITY 1
#define MONITOR_INTERVAL_MILLISECONDS 900

// 结构体用于存储format常量字符串及其打印次数
typedef struct FormatEntry {
    const char* format;
    unsigned int count;
} FormatEntry;

typedef struct ThreadData {
    FormatEntry* entries;
    int capacity;
    int size;
    int version;
} ThreadData;

static pthread_key_t threadDataKey; // 线程特定数据键

static int globalVersion = 0; // 全局版本号
static timer_t g_cycle_timer;

// 线程特定数据的析构函数
void deleteThreadData(void* ptr) {
    ThreadData* data = (ThreadData*)ptr;
    free(data->entries);
    free(data);
}

// 向threadEntries数组中添加或更新format常量字符串的打印次数
bool updateFormatEntry(const char* format) {
    ThreadData* data = (ThreadData*)pthread_getspecific(threadDataKey);
    if(NULL == data)
    {
        data = (ThreadData*)malloc(sizeof(ThreadData));
        data->entries = (FormatEntry*)malloc(sizeof(FormatEntry) * INITIAL_CAPACITY);
        data->capacity = INITIAL_CAPACITY;
        data->size = 0;
        data->version = 0;
        pthread_setspecific(threadDataKey, data);
    }

    // 检查版本号
    if (data->version != globalVersion) {
        for (int i = 0; i < data->size; i++) {
            if (data->entries[i].count > 1) {
                printf("str_addr[%p]:%u\n", data->entries[i].format, data->entries[i].count);
            }
        }
        data->size = 0;
        data->version = globalVersion;
    }

    // 更新线程局部存储的FormatEntry
    for (int i = 0; i < data->size; i++) {
        if (data->entries[i].format == format) {
            data->entries[i].count++;
            return true;
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
    return false;
}

// 检查并输出过滤后的打印
void checkAndPrintFiltered() {
    globalVersion++; // 增加全局版本号
}

// 重定义printf函数，实现监测和过滤
int myPrintf(const char* format, ...) {
    if(updateFormatEntry(format))
    {
        return 0;
    }

    va_list args;
    va_start(args, format);
    int result = vprintf(format, args);
    va_end(args);

    return result;
}

void* threadFunc(void* arg) {

    while (1) {
        myPrintf("This is a monitored print from thread %ld\n", (long)arg);
        usleep(1000); // 小睡一段时间以降低CPU使用率
    }
    return NULL;
}

void timer_callback(union sigval sv) {
    printf("Timer %d triggered!\n", sv.sival_int);
    switch (sv.sival_int) {
        case 1:
            checkAndPrintFiltered();
            break;
        default:
            break;
    }
}
void create_clearrecord_timer()
{
    // 创建并设置定时器

    struct sigevent sev;
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = timer_callback;
    sev.sigev_notify_attributes = NULL;
    sev.sigev_value.sival_int = 1;
    timer_create(CLOCK_REALTIME, &sev, &g_cycle_timer);

    struct itimerspec its;
    its.it_value.tv_sec = 1;
    its.it_value.tv_nsec = 0;
    its.it_interval = its.it_value;
    timer_settime(g_cycle_timer, 0, &its, NULL);

    printf("==========timer_settime ended ==========\n");
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

    create_clearrecord_timer();

    // 等待所有线程结束
    for (int i = 0; i < 10; i++) {
        pthread_join(threads[i], NULL);
    }

    timer_delete(g_cycle_timer);

    // 恢复printf函数
    printf("========== Monitoring ended ==========\n");

    return 0;
}