#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <errno.h>

// 定义常量和宏
#define DEFAULT_POOL_SIZE 5000            // 默认链表池大小
#define MAX_MSG_LEN 512                   // 最长记录消息长度
#define MAX_MODULE_NAME_LEN 32            // 最长模块名长度
#define DEFAULT_LOG_FILE "/home/boot.log" // 默认日志文件路径

typedef struct _logMsgNode
{
    char msg[MAX_MSG_LEN];
    int module_num;
    struct timeval tv;
    struct _logMsgNode *next;
} LogMsgNode;

typedef struct _modulePool
{
    char moduleName[MAX_MODULE_NAME_LEN];
    LogMsgNode *head;      // 链表头指针
    LogMsgNode *tail;      // 链表尾指针
    int count;             // 当前链表池中的节点数量
    int poolSize;          // 当前链表池大小
    pthread_mutex_t mutex; // 互斥锁，防止多线程竞争链表池
} ModulePool;

typedef struct _logSystem
{
    char logFilePath[256];       // 日志文件路径
    ModulePool *pools;           // 模块链表池数组指针
    int poolNum;                 // 当前模块链表池数量
    int maxPoolNum;              // 最大模块链表池数量
    int maxPoolSize;             // 最大模块链表池大小
    pthread_mutex_t mutex;       // 整个系统的互斥锁
    pthread_t *threads;          // 写入日志文件的线程池数组指针
    int maxThreadNum;            // 最大线程池数量
    int threadNum;               // 当前线程池内线程数量
    pthread_mutex_t threadMutex; // 线程池互斥锁
} LogSystem;

// 声明全局变量
static LogSystem sysLog = {DEFAULT_LOG_FILE, NULL, 0, 0, DEFAULT_POOL_SIZE, PTHREAD_MUTEX_INITIALIZER, NULL, 10, 0, PTHREAD_MUTEX_INITIALIZER};

// 辅助函数声明
static int getModulePoolIndex(char *moduleName);
static void freeModulePool(ModulePool *pool);

// 初始化接口，设置最大模块链表池数量和日志文件路径
void init(int maxModulePools, char *logFilePath)
{
    pthread_mutex_lock(&sysLog.mutex);
    if (maxModulePools > 0)
    {
        sysLog.maxPoolNum = maxModulePools;
    }
    if (logFilePath != NULL)
    {
        strncpy(sysLog.logFilePath, logFilePath, sizeof(sysLog.logFilePath) - 1);
    }
    else
    {
        strncpy(sysLog.logFilePath, DEFAULT_LOG_FILE, sizeof(sysLog.logFilePath) - 1);
    }
    if (sysLog.pools == NULL)
    {
        sysLog.pools = (ModulePool *)malloc(sizeof(ModulePool) * sysLog.maxPoolNum);
        memset(sysLog.pools, 0, sizeof(ModulePool) * sysLog.maxPoolNum);
        sysLog.poolNum = 0;
    }
    int i;
    for (i = 0; i < sysLog.poolNum; i++)
    {
        freeModulePool(&(sysLog.pools[i]));
    }
    sysLog.poolNum = 0;
    pthread_mutex_unlock(&sysLog.mutex);
}

// 记录某个模块打印信息，并自动添加格式前缀，并根据模块名选择对应的链表池进行存储
void logMessage(char *moduleName, char *depModuleNameList, char *msg)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    int module_num = getModulePoolIndex(moduleName);
    ModulePool *pool = &(sysLog.pools[module_num]);
    pthread_mutex_lock(&(pool->mutex));
    if (pool->head == NULL)
    {
        pool->head = (LogMsgNode *)malloc(sizeof(LogMsgNode));
        pool->tail = pool->head;
    }
    else
    {
        pool->tail->next = (LogMsgNode *)malloc(sizeof(LogMsgNode));
        pool->tail = pool->tail->next;
    }
    pool->tail->next = NULL;
    strncpy(pool->tail->msg, msg, MAX_MSG_LEN - 1);
    pool->tail->tv.tv_sec = now.tv_sec;
    pool->tail->tv.tv_usec = now.tv_usec;
    pool->count++;
    if (pool->count > pool->poolSize)
    {
        if (sysLog.threadNum < sysLog.maxThreadNum)
        {
            pthread_mutex_lock(&sysLog.threadMutex);
            sysLog.threads = (pthread_t *)realloc(sysLog.threads, (++sysLog.threadNum) * sizeof(pthread_t));
            pthread_create(&sysLog.threads[sysLog.threadNum - 1], NULL, &dumpLogThread, NULL);
            pthread_mutex_unlock(&sysLog.threadMutex);
        }
        else
        {
            dumpLog();
        }
    }
    pthread_mutex_unlock(&(pool->mutex));
}

// 获取某个模块当前的链表池数量索引，并增加新的链表池记录
int getModulePoolIndex(char *moduleName)
{
    int i;
    ModulePool *pool;
    for (i = 0; i < sysLog.poolNum; i++)
    {
        pool = &(sysLog.pools[i]);
        if (!strcmp(pool->moduleName, moduleName))
        {
            return i;
        }
    }
    if (sysLog.poolNum >= sysLog.maxPoolNum)
    {
        return -1;
    }
    memset(&(sysLog.pools[sysLog.poolNum]), 0, sizeof(ModulePool));
    pool = &(sysLog.pools[sysLog.poolNum]);
    snprintf(pool->moduleName, MAX_MODULE_NAME_LEN - 1, "%s", moduleName);
    pthread_mutex_init(&(pool->mutex), NULL);
    pool->poolSize = sysLog.maxPoolSize;
    sysLog.poolNum++;
    return sysLog.poolNum - 1;
}

// 输出所有模块的第一条打印时间和最后一条打印时间的时间差，以及模块依赖的其它模块名字符串
char *calcModuleTimeCost()
{
    static char result[1024 * 1024] = {0}; // 存放结果字符串，静态内存分配
    struct timeval startTime, endTime;
    char timeDiffStr[32] = {0};
    int maxDepModListLen = 1024;
    char *depModList = (char *)malloc(maxDepModListLen); // 动态内存分配
    memset(result, 0, sizeof(result));                   // 清空结果字符串
    int i, j;
    for (i = 0; i < sysLog.poolNum; i++)
    {
        if (sysLog.pools[i].head == NULL)
            continue;
        gettimeofday(&startTime, NULL); // 开始计算耗时
        LogMsgNode *node = sysLog.pools[i].head;
        while (node != NULL)
        {
            node = node->next;
        }
        gettimeofday(&endTime, NULL); // 计算完毕
        int hour = (int)((endTime.tv_sec - startTime.tv_sec) / 3600);
        int min = (int)(((endTime.tv_sec - startTime.tv_sec) % 3600) / 60);
        int sec = (int)((endTime.tv_sec - startTime.tv_sec) % 60);
        snprintf(timeDiffStr, sizeof(timeDiffStr) - 1, "[%02d:%02d:%02d]", hour, min, sec);
        snprintf(depModList, maxDepModListLen - 1, "Dependence:%s", sysLog.pools[i].moduleName);
        for (j = 0; j < strlen(depModList); j++)
        {
            depModList[j] = toupper(depModList[j]); // 转换为大写字母
        }
        snprintf(result + strlen(result), sizeof(result) - strlen(result) - 1, "%s %ld.%06ld|%s|%-12s|%s\n",
                 timeDiffStr,
                 sysLog.pools[i].head->tv.tv_sec,
                 sysLog.pools[i].head->tv.tv_usec,
                 sysLog.pools[i].moduleName,
                 SOMETIME,
                 depModList);
    }
    free(depModList); // 释放动态内存
    return result;
}

// 内部辅助函数，将一个模块的链表池全部写入文件
static void dumpModuleLog(ModulePool *pool, FILE *file)
{
    struct timeval startTime, endTime;
    gettimeofday(&startTime, NULL);
    LogMsgNode *node = pool->head;
    char timeDiffStr[32] = {0};
    char depModuleNameList[1024] = {0};
    int isFirstNode = 1;
    while (node != NULL)
    {
        if (isFirstNode)
        {
            isFirstNode = 0;
        }
        else
        {
            fprintf(file, "\n"); // 打印两个消息之间的换行符
        }
        // 将消息写入文件
        int hour = (int)(node->tv.tv_sec / 3600);
        int min = (int)((node->tv.tv_sec % 3600) / 60);
        int sec = (int)(node->tv.tv_sec % 60);
        snprintf(timeDiffStr, sizeof(timeDiffStr) - 1, "[%02d:%02d:%02d.%06ld]", hour, min, sec, node->tv.tv_usec);
        // 将依赖模块列表转换成大写
        if (depModuleNameList[0] != '\0')
        {
            for (int j = 0; j < strlen(depModuleNameList); j++)
            {
                depModuleNameList[j] = toupper(depModuleNameList[j]);
            }
        }
        fprintf(file, "%s |%-12s|%-15s|%s", timeDiffStr, pool->moduleName, SOMETIME, depModuleNameList);
        fflush(file);
        fwrite(node->msg, strlen(node->msg), 1, file);
        fflush(file);
        node = node->next;
    }
    gettimeofday(&endTime, NULL);
    fprintf(stdout, "[%s]: dump log of %s to file success, cost %d usec.\n", __func__, pool->moduleName, (int)((endTime.tv_sec - startTime.tv_sec) * 1000000 + (endTime.tv_usec - startTime.tv_usec)));
}

// 将当前所有模块的链表池内的消息全部写入文件并清空链表池
void dumpLog()
{
    FILE *fout = fopen(sysLog.logFilePath, "a+");
    if (fout == NULL)
    {
        fprintf(stderr, "[%s]: cannot open file %s, %s.\n", __func__, sysLog.logFilePath, strerror(errno));
        return;
    }
    int i, writeCount = 0;
    struct timeval startTime, endTime;
    gettimeofday(&startTime, NULL);
    for (i = 0; i < sysLog.poolNum; i++)
    {
        if (sysLog.pools[i].count <= 0)
            continue;
        pthread_mutex_lock(&(sysLog.pools[i].mutex));
        dumpModuleLog(&(sysLog.pools[i]), fout);
        freeModulePool(&(sysLog.pools[i]));
        pthread_mutex_unlock(&(sysLog.pools[i].mutex));
        writeCount += sysLog.pools[i].count;
    }
    fflush(fout);
    fclose(fout);
    gettimeofday(&endTime, NULL);
    fprintf(stdout, "[%s]: dump %d logs to file success, cost %d usec.\n", __func__, writeCount, (int)((endTime.tv_sec - startTime.tv_sec) * 1000000 + (endTime.tv_usec - startTime.tv_usec)));
}

// 内部辅助函数，释放一个模块的链表池内存
static void freeModulePool(ModulePool *pool)
{
    if (pool->head == NULL)
    {
        return;
    }
    LogMsgNode *freeNode = pool->head;
    while (freeNode != NULL)
    {
        LogMsgNode *nextFreeNode = freeNode->next;
        free(freeNode);
        freeNode = nextFreeNode;
    }
    pool->head = NULL;
    pool->tail = NULL;
    pool->count = 0;
}

// 内部辅助函数，从指定字符串中解析出逗号分隔的模块名列表，并返回模块数量
static int parseDepModuleNameList(char *depModuleNameList, char *depModuleNameArr[])
{
    int count = 0;
    char *tok = strtok(depModuleNameList, ",");
    while (tok != NULL && count < MAX_NUM_DEP_MODULES)
    {
        depModuleNameArr[count++] = tok;
        tok = strtok(NULL, ",");
    }
    return count;
}

// 内部辅助函数，从当前时间减去起始时间，并返回结果字符串
static char *getTimeCostStr(struct timeval startTime)
{
    static char result[32] = {0};
    struct timeval now;
    gettimeofday(&now, NULL);
    int usec_diff = now.tv_usec - startTime.tv_usec;
    int sec_diff = now.tv_sec - startTime.tv_sec;
    if (usec_diff < 0)
    {
        usec_diff += 1000000;
        sec_diff--;
    }
    snprintf(result, sizeof(result) - 1, "%d.%06d", sec_diff, usec_diff);
    return result;
}

// 将每个模块的首次打印时间、最新打印时间、以及二者时间差，追加到指定文件中
// void createModuleCostFile(char* filePath){
//    if(filePath == NULL) return;
//    FILE* fout = fopen(filePath, "a+");
//   if(fout == NULL){
//   fprintf(stderr, "[%s]: cannot open file %s, %s.\n", __func__, filePath,

// 输出该模块的所有打印到文件
void createModuleLogFile(char *moduleName)
{
    int module_num = getModulePoolIndex(moduleName);
    ModulePool *pool = &(sysLog.pools[module_num]);
    pthread_mutex_lock(&(pool->mutex));
    char costFilePath[256];
    snprintf(costFilePath, sizeof(costFilePath), "%s.%s", sysLog.logFilePath, moduleName);
    FILE *fp = fopen(costFilePath, "w");
    if (fp == NULL)
    {
        fprintf(stderr, "Failed to open module cost file %s: %s\n", costFilePath, strerror(errno));
        pthread_mutex_unlock(&(pool->mutex));
        return;
    }
    LogMsgNode *node = pool->head;
    while (node != NULL)
    {
        fprintf(fp, "%ld.%06ld %s\n", node->tv.tv_sec, node->tv.tv_usec, node->msg);
        node = node->next;
    }
    fclose(fp);
    pthread_mutex_unlock(&(pool->mutex));
}