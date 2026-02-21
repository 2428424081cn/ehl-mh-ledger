#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <Node_IP>\n", argv[0]);
        return 1;
    }

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8888);
    inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

    uint8_t packet[32];
    memset(packet, 0xEE, 32); // 填充模拟哈希

    printf("EHL-MH Spammer Active. Target: %s\n", argv[1]);
    
    uint64_t count = 0;
    while(1) {
        sendto(sockfd, packet, 32, 0, (const struct sockaddr *)&servaddr, sizeof(servaddr));
        count++;
        if (count % 100000 == 0) {
            printf("Sent %lu batches...\n", count);
        }
        // 极致性能测试时不加 usleep
    }
    return 0;
}

