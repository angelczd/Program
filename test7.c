#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_MODULE_LEN 64
#define MAX_LOG_LEN 512
#define DEFAULT_POOL_SIZE 300
#define DEFAULT_LOG_PATH "/home/boot.log"
#define DEFAULT_THRESHOLD 200

struct LogNode {
    char module[MAX_MODULE_LEN];
    time_t first;
    time_t last;
    char *dependence;
};

struct LogMessage {
    time_t timestamp;
    char module[MAX_MODULE_LEN];
    int seconds_from_first;
    char log[MAX_LOG_LEN];
};

struct LogPoolNode {
    struct LogMessage data;
    struct LogPoolNode *next;
};

static struct LogPoolNode *g_pool_head = NULL;
static struct LogNode *g_log_list_head = NULL;

static size_t g_pool_size;
static char *g_log_path;
static size_t g_threshold;

// 初始化接口
void init_logger(size_t pool_size, const char *log_path, size_t threshold) {
    if (pool_size == 0) {
        g_pool_size = DEFAULT_POOL_SIZE;
    } else {
        g_pool_size = pool_size;
    }
    if (log_path == NULL) {
        g_log_path = DEFAULT_LOG_PATH;
    } else {
        g_log_path = log_path;
    }
    if (threshold == 0) {
        g_threshold = DEFAULT_THRESHOLD;
    } else {
        g_threshold = threshold;
    }
}

// 打印日志
void log_message(const char *module, const char *dependence,
                 const char *format, ...) {
    struct LogMessage message;

    // 获取当前时间戳
    message.timestamp = time(NULL);

    // 拼接打印字符串
    va_list args;
    va_start(args, format);
    vsnprintf(message.log, MAX_LOG_LEN - 1, format, args);
    va_end(args);

    // 判断模块是否已经存在，若不存在则添加到模块列表中
    struct LogNode *current_node = g_log_list_head;
    while (current_node != NULL) {
        if (strncmp(current_node->module, module, MAX_MODULE_LEN) == 0) {
            break;
        }
        current_node = current_node->next;
    }
    if (current_node == NULL) {
        current_node = (struct LogNode *) malloc(sizeof(struct LogNode));
        strncpy(current_node->module, module, MAX_MODULE_LEN);
        current_node->first = message.timestamp;
        current_node->last = message.timestamp;
        size_t dependence_len = strlen(dependence) + 1;
        current_node->dependence = (char *) malloc(dependence_len);
        strcpy(current_node->dependence, dependence);
        current_node->next = g_log_list_head;
        g_log_list_head = current_node;
    } else {
        current_node->last = message.timestamp;
    }

    // 计算本次打印距离该模块第一句打印的秒数
    message.seconds_from_first = difftime(message.timestamp, current_node->first);

    // 拷贝模块名称
    strncpy(message.module, module, MAX_MODULE_LEN);

    // 将消息加入链表池
    struct LogPoolNode *new_node = (struct LogPoolNode *) malloc(sizeof(struct LogPoolNode));
    new_node->next = NULL;
    new_node->data = message;

    struct LogPoolNode *end_node = g_pool_head;
    if (end_node == NULL) {
        g_pool_head = new_node;
    } else {
        while (end_node->next != NULL) {
            end_node = end_node->next;
        }
        end_node->next = new_node;
    }

    // 如果链表池达到阈值，则将缓存中的全部写入文件
    if (g_pool_size >= g_threshold) {
        FILE *fp = fopen(g_log_path, "a");
        if (fp == NULL) {
            printf("open file failed.\n");
            return;
        }
        struct LogPoolNode *current_node = g_pool_head;
        while (current_node != NULL) {
            fprintf(fp, "[%ld][%s][%d] %s\n", current_node->data.timestamp, current_node->data.module,
                    current_node->data.seconds_from_first, current_node->data.log);
            struct LogPoolNode *next_node = current_node->next;
            free(current_node);
            current_node = next_node;
        }
        fclose(fp);
        g_pool_head = NULL;
        g_pool_size = 0;
    } else {
        g_pool_size++;
    }
}

// 将日志中所有的模块信息打印到文件中
void print_log_list_to_file() {
    FILE *fp = fopen(g_log_path, "a");
    if (fp == NULL) {
        printf("open file failed.\n");
        return;
    }
    struct LogNode *current_node = g_log_list_head;
    while (current_node != NULL) {
        struct tm *first_time_local = localtime(&current_node->first);
        char first_time_buffer[32];
        strftime(first_time_buffer, sizeof(first_time_buffer), "%Y-%m-%d %H:%M:%S", first_time_local);

        struct tm *last_time_local = localtime(&current_node->last);
        char last_time_buffer[32];
        strftime(last_time_buffer, sizeof(last_time_buffer), "%Y-%m-%d %H:%M:%S", last_time_local);

        int diff_seconds = difftime(current_node->last, current_node->first);

        fprintf(fp, "[%s][%s][%s][%d]%s\n", current_node->module, first_time_buffer,
                last_time_buffer, diff_seconds, current_node->dependence);

        current_node = current_node->next;
    }
    fclose(fp);
}
