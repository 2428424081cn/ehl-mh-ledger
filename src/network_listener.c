#ifndef NETWORK_LISTENER_H
#define NETWORK_LISTENER_H

#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>

#define UDP_PORT 8888
#define HASH_BATCH_SIZE 32

// 这是一个轻量级的异步接收器
void* start_network_listener(void* arg) {
    int sockfd;
    uint8_t buffer[HASH_BATCH_SIZE];
    struct sockaddr_in servaddr, cliaddr;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) return NULL;

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(UDP_PORT);

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) return NULL;

    while(1) {
        socklen_t len = sizeof(cliaddr);
        // 接收来自客户端的哈希块
        ssize_t n = recvfrom(sockfd, buffer, HASH_BATCH_SIZE, MSG_WAITALL, (struct sockaddr *)&cliaddr, &len);
        if (n == HASH_BATCH_SIZE) {
            // 这里后续会接入 MMR 存储逻辑
            // 目前先保持空转以最大化接收频率测试
        }
    }
    return NULL;
}

#endif

