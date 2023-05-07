#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <dns_sd.h>

//将rdata解析为IPv4或IPv6地址
char *parse_address(uint16_t rrtype, const void *rdata) {
    char *addr_str = NULL;
    if (rrtype == kDNSServiceType_A) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = 0;
        memcpy(&(addr.sin_addr), rdata, sizeof(struct in_addr));
        addr_str = (char*) malloc(INET_ADDRSTRLEN);
        if (addr_str != NULL) {
            inet_ntop(AF_INET, &(addr.sin_addr), addr_str, INET_ADDRSTRLEN);
        }
    } else if (rrtype == kDNSServiceType_AAAA) {
        struct sockaddr_in6 addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin6_family = AF_INET6;
        addr.sin6_port = 0;
        memcpy(&(addr.sin6_addr), rdata, sizeof(struct in6_addr));
        addr_str = (char*) malloc(INET6_ADDRSTRLEN);
        if (addr_str != NULL) {
            inet_ntop(AF_INET6, &(addr.sin6_addr), addr_str, INET6_ADDRSTRLEN);
        }
    }
    return addr_str;
}

// 用于DNSServiceBrowse的回调函数
void browse_reply(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
                  DNSServiceErrorType errorCode, const char *serviceName, const char *regtype,
                  const char *replyDomain, void *context) {
    // 如果发生错误，则直接返回
    if (errorCode != kDNSServiceErr_NoError) {
        return;
    }

    // 忽略不是_http._tcp服务的回复
    if (strcmp(regtype, "_http._tcp") != 0) {
        return;
    }

    // 解析服务名称中的IP地址、端口号和主机名称
    char addr_str[INET6_ADDRSTRLEN]; // 更正addr_str的类型
    uint16_t port;
    char host_str[256] = { 0 }; // 清空，避免访问未初始化的内存

    //http：// <IP地址>：<端口>.<hostname> .local
    if (sscanf(serviceName, "http://%[^:]:%hu.%[^.].local", addr_str, &port, host_str) != 3) { // 解析服务名称
        return;
    }

    // 打印主机名称、IP地址和端口号
    printf("Found host: %s, IP address: %s, Port: %hu\n", host_str, addr_str, port);

    // 查询IPv4地址和IPv6地址
    DNSServiceQueryRecord(&sdRef, 0, 0, addr_str, kDNSServiceType_A, kDNSServiceClass_IN,
                          query_record_reply, NULL);
    DNSServiceQueryRecord(&sdRef, 0, 0, addr_str, kDNSServiceType_AAAA, kDNSServiceClass_IN,
                          query_record_reply, NULL);
}

// 用于DNSServiceQueryRecord的回调函数
void query_record_reply(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
                         DNSServiceErrorType errorCode, const char *fullname, uint16_t rrtype,
                         uint16_t rrclass, uint16_t rdlen, const void *rdata, uint32_t ttl,
                         void *context) {
    // 只处理A记录和AAAA记录
    if (rrtype != kDNSServiceType_A && rrtype != kDNSServiceType_AAAA) {
        return;
    }

    // 解析地址
    char *addr_str = parse_address(rrtype, rdata);
    if (addr_str == NULL) {
        return;
    }

    // 打印IP地址
    printf("Found IP address: %s\n", addr_str);

    // 释放资源
    free(addr_str);
}

int main(int argc, char **argv) {
    DNSServiceRef sdRef;
    DNSServiceErrorType err;

    // 开始浏览_http._tcp服务
    err = DNSServiceBrowse(&sdRef, 0, 0, "_http._tcp", NULL, browse_reply, NULL);
    if (err != kDNSServiceErr_NoError) {
        printf("DNSServiceBrowse failed: %d\n", err);
        return 1;
    }

    // 进入事件循环
    err = DNSServiceProcessResult(sdRef);
    if (err != kDNSServiceErr_NoError) {
        printf("DNSServiceProcessResult failed: %d\n", err);
        return 1;
    }

    // 释放资源
    DNSServiceRefDeallocate(sdRef);

    return 0;
}
