#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>

#define LOG_BUFFER_SIZE 512           // 打印信息缓冲池大小
#define MAX_MODULE_NAME_LEN 32        // 模块名最大长度
#define BOOT_LOG_PATH "/home/boot.log"// 日志文件路径
#define LOG_FLUSH_THRESHOLD 200       // 缓冲池自动写入文件的阈值

#define min(x, y) ((x) < (y) ? (x) : (y))

// 定义打印信息结构体
struct log_message {
    char *message;                  // 要记录的打印信息
    struct log_message* next;       // 下一条打印信息
};

// 模块链表节点
struct module_node {
    char name[MAX_MODULE_NAME_LEN];         // 模块名称
    char *dep_info;                         // 模块依赖信息字符串
    time_t first_log_time, last_log_time;   // 模块第一条打印时间和最后一条打印时间
    struct module_node *next;               // 下一个模块节点
};

static struct module_node *module_list_head = NULL; // 模块链表头
static struct log_message *log_msg_buffer = NULL;   // 打印信息缓冲池
static unsigned int log_msg_count = 0;             // 打印信息数量计数器
static unsigned int log_buffer_size = LOG_BUFFER_SIZE;   // 打印信息缓存池大小
static char *boot_log_path = BOOT_LOG_PATH;              // 日志文件路径
static unsigned int log_flush_threshold = LOG_FLUSH_THRESHOLD;  // 缓冲区日志数目达到该值后触发自动写入

// 刷新并写入所有日志
void flush_boot_log() {
    if (log_msg_count == 0) {
        return;
    }

    // 打开或创建日志文件
    FILE *fp = fopen(boot_log_path, "a");
    if (fp == NULL) {
        return;
    }

    // 遍历所有打印信息，按照规定格式写入日志文件，并释放相关内存空间
    struct log_message *tmp_msg = log_msg_buffer;
    while (tmp_msg != NULL) {
        fwrite(tmp_msg->message, strlen(tmp_msg->message), 1, fp);
        fputc('\n', fp);
        struct log_message *next = tmp_msg->next;
        free(tmp_msg->message);
        free(tmp_msg);
        tmp_msg = next;
    }

    // 重置缓冲池数据
    log_msg_buffer = NULL;
    log_msg_count = 0;

    fclose(fp);
}

// 打印信息记录接口，将所有打印先记录到缓存中
void module_boot_log(const char *module_name, const char *dependency, const char *format, ...) {
    if (log_msg_buffer == NULL) {
        return;
    }

    // 记录当前系统时间
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long now = tv.tv_sec;

    // 根据传入的格式化字符串构造打印信息
    va_list args;
    va_start(args, format);
    char *msg = (char *)calloc(log_buffer_size, sizeof(char));
    int len = snprintf(msg, log_buffer_size,
                       "[%ld][%s][%.3f]",
                       now, module_name, log_msg_count == 0 ? 0.0 : 1.0 * (now - module_list_head->first_log_time));
    len += vsnprintf(msg + len, log_buffer_size - len, format, args);

    // 构造完整的打印信息结构体，插入到缓冲池末尾
    struct log_message *new_log_msg = (struct log_message *)malloc(sizeof(struct log_message));
    new_log_msg->message = msg;
    new_log_msg->next = NULL;
    if (log_msg_buffer == NULL) {
        log_msg_buffer = new_log_msg;
    } else {
        struct log_message *tmp = log_msg_buffer;
        while (tmp->next != NULL) {
            tmp = tmp->next;
        }
        tmp->next = new_log_msg;
    }
    log_msg_count++;

    // 如果打印信息数量达到自动刷新阈值，则自动将缓冲池数据写入文件
    if (log_msg_count >= log_flush_threshold) {
        flush_boot_log();
    }

    va_end(args);

    // 更新/新增模块状态信息
    struct module_node *cur_module = module_list_head;
    struct module_node *prev_module = NULL;
    while (cur_module != NULL && strcmp(cur_module->name, module_name) != 0) {
        prev_module = cur_module;
        cur_module = cur_module->next;
    }
    if (cur_module == NULL) {
        cur_module = (struct module_node *)malloc(sizeof(struct module_node));
        memset(cur_module, 0, sizeof(struct module_node));
        memcpy(cur_module->name, module_name, strlen(module_name) + 1);
        if (prev_module == NULL) {
            module_list_head = cur_module;
        } else {
            prev_module->next = cur_module;
        }
    }
    cur_module->first_log_time = min(cur_module->first_log_time, now);
    cur_module->last_log_time = max(cur_module->last_log_time, now);
    if (cur_module->dep_info != NULL) {
        free(cur_module->dep_info);
    }
    cur_module->dep_info = (char *)calloc(strlen(dependency) + 1, sizeof(char));
    memcpy(cur_module->dep_info, dependency, strlen(dependency));
}

// 初始化启动日志记录组件，设置缓存池大小、日志文件路径和自动写入阈值
void init_boot_log(unsigned int buffer_size, const char *path, unsigned int flush_threshold) {
    log_buffer_size = buffer_size > 0 ? buffer_size : LOG_BUFFER_SIZE;
    boot_log_path = path != NULL ? path : BOOT_LOG_PATH;
    log_flush_threshold = flush_threshold > 0 ? flush_threshold : LOG_FLUSH_THRESHOLD;
}

// 将模块状态信息写入日志文件
void print_module_statistics() {
    if (module_list_head == NULL) {
        return;
    }

    FILE *fp = fopen(boot_log_path, "a");
    if (fp == NULL) {
        return;
    }

    // 遍历每个模块，并将其打印信息写入日志文件中
    struct module_node *cur_module = module_list_head;
    char *buffer = (char *)calloc(log_buffer_size, sizeof(char));
    while (cur_module != NULL) {
        int len = snprintf(buffer, log_buffer_size, "[%s][%ld][%ld][%.3f][%s]\n",
                           cur_module->name, cur_module->first_log_time, cur_module->last_log_time,
                           cur_module->last_log_time - cur_module->first_log_time, cur_module->dep_info);
        fwrite(buffer, len, 1, fp);
        cur_module = cur_module->next;
    }
    fclose(fp);
    free(buffer);
}

int main(int argc, char *argv[]) {
    // 初始化启动日志记录模块
    init_boot_log(300, "/home/log/boot.log", 500);

    // 测试用例
    module_boot_log("module1", "dep info 1", "[INFO] This is a test message from module1");
    module_boot_log("module2", "dep info 2", "[ERROR] Something went wrong in module2");
    module_boot_log("module1", "dep info 1", "[DEBUG] Debug info from module1");

    // 刷新所有读取到的日志信息
    flush_boot_log();

    // 输出所有模块的状态信息
    print_module_statistics();

    return 0;
}
