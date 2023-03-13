#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define MAX_LOG_MSG_SIZE 1024
#define MAX_MODULE_NAME_LENGTH 32
#define MAX_OTHER_MODULES 5

typedef struct LogEntry
{
    char module_name[MAX_MODULE_NAME_LENGTH];
    time_t first_print_time;
    time_t last_print_time;
    char other_modules[MAX_OTHER_MODULES][MAX_MODULE_NAME_LENGTH];
    int other_module_count;
    char content[MAX_LOG_MSG_SIZE];

} log_entry_t;

typedef struct Element
{
    log_entry_t value;
    struct Element *next;
} element_t;

static struct
{
    unsigned int size;
    unsigned int count;
    element_t *head;
    element_t *tail;
    pthread_mutex_t mutex;
} log_pool_stack = {0};

static const char *default_log_file_path = "./mylog.log";
static unsigned int log_pool_size = 5000;
static unsigned int flush_threshold = 2500;
static FILE *log_file = NULL;

void init_log_pool(unsigned int size, const char *file_path, unsigned int threshold);

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

    printf("log test start...");

    init_log_pool(log_pool_size, default_log_file_path, flush_threshold);

    start_commit_to_disk_thread();

    log_msg("MAIN", NULL, 0, "Hello, world!\n");

    flush_to_file();

    return 0;
}

// 初始化日志池
void init_log_pool(unsigned int size, const char *file_path, unsigned int threshold)
{
    if (log_file != NULL)
        return;

    log_pool_stack.size = size;
    log_pool_stack.count = 0;
    log_pool_stack.head = NULL;
    log_pool_stack.tail = NULL;

    flush_threshold = threshold;

    if (file_path == NULL)
    {
        file_path = default_log_file_path;
    }

    log_file = fopen(file_path, "a");
    if (log_file == NULL)
    {
        printf("Error opening log file '%s': %s\n",
               file_path, strerror(errno));
        return;
    }

    setvbuf(log_file, NULL, _IOLBF, 1024);
}

// 从日志池中分配一个元素
element_t *allocate_element()
{
    element_t *elem;
    if (log_pool_stack.head != NULL)
    {
        elem = log_pool_stack.head;
        log_pool_stack.head = elem->next;
    }
    else
    {
        elem = (element_t *)malloc(sizeof(*elem));
        elem->next = NULL;
    }
    return elem;
}

void release_element(element_t *elem)
{
    elem->next = NULL;
    memset(&elem->value, 0, sizeof(elem->value));
    if (log_pool_stack.head == NULL)
    {
        log_pool_stack.head = elem;
        log_pool_stack.tail = elem;
    }
    else
    {
        log_pool_stack.tail->next = elem;
        log_pool_stack.tail = elem;
    }
}

void log_msg(const char *module_name, const char **other_module_names,
             int other_module_count, const char *format, ...)
{
    va_list ap;
    char msg[MAX_LOG_MSG_SIZE];

    va_start(ap, format);
    vsnprintf(msg, MAX_LOG_MSG_SIZE, format, ap);
    va_end(ap);

    pthread_mutex_lock(&log_pool_stack.mutex);

    element_t *new_elem = allocate_element();
    if (new_elem == NULL)
    {
        printf("Failed to allocate new log element.\n");
        return;
    }

    new_elem->value.first_print_time = time(NULL);
    strncpy(new_elem->value.module_name, module_name, MAX_MODULE_NAME_LENGTH - 1);
    new_elem->value.module_name[MAX_MODULE_NAME_LENGTH - 1] = '\0';
    new_elem->value.last_print_time = new_elem->value.first_print_time;
    new_elem->value.other_module_count = other_module_count;
    memcpy(new_elem->value.other_modules, other_module_names,
           sizeof(char *) * other_module_count);
    strncpy(new_elem->value.content, msg, MAX_LOG_MSG_SIZE - 1);
    new_elem->value.content[MAX_LOG_MSG_SIZE - 1] = '\0';

    if (log_pool_stack.count == log_pool_stack.size)
    {
        log_msg("LOG_POOL_OVERFLOW", NULL, 0, "");
    }
    else
    {
        if (log_pool_stack.head == NULL)
        {
            log_pool_stack.head = new_elem;
            log_pool_stack.tail = new_elem;
        }
        else
        {
            log_pool_stack.tail->next = new_elem;
            log_pool_stack.tail = new_elem;
        }
        log_pool_stack.count++;
    }

    pthread_mutex_unlock(&log_pool_stack.mutex);

    if (log_pool_stack.count >= flush_threshold)
    {
        flush_to_file();
    }
}

void start_commit_to_disk_thread()
{
    pthread_t tid;
    pthread_create(&tid, NULL, (void *(*)(void *)) & flush_to_file, NULL);
}

void flush_to_file()
{

    pthread_mutex_lock(&log_pool_stack.mutex);

    if (log_pool_stack.count == 0 || log_file == NULL)
    {
        pthread_mutex_unlock(&log_pool_stack.mutex);
        return;
    }

    element_t *element = log_pool_stack.head;
    while (element != NULL)
    {

        char formatted_time[25];
        strftime(formatted_time, sizeof(formatted_time), "%F %T",
                 localtime(&(element->value.first_print_time)));

        fprintf(log_file, "[%s][%s][%.2lf] %s\n",
                formatted_time,
                element->value.module_name,
                time_interval(element->value.first_print_time,
                              element->value.last_print_time),
                element->value.content);

        fflush(log_file);

        element_t *prev = element;
        element = element->next;
        release_element(prev);

        log_pool_stack.count--;
    }

    pthread_mutex_unlock(&log_pool_stack.mutex);
}

double time_interval(time_t t1, time_t t2)
{
    return difftime(t2, t1);
}

void close_log_file()
{
    if (log_file != NULL)
    {
        fclose(log_file);
        log_file = NULL;
    }
}
