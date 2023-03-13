#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#define MAX_MODULE_LEN 32
#define MAX_DEPENDENCE_LEN 64
#define MAX_LOG_LEN 512
#define DEFAULT_POOL_SIZE 300
#define DEFAULT_LOG_PATH "./boot.log"
#define DEFAULT_THRESHOLD 200

typedef struct LogNode
{
    char module[MAX_MODULE_LEN];
    time_t first;
    time_t last;
    char dependence[MAX_DEPENDENCE_LEN];
    struct LogNode *next;
} LogNode;

typedef struct LogMessage
{
    time_t timestamp;
    char module[MAX_MODULE_LEN];
    int seconds_from_first;
    char log[MAX_LOG_LEN];
} LogMessage;

typedef struct LogPoolNode
{
    LogMessage data;
    struct LogPoolNode *next;
} LogPoolNode;

static LogPoolNode *g_pool_head = NULL;
static LogNode *g_log_list_head = NULL;
static size_t g_log_pool_size = 0;
static char *g_log_path = NULL;
static size_t g_threshold = 0;

// 初始化接口
void init_logger(size_t max_log_pool_size, const char *log_path, size_t threshold)
{
    g_log_pool_size = (max_log_pool_size == 0) ? DEFAULT_POOL_SIZE : max_log_pool_size;
    g_log_path = (log_path == NULL) ? strdup(DEFAULT_LOG_PATH) : strdup(log_path);
    g_threshold = (threshold == 0) ? DEFAULT_THRESHOLD : threshold;

    if (g_log_list_head != NULL)
    {
        free(g_log_list_head);
        g_log_list_head = NULL;
    }
}

// 打印日志
void log_message(const char *module, const char *dependence, const char *format, ...)
{

    // 获取当前时间戳
    const time_t timestamp = time(NULL);

    // 拼接打印字符串
    char log[MAX_LOG_LEN] = {0};
    va_list args;
    va_start(args, format);
    vsnprintf(log, MAX_LOG_LEN - 1, format, args);
    va_end(args);

    // 判断模块是否已经存在，若不存在则添加到模块列表中
    LogNode *current_node = g_log_list_head;
    while (current_node != NULL)
    {
        if (strncmp(current_node->module, module, MAX_MODULE_LEN) == 0)
        {
            break;
        }
        current_node = current_node->next;
    }
    if (current_node == NULL)
    {
        current_node = (LogNode *)calloc(1, sizeof(LogNode));
        if (current_node == NULL)
        {
            printf("memory allocation error\n");
            return;
        }
        strncpy(current_node->module, module, MAX_MODULE_LEN);
        current_node->first = timestamp;
        current_node->last = timestamp;
        strncpy(current_node->dependence, dependence, MAX_DEPENDENCE_LEN);
        current_node->next = g_log_list_head;
        g_log_list_head = current_node;
    }
    else
    {
        current_node->last = timestamp;
    }

    // 计算本次打印距离该模块第一句打印的秒数
    int seconds_from_first = difftime(timestamp, current_node->first);

    // 将消息加入链表池
    LogPoolNode *new_node = (LogPoolNode *)calloc(1, sizeof(LogPoolNode));
    if (new_node == NULL)
    {
        printf("failed to allocate memory!\n");
        return;
    }
    new_node->next = NULL;
    snprintf(new_node->data.module, MAX_MODULE_LEN - 1, "%s", module);
    new_node->data.timestamp = timestamp;
    new_node->data.seconds_from_first = seconds_from_first;
    snprintf(new_node->data.log, MAX_LOG_LEN - 1, "%s", log);

    LogPoolNode *end_node = g_pool_head;
    if (end_node == NULL)
    {
        g_pool_head = new_node;
    }
    else
    {
        while (end_node->next != NULL)
        {
            end_node = end_node->next;
        }
        end_node->next = new_node;
    }

    // 如果链表池达到阈值，则将缓存中的全部写入文件
    if (++g_log_pool_size >= g_threshold)
    {
        FILE *fp = fopen(g_log_path, "a+");
        if (fp == NULL)
        {
            printf("open file failed.\n");
            return;
        }
        LogNode *current_node = g_log_list_head;
        while (current_node != NULL)
        {
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
        g_pool_head = NULL;
        g_log_pool_size = 0;
    }
}

// 将日志中所有的模块信息打印到文件中
void print_module_info_to_file()
{
    FILE *fp = fopen(g_log_path, "a+");
    if (fp == NULL)
    {
        printf("open file failed.\n");
        return;
    }

    while (g_pool_head != NULL)
    {
        fprintf(fp, "[%ld][%s][%d] %s\n",
                g_pool_head->data.timestamp, g_pool_head->data.module,
                g_pool_head->data.seconds_from_first, g_pool_head->data.log);
        LogPoolNode *current_node = g_pool_head;
        g_pool_head = g_pool_head->next;
        free(current_node);
    }
    fclose(fp);
}

// 释放内存，清空链表
void cleanup_logger()
{
    while (g_log_list_head != NULL)
    {
        LogNode *tmp = g_log_list_head;
        g_log_list_head = g_log_list_head->next;
        free(tmp);
    }
    free(g_log_path);

    while (g_pool_head != NULL)
    {
        LogPoolNode *tmp = g_pool_head;
        g_pool_head = g_pool_head->next;
        free(tmp);
    }
}

int main()
{
    init_logger(1024, NULL, 1024);
    log_message("TestModule", "TestDependencies", "This is a test\n");
    print_module_info_to_file();
    cleanup_logger();
    return 0;
}