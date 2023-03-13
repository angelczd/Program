#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define MAX_LOG_MSG_SIZE 1024     // 定义日志信息最大长度为1024
#define MAX_MODULE_NAME_LENGTH 32 // 定义模块名称最大长度为32
#define MAX_OTHER_MODULES 5       // 定义其他模块名称的最大数量为5

// 定义日志实体类型
typedef struct LogEntry
{
    char module_name[MAX_MODULE_NAME_LENGTH];                      // 模块名称
    time_t first_print_time;                                       // 首次打印时间
    time_t last_print_time;                                        // 最新打印时间
    char other_modules[MAX_OTHER_MODULES][MAX_MODULE_NAME_LENGTH]; // 其他模块名称列表
    int other_module_count;                                        // 其他模块名称的数量
    char content[MAX_LOG_MSG_SIZE];                                // 日志内容
} log_entry_t;

// 定义元素类型
typedef struct Element
{
    log_entry_t value;    // 日志实体
    struct Element *next; // 下一个元素指针
} element_t;

static struct
{
    unsigned int size;     // 日志池大小
    unsigned int count;    // 当前日志数量
    element_t *head;       // 队首元素指针
    element_t *tail;       // 队尾元素指针
    pthread_mutex_t mutex; // 互斥锁
} log_pool_stack = {0};

static const char *default_log_file_path = "./mylog.log"; // 默认日志文件路径
static unsigned int log_pool_size = 5000;                 // 默认日志池大小
static unsigned int flush_threshold = 2500;               // 默认写盘阈值
static FILE *log_file = NULL;                             // 日志文件指针

// 初始化日志池
void init_log_pool(unsigned int size, const char *file_path, unsigned int threshold);

// 从日志池中分配一个元素
element_t *allocate_element();
void release_element(element_t *elem);

void log_msg(const char *module_name, const char **other_module_names,
             int other_module_count, const char *format, ...);

double time_interval(time_t t1, time_t t2);

void start_commit_to_disk_thread();

void close_log_file();

void flush_to_file();

int main()
{

    printf("log test start..."); // 测试输出

    init_log_pool(log_pool_size, default_log_file_path, flush_threshold); // 初始化日志池

    start_commit_to_disk_thread(); // 开启后台写盘线程

    log_msg("MAIN", NULL, 0, "Hello, world!\n"); // 打印一条日志

    flush_to_file(); // 刷新至磁盘

    log_pool_release();

    return 0;
}

// 初始化日志池
void init_log_pool(unsigned int size, const char *file_path, unsigned int threshold)
{
    if (log_file != NULL)
        return;

    log_pool_stack.size = size; // 设置日志池大小
    log_pool_stack.count = 0;   // 当前日志数量为0
    log_pool_stack.head = NULL; // 队首元素置空
    log_pool_stack.tail = NULL; // 队尾元素置空
    pthread_mutex_init(&log_pool_stack.mutex, NULL);
    flush_threshold = threshold; // 设置写盘阈值

    // 如果未指定日志文件路径，则使用默认路径
    if (file_path == NULL)
    {
        file_path = default_log_file_path;
    }

    // 打开日志文件，若失败则输出错误信息
    log_file = fopen(file_path, "a");
    if (log_file == NULL)
    {
        printf("Error opening log file '%s': %s\n",
               file_path, strerror(errno));
        return;
    }

    // 对流进行缓冲
    setvbuf(log_file, NULL, _IOLBF, 1024);
}

// 从日志池中分配一个元素
element_t *allocate_element()
{
    element_t *elem;
    if (log_pool_stack.head != NULL)
    {
        elem = log_pool_stack.head;
        log_pool_stack.head = elem->next; // 更新队首指针
    }
    else
    {
        elem = (element_t *)malloc(sizeof(*elem)); // 分配内存
        elem->next = NULL;
    }
    return elem;
}

// 释放元素并回收内存
void release_element(element_t *elem)
{
    elem->next = NULL;
    memset(&elem->value, 0, sizeof(elem->value)); // 将内存清零
    if (log_pool_stack.head == NULL)              // 若队列为空
    {
        log_pool_stack.head = elem; // 新元素作为队首元素
        log_pool_stack.tail = elem; // 新元素作为队尾元素
    }
    else // 队列不为空
    {
        log_pool_stack.tail->next = elem; // 新元素加入队尾
        log_pool_stack.tail = elem;       // 更新队尾指针
    }
}

// 记录日志
void log_msg(const char *module_name, const char **other_module_names,
             int other_module_count, const char *format, ...)
{
    va_list ap;
    char msg[MAX_LOG_MSG_SIZE];

    if (!format || !module_name)
    {
        return
    }
    // 使用可变参数函数构造日志信息
    va_start(ap, format);
    vsnprintf(msg, MAX_LOG_MSG_SIZE, format, ap);
    va_end(ap);

    pthread_mutex_lock(&log_pool_stack.mutex); // 加锁

    element_t *new_elem = allocate_element(); // 分配元素
    if (new_elem == NULL)
    {
        printf("Failed to allocate new log element.\n"); // 分配失败则输出错误信息
        return;
    }

    new_elem->value.first_print_time = time(NULL);                                 // 设置首次打印时间
    strncpy(new_elem->value.module_name, module_name, MAX_MODULE_NAME_LENGTH - 1); // 设置模块名称
    new_elem->value.module_name[MAX_MODULE_NAME_LENGTH - 1] = '\0';                // 不足32位的用'\0'填充
    new_elem->value.last_print_time = new_elem->value.first_print_time;            // 最新打印时间初始值为首次打印时间
    new_elem->value.other_module_count = other_module_count;                       // 设置其他模块名称数量
    if (!other_module_names)
    {
        memcpy(new_elem->value.other_modules, other_module_names,
               sizeof(char *) * other_module_count); // 复制其他模块名称到数组中
    }
    strncpy(new_elem->value.content, msg, MAX_LOG_MSG_SIZE - 1); // 复制日志信息
    new_elem->value.content[MAX_LOG_MSG_SIZE - 1] = '\0';

    if (log_pool_stack.count == log_pool_stack.size) // 日志池已满
    {
        log_msg("LOG_POOL_OVERFLOW", NULL, 0, ""); // 输出溢出日志
    }
    else
    {
        if (log_pool_stack.head == NULL) // 日志池为空
        {
            log_pool_stack.head = new_elem; // 新元素作为队首元素
            log_pool_stack.tail = new_elem; // 新元素作为队尾元素
        }
        else // 日志池非空
        {
            log_pool_stack.tail->next = new_elem; // 新元素加入队尾
            log_pool_stack.tail = new_elem;       // 更新队尾指针
        }
        log_pool_stack.count++; // 更新日志数量
    }

    pthread_mutex_unlock(&log_pool_stack.mutex); // 释放锁

    if (log_pool_stack.count >= flush_threshold) // 超过写盘阈值
    {
        flush_to_file(); // 刷新至磁盘
    }
}

// 开启后台写盘线程
void start_commit_to_disk_thread()
{
    pthread_t tid;
    pthread_create(&tid, NULL, (void *(*)(void *)) & flush_to_file, NULL); // 创建线程
}

// 将所有未写入日志文件的数据刷新至磁盘
void flush_to_file()
{
    pthread_mutex_lock(&log_pool_stack.mutex); // 加锁

    if (log_pool_stack.count == 0 || log_file == NULL) // 如果日志池为空或未打开日志文件，则直接返回
    {
        pthread_mutex_unlock(&log_pool_stack.mutex);
        return;
    }

    element_t *element = log_pool_stack.head;
    while (element != NULL)
    {

        char formatted_time[25];
        strftime(formatted_time, sizeof(formatted_time), "%F %T",
                 localtime(&(element->value.first_print_time))); // 格式化时间

        fprintf(log_file, "[%s][%s][%.2lf] %s\n",
                formatted_time,
                element->value.module_name,
                difftime(element->value.last_print_time,
                         element->value.first_print_time), // 计算两次写入磁盘的时间间隔
                element->value.content);                   // 写入日志内容

        fflush(log_file); // 刷新流

        element_t *prev = element;
        element = element->next;
        release_element(prev); // 释放元素并回收内存

        log_pool_stack.count--; // 减少日志数量
    }

    pthread_mutex_unlock(&log_pool_stack.mutex); // 释放锁
}

// 关闭日志文件
void close_log_file()
{
    if (log_file != NULL)
    {
        fclose(log_file);
        log_file = NULL;
    }
}

//程序结束释放资源
void log_pool_release()
{
    close_log_file();
    pthread_mutex_destroy(&log_pool_stack.mutex);
    element_t *element = log_pool_stack.head;
    while (element != NULL)
    {
        element_t *prev = element;
        element = element->next;
        free(prev);
    }
}

//输出所有模块的首条打印时间和最后一条打印时间差，重复的模块只输出一次
void print_module_time()
{
    pthread_mutex_lock(&log_pool_stack.mutex); // 加锁

    if (log_pool_stack.count == 0 || log_file == NULL) // 如果日志池为空或未打开日志文件，则直接返回
    {
        pthread_mutex_unlock(&log_pool_stack.mutex);
        return;
    }

    element_t *element = log_pool_stack.head;
    while (element != NULL)
    {
        char formatted_time[25];
        strftime(formatted_time, sizeof(formatted_time), "%F %T",
                 localtime(&(element->value.first_print_time))); // 格式化时间

        fprintf(log_file, "[%s][%s][%.2lf] %s\n",
                formatted_time,
                element->value.module_name,
                difftime(element->value.last_print_time,
                         element->value.first_print_time), // 计算两次写入磁盘的时间间隔
                element->value.content);                   // 写入日志内容

        fflush(log_file); // 刷新流

        element_t *prev = element;
        element = element->next;
        release_element(prev); // 释放元素并回收内存

        log_pool_stack.count--; // 减少日志数量
    }

    pthread_mutex_unlock(&log_pool_stack.mutex); // 释放锁
}