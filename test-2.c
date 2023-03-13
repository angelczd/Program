#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define MAX_MODULE_NAME_LEN 32 // 模块名最大长度
#define MAX_PRINT_MSG_LEN 512  // 打印消息最大长度

typedef struct log_node_s
{
    char mod_name[MAX_MODULE_NAME_LEN]; // 模块名
    char print_msg[MAX_PRINT_MSG_LEN];  // 打印消息内容
    unsigned int beg_time;              // 第一条打印时间
    unsigned int end_time;              // 最后一条打印时间
    struct log_node_s *next;            // 链表指针
} log_node_t;

typedef struct log_pool_s
{
    pthread_mutex_t pool_lock;  // 互斥锁，避免多线程并发问题
    unsigned int mod_num;       // 记录模块数量
    unsigned int print_num;     // 记录当前链表中打印数量
    unsigned int max_print_num; // 最大打印数量阈值
    log_node_t *head;           // 链表头结点
    FILE *fp;                   // 日志文件句柄
    char log_filename[256];     // 日志文件路径名
} log_pool_t;

static log_pool_t *g_mod_pool; // 全局日志池指针

/* 初始化日志池 */
int init_log_pool(unsigned int pool_size, char *filename)
{
    if (pool_size == 0 || filename == NULL)
    {
        return -1;
    }
    g_mod_pool = (log_pool_t *)malloc(sizeof(log_pool_t)); // 动态申请空间
    if (g_mod_pool == NULL)
    {
        return -1;
    }
    memset(g_mod_pool, 0, sizeof(log_pool_t));       // 初始化、清零
        g_mod_pool->max_print_num = pool_size;        // 设置阈值大小
    strncpy(g_mod_pool->log_filename, filename, 255); // 设置文件路径名
    g_mod_pool->head = (log_node_t *)malloc(sizeof(log_node_t));
    if (g_mod_pool->head == NULL)
    {
        free(g_mod_pool);
        return -1;
    }
    memset(g_mod_pool->head, 0, sizeof(log_node_t));

    pthread_mutex_init(&g_mod_pool->pool_lock, NULL);
    g_mod_pool->fp = fopen(g_mod_pool->log_filename, "a+");
    if (g_mod_pool->fp == NULL)
    {
        free(g_mod_pool->head);
        free(g_mod_pool);
        return -1;
    }

    return 0;
}

/* 打印信息函数，传入参数str为需要写入日志的字符串 */
void write_to_log(char *mod_name, char *dep_mod_names, char *str)
{
    log_node_t *node = NULL, *pos = NULL;
    char print_buff[MAX_PRINT_MSG_LEN + 128];
    time_t now;
    struct tm tm_now;
    unsigned int cur_sec;

    /* 获取当前业务秒级别时间 */
    now = time(NULL);
    localtime_r(&now, &tm_now);
    cur_sec = (unsigned int)now;

    snprintf(print_buff, MAX_PRINT_MSG_LEN + 128, "[%04d-%02d-%02d %02d:%02d:%02d][%s][%s] %s\n",
             tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
             tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec, mod_name, dep_mod_names, str);

    node = (log_node_t *)malloc(sizeof(log_node_t));

    node->beg_time = cur_sec;
    node->end_time = cur_sec;
    strncpy(node->print_msg, print_buff, MAX_PRINT_MSG_LEN);

    pthread_mutex_lock(&g_mod_pool->pool_lock);
    pos = g_mod_pool->head;
    while (pos->next != NULL)
    {
        if (strncmp(pos->next->mod_name, mod_name, MAX_MODULE_NAME_LEN) == 0)
        {
            /* 该模块已经存在，直接添加到链表尾部即可 */
            pos->next->end_time = cur_sec;
            strncpy(pos->next->print_msg, node->print_msg, MAX_PRINT_MSG_LEN);
            free(node);
            break;
        }
        pos = pos->next;
    }
    if (pos->next == NULL)
    {
        /* 该模块还不存在，需要新建模块结点 */
        strncpy(node->mod_name, mod_name, MAX_MODULE_NAME_LEN);
        pos->next = node;
        g_mod_pool->mod_num++;
    }
    g_mod_pool->print_num++;
    /* 若链表中打印的数量达到阈值，则将链表中的所有结点写入到日志文件中 */
    if (g_mod_pool->print_num >= g_mod_pool->max_print_num)
    {
        pos = g_mod_pool->head->next;
        while (pos != NULL)
        {
            fwrite(pos, 1, sizeof(log_node_t), g_mod_pool->fp);
            free(pos);
            pos = pos->next;
        }
        g_mod_pool->head->next = NULL;
        g_mod_pool->print_num = 0;
    }
    pthread_mutex_unlock(&g_mod_pool->pool_lock);
}

/* 将内存中的日志信息写入文件 */
void dump_log(char *filename)
{
    log_node_t *pos = NULL;

    if (g_mod_pool == NULL || g_mod_pool->head == NULL)
    {
        return;
    }

    if (filename != NULL)
    {
        g_mod_pool->fp = fopen(filename, "a+");
    }
    
    pos = g_mod_pool->head->next;
    while (pos != NULL && pos->next != NULL)
    {
        fwrite(pos, 1, sizeof(log_node_t), g_mod_pool->fp);
        pos = pos->next;
        free(pos); // Move free to last step to avoid breaking traversal 
    }
    
    g_mod_pool->head->next = NULL;
    g_mod_pool->print_num = 0;
    
    fclose(g_mod_pool->fp); // Close the pointer to file
}


/* 在程序退出时释放日志池结构体及其它资源 */
void release_log_pool()
{
    dump_log(NULL);
    fclose(g_mod_pool->fp);

    pthread_mutex_destroy(&g_mod_pool->pool_lock);
    free(g_mod_pool->head);
    free(g_mod_pool);
}
 