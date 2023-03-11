#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_MODULE_NAME_LEN 20
#define MAX_LOG_LEN 1024
#define MAX_TIME_STR_LEN 32
#define LOG_POOL_SIZE 5000
#define TIME_DIFF_INTERVAL 30

// 打印日志的模块名称和等级
struct log_meta
{
    char module[MAX_MODULE_NAME_LEN]; // 模块名称
    int level;                        // 等级
};

// 打印日志的内容和时间
struct log_entry
{
    time_t time;           // 打印时间
    char log[MAX_LOG_LEN]; // 日志内容
};

// 日志链表节点
struct log_node
{
    struct log_entry entry;
    struct log_node *next;
};

// 链表池
struct log_pool
{
    struct log_node *head;
    struct log_node *tail;
    int size;
    int capacity;
};

// 日志信息
struct log_info
{
    char log_dir[256];               // 日志文件存储路径
    struct log_pool pool;            // 日志链表池
    FILE *log_file;                  // 日志文件
    struct log_node *time_diff_list; // 保存时间差信息的链表
    pthread_mutex_t mutex;           // 互斥锁
    bool initialized;                // 是否已经初始化
};

// 全局变量
static struct log_info log_info = {
    .log_dir = "/home/boot.log",
    .pool = {.head = NULL, .tail = NULL, .size = 0, .capacity = LOG_POOL_SIZE},
    .log_file = NULL,
    .time_diff_list = NULL,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .initialized = false};

/**
 * 获取当前时间的字符串形式
 */
static void get_current_time_str(char *time_str)
{
    time_t now = time(NULL);
    strftime(time_str, MAX_TIME_STR_LEN, "%Y-%m-%d %H:%M:%S", localtime(&now));
}

/**
 * 打印日志信息到标准输出
 */
static void print_log_to_stdout(const struct log_meta *meta, const struct log_entry *entry)
{
    char time_str[MAX_TIME_STR_LEN] = {0};
    get_current_time_str(time_str);
    printf("%s [%s] %s: %s\n", time_str, meta->module, meta->level == 1 ? "INFO" : "ERROR", entry->log);
}

/**
 * 初始化日志链表池
 */
static void init_log_pool(struct log_pool *pool)
{
    pool->head = NULL;
    pool->tail = NULL;
    pool->size = 0;
}

/**
 * 获取日志链表池中的节点数
 */
static int get_log_pool_size(struct log_pool *pool)
{
    return pool->size;
}

/**
 * 在日志链表池的尾部插入一个节点
 */
static void append_log_node(struct log_pool *pool, struct log_node *node)
{
    if (pool->tail == NULL)
    {
        pool->head = node;
    }
    else
    {
        pool->tail->next = node;
    }
    pool->tail =
