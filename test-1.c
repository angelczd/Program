// 定义常量和宏
#define LOG_POOL_SIZE 5000 // 默认链表池大小为5000
#define LOG_FILE_PATH "/home/boot.log" // 默认文件路径为/home/boot.log

typedef struct log_node {
    char *log;
    struct log_node *next;
} LogNode;

typedef struct print_log {  
    LogNode *pool;   // 链表池
    int pool_size;   // 链表池大小
    int len;         // 链表中已经存储的日志数量   
    int threshold;   // 阈值
    pthread_mutex_t mutex;     // 互斥锁，用于避免多线程下的并发问题
    pthread_t save_thread;     // 线程池
    bool stopped;              // 标记是否需要停止磁盘写入线程
    struct tm start_time;      // 模块第一次打印的时间
    struct tm end_time;        // 模块最后一次打印的时间
    char *module_name;         // 模块名字
    char *dependency_list;     // 依赖模块列表，按逗号分隔
} PrintLog;

static const char TIME_FORMAT[] = "[%Y-%m-%d %H:%M:%S]"; // 时间格式

void init_print_log(int pool_size, char *file_path) {
    // 初始化接口，设置链表池大小和文件存储路径
    if (!pool_size) {
        pool_size = LOG_POOL_SIZE;
    }

    if (!file_path) {
        file_path = LOG_FILE_PATH;
    }

    // do something ...
}

void write_to_disk(PrintLog *plog, char *file_path) {
    // 打印接口需要负责将所有打印先记录到内存中，
    // 额外提供另一个接口，接口入参需要传入一个路径，
    // 该接口负责将所有打印全部写入到入参指定的路径的文件中

    // do something ...    
}

void add_log(PrintLog *plog, char *str) {
    // 接口入参还需要传入需要打印的字符串，
    // 接口负责自动添加该格式内容的前缀：
    // [当前系统时间年月日时分秒][模块名][模块耗时]打印字符串内容

    // do something ...    
}

void *save_log_thread(void *arg) {
    // 使用线程池来管理写入磁盘的操作，
    // 当链表池满了后，就将链表池中的日志提交到线程池中执行写入磁盘的操作

    // do something ...    
}

void write_to_pool(PrintLog *plog, char *str) {
    // 使用链表池来存储打印信息，
    // 避免频繁进行内存分配和释放操作；
    // 设置一个阈值，当链表池中的打印数量达到该阈值时，
    // 自动将链表池中的打印信息写入文件；

    // do something ...    
}

void print_timediff() {
    // 记录每个模块的第一条打印的时间和最后一条打印的时间，
    // 并在格式前缀中增加当前模块此刻距离第一条打印的耗时；
    // 提供一个单独接口，分别输出所有模块的第一条打印时间和最后一条打印时间的时间差，
    // 以及模块依赖的其它模块名字符串
    
    // do something ...
}
