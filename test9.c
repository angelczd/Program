#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define MAX_MODULE_NAME_LEN 256
#define MAX_DEPENDENCIES_LEN 1024
#define MAX_LOG_ENTRY_LEN 512
#define MAX_LOG_FILE_PATH_LEN 128
#define DEFAULT_LOG_ENTRIES_CAPACITY 500
#define DEFAULT_LOG_FILE_PATH "./boot.log"
#define DEFAULT_LOG_FLUSH_THRESHOLD 250

typedef struct log_entry
{
    char timestr[20];
    char module_name[MAX_MODULE_NAME_LEN];
    int seconds_from_first_log;
    char log_content[MAX_LOG_ENTRY_LEN];
} log_entry_t;

typedef struct module_info
{
    char module_name[MAX_MODULE_NAME_LEN];
    time_t first_log_time;
    time_t last_log_time;
    char dependencies[MAX_DEPENDENCIES_LEN];
    struct module_info *next;
} module_info_t;

typedef struct log_buffer
{
    log_entry_t *entries;   //日志内容数组
    int capacity;           //日志缓冲区容量
    int count;              //当前存储日志数量(未保存文件的)
    int read_index;         //读取日志的索引
    int write_index;        //写入日志的索引
} log_buffer_t;

//日志缓冲区
static log_buffer_t g_log_buffer = {.entries = NULL, .capacity = 0, .count = 0, .read_index = 0, .write_index = 0};

//模块链表头尾指针
static module_info_t *g_module_list_head = NULL;
static module_info_t *g_module_list_tail = NULL;

//日志文件路径
static char g_log_file_path[MAX_LOG_FILE_PATH_LEN];
//日志刷新阈值
static int g_log_flush_threshold = DEFAULT_LOG_FLUSH_THRESHOLD;

//日志写入线程
static pthread_t g_writer_thread;
//log_buffer写入互斥锁
static pthread_mutex_t g_log_buf_mutex = PTHREAD_MUTEX_INITIALIZER;


int get_seconds_from_first_log(const char *module_name)
{
    module_info_t *p = g_module_list_head;
    while (p != NULL)
    {
        if (strcmp(p->module_name, module_name) == 0)
        {
            return difftime(p->last_log_time, p->first_log_time);
        }
        p = p->next;
    }

    return 0;
}

module_info_t *add_module_info(const char *module_name, const char *dependencies)
{
    module_info_t *m = malloc(sizeof(module_info_t));
    if (m == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for module info.\n");
        return NULL;
    }
    strncpy(m->module_name, module_name, MAX_MODULE_NAME_LEN);
    m->first_log_time = 0;
    m->last_log_time = 0;
    strcpy(m->dependencies, dependencies);
    m->next = NULL;
    if (g_module_list_head == NULL)
    {
        g_module_list_head = m;
    }
    else
    {
        g_module_list_tail->next = m;
    }
    g_module_list_tail = m;
    return m;
}

module_info_t *find_or_add_module_info(const char *module_name, const char *dependencies)
{
    module_info_t *p = g_module_list_head;
    while (p != NULL)
    {
        if (strcmp(p->module_name, module_name) == 0)
        {
            if (strlen(dependencies) > 0)
            {
                // 先判断当前字符串是不是空的，如果为空则不需要输入逗号
                if (strlen(p->dependencies) > 0)
                {
                    strncat(p->dependencies, ",", MAX_DEPENDENCIES_LEN - strlen(p->dependencies) - 1);
                }
                char *tmp_dependencies = strdup(dependencies);
                char *tok = strtok(tmp_dependencies, ",");
                while (tok != NULL)
                {
                    if (strstr(p->dependencies, tok) == NULL)
                    {
                        strncat(p->dependencies, tok, MAX_DEPENDENCIES_LEN - strlen(p->dependencies) - 1);
                    }
                    tok = strtok(NULL, ",");
                }
                free(tmp_dependencies);
            }
            return p;
        }
        p = p->next;
    }
    return add_module_info(module_name, dependencies);
}

int init_log_buffer(int capacity, const char *log_path, int flush_threshold)
{
    //init log file path
    if(NULL != log_path)
    {
        snprintf(g_log_file_path, MAX_LOG_FILE_PATH_LEN, "%s", log_path);
    }
    else
    {
        snprintf(g_log_file_path, MAX_LOG_FILE_PATH_LEN, "%s", DEFAULT_LOG_FILE_PATH);
    }
    //init log flush threshold
    g_log_flush_threshold = flush_threshold <= 0 ? DEFAULT_LOG_FLUSH_THRESHOLD : flush_threshold;

    //init log buffer
    g_log_buffer.entries = (log_entry_t*)malloc(capacity * sizeof(log_entry_t));
    if (g_log_buffer.entries == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for log buffer.\n");
        return -1;
    }
    memset(g_log_buffer.entries, 0, capacity * sizeof(log_entry_t));
    g_log_buffer.capacity = capacity <= 0 ? DEFAULT_LOG_ENTRIES_CAPACITY : capacity;
    g_log_buffer.count = 0;
    g_log_buffer.read_index = 0;
    g_log_buffer.write_index = 0;

    return 0;
}

void print_to_log_buffer(const char *module_name, const char *dependencies, const char *log_content)
{
    time_t current_time = time(NULL);
    current_time = localtime(&current_time);

    // calculate seconds from first log
    int seconds = 0;
    if (g_module_list_head != NULL)
    {
        module_info_t *m = find_or_add_module_info(module_name, dependencies);
        if (m == NULL)
        {
            fprintf(stderr, "Failed to add module info.\n");
            return;
        }

        if (m->first_log_time == 0)
        {
            m->first_log_time = current_time;
            seconds = 0;
        }
        else
        {
            seconds = get_seconds_from_first_log(module_name);
        }
        m->last_log_time = current_time;
    }

    if(g_log_buffer.count < g_log_buffer.capacity)
    {
        g_log_buffer.entries[g_log_buffer.write_index]->seconds_from_first_log = seconds;
        strftime(g_log_buffer.entries[g_log_buffer.write_index]->timestr, 20, "%Y-%m-%d %H:%M:%S", current_time);
        strncpy(g_log_buffer.entries[g_log_buffer.write_index]->log_content, log_content, MAX_LOG_ENTRY_LEN);

        g_log_buffer.write_index = (g_log_buffer.write_index + 1) % g_log_buffer.capacity;
        if (g_log_buffer.count < g_log_buffer.capacity)
        {
            pthread_mutex_lock(&g_log_buf_mutex);
            g_log_buffer.count++;
            pthread_mutex_unlock(&g_log_buf_mutex);
        }
        else
        {
            g_log_buffer.read_index = (g_log_buffer.read_index + 1) % g_log_buffer.capacity;
        }
    }
    else
    {
        fprintf(stderr, "Log buffer is full!\n");
    }
    return;
}

void *writer_thread_func(void *arg)
{
    FILE *log_file = NULL; 

    int count = 0;
    int index = 0;

    log_file =  fopen(g_log_file_path, "a");
    if (log_file == NULL)
    {
        fprintf(stderr, "Failed to open log file: %s\n", g_log_file_path);
        return NULL;
    }

    while (1)
    {
        sleep(1);

        if (g_log_buffer.count >= g_log_flush_threshold)
        {
            pthread_mutex_lock(&g_log_buf_mutex);
            count = g_log_buffer.count;
            index = g_log_buffer.read_index;
            pthread_mutex_unlock(&g_log_buf_mutex);

            for (int i = 0; i < count; i++)
            {
                fprintf(log_file, "[%s][%s][%d]%s\n",
                        g_log_buffer.entries[index]->timestr,
                        g_log_buffer.entries[index]->module_name,
                        g_log_buffer.entries[index]->seconds_from_first_log,
                        g_log_buffer.entries[index]->log_content);

                index = (index + 1) % g_log_buffer.capacity;
            }
            fflush(log_file);
            
            pthread_mutex_lock(&g_log_buf_mutex);
            g_log_buffer.count -= count;
            g_log_buffer.read_index = index;
            pthread_mutex_unlock(&g_log_buf_mutex);
        }
    }

    fclose(log_file);

    return NULL;
}

int start_log_writer_thread()
{
    return pthread_create(&g_writer_thread, NULL, writer_thread_func, NULL);
}

void log_module_list()
{
    module_info_t *p = NULL;
    FILE *log_file = fopen(g_log_file_path, "a");
    if (log_file == NULL)
    {
        fprintf(stderr, "Failed to open log file: %s\n", g_log_file_path);
        return;
    }
    fprintf(log_file,"[%s][%s][%s][%.2lf]%s\n", "module", "first_log_time", "last_log_time", "diff", "dependencies");
    p = g_module_list_head;
    while (p != NULL)
    {
        double diff = difftime(p->last_log_time, p->first_log_time);
        fprintf(log_file, "[%s][%s][%s][%.2lf]%s\n",
                p->module_name,
                asctime(p->first_log_time),
                asctime(p->last_log_time),
                diff,
                p->dependencies);
        p = p->next;
    }
    fclose(log_file);
}

void release_resources()
{
    pthread_cancel(g_writer_thread);
    pthread_join(g_writer_thread, NULL);
    pthread_mutex_destroy(&g_log_buf_mutex);

    for (int i = 0; i < g_log_buffer.capacity; i++)
    {
        if (g_log_buffer.entries[i] != NULL)
        {
            free(g_log_buffer.entries[i]);
        }
    }
    free(g_log_buffer.entries);
    module_info_t *p = g_module_list_head;
    while (p != NULL)
    {
        module_info_t *tmp = p;
        p = p->next;
        free(tmp);
    }
}

int main()
{
    if (init_log_buffer(DEFAULT_LOG_ENTRIES_CAPACITY, DEFAULT_LOG_FILE_PATH, DEFAULT_LOG_ENTRIES_FLUSH_THRESHOLD) != 0)
    {
        fprintf(stderr, "Failed to initialize log buffer.\n");
        exit(EXIT_FAILURE);
    }
    if (start_log_writer_thread() != 0)
    {
        fprintf(stderr, "Failed to start log writer thread.\n");
        exit(EXIT_FAILURE);
    }

    print_to_log_buffer("Module A", "", "This is a log entry of Module A.");
    print_to_log_buffer("Module B", "", "This is a log entry of Module B.");
    print_to_log_buffer("Module A", "", "Another log entry of Module A.");
    print_to_log_buffer("Module C", "", "This is a log entry of Module C.");
    print_to_log_buffer("Module A", "Module B,Module D", "The last log entry of Module A.");

    log_module_list();

    release_resources();
    return 0;
}
