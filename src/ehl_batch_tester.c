#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

#define MAX_BATCH 10000

// 记录成功存入的数据：它的标记(tag)和分配的物理坑位(idx)
typedef struct {
    uint64_t tag;
    uint64_t idx;
} Record;

Record records[MAX_BATCH];
int success_count = 0;

void generate_mock_hash(uint64_t data_tag, uint8_t *out) {
    memset(out, 0, 32);
    snprintf((char*)out, 32, "EHL-DATA-%08lu", data_tag); 
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("用法: %s <手机A的IP> <批量测试数量>\n", argv[0]);
        printf("例如: %s 10.62.147.51 100\n", argv[0]);
        return 1;
    }

    char *ip = argv[1];
    int target_count = atoi(argv[2]);
    if (target_count > MAX_BATCH) target_count = MAX_BATCH;

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8888);
    inet_pton(AF_INET, ip, &servaddr.sin_addr);

    // 批量测试时，超时时间设短一点 (200毫秒)
    struct timeval tv = {0, 200000}; 
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    printf("\n\033[36;1m=== 阶段 1: 开始批量上传 %d 个存证 ===\033[0m\n", target_count);
    
    for (int i = 0; i < target_count; i++) {
        uint64_t current_tag = 80000 + i; // 假设数据标记从 80000 开始
        
        uint8_t packet[33];
        packet[0] = 0x01;
        generate_mock_hash(current_tag, packet + 1);
        sendto(sockfd, packet, 33, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));

        uint8_t receipt[9];
        int n = recvfrom(sockfd, receipt, 9, 0, NULL, NULL);
        if (n == 9 && receipt[0] == 0x05) {
            uint64_t real_idx;
            memcpy(&real_idx, receipt + 1, 8);
            
            // 记录到本地账本
            records[success_count].tag = current_tag;
            records[success_count].idx = real_idx;
            success_count++;
            
            if (success_count % 10 == 0) {
                printf("\r已成功落盘: %d / %d", success_count, target_count);
                fflush(stdout);
            }
        }
    }
    printf("\n上传阶段完成！成功率: %.2f%%\n", (float)success_count / target_count * 100.0);

    if (success_count == 0) {
        printf("\033[31m没有数据成功存入，退出测试。\033[0m\n");
        close(sockfd);
        return 1;
    }

    printf("\n\033[36;1m=== 阶段 2: 开始极速交叉验证 ===\033[0m\n");
    int verify_passed = 0;
    
    for (int i = 0; i < success_count; i++) {
        uint64_t query_idx = records[i].idx;
        uint64_t original_tag = records[i].tag;

        uint8_t query[9];
        query[0] = 0x02;
        memcpy(query + 1, &query_idx, 8);
        sendto(sockfd, query, 9, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));

        uint8_t resp[33];
        int n = recvfrom(sockfd, resp, 33, 0, NULL, NULL);
        
        if (n == 33 && resp[0] == 0x03) {
            uint8_t expected[32];
            generate_mock_hash(original_tag, expected);
            
            if (memcmp(resp + 1, expected, 32) == 0) {
                verify_passed++;
            } else {
                printf("\n\033[31m[ 篡改警告 ] 索引 #%lu 数据与标签 #%lu 不符！\033[0m\n", query_idx, original_tag);
            }
        }
        
        if ((i + 1) % 10 == 0) {
            printf("\r已验证: %d / %d", i + 1, success_count);
            fflush(stdout);
        }
    }

    printf("\n\n\033[32;1m=== 批量测试报告 ===\033[0m\n");
    printf("尝试发送 : %d\n", target_count);
    printf("成功落盘 : %d\n", success_count);
    printf("完美验证 : %d\n", verify_passed);
    
    if (verify_passed == success_count) {
        printf("\033[32;1m结论: 系统逻辑 100%% 稳定！你的 EHL-MH 架构坚不可摧！\033[0m\n");
    } else {
        printf("\033[31;1m结论: 存在数据损坏，通过率 %.2f%%\033[0m\n", (float)verify_passed / success_count * 100.0);
    }

    close(sockfd);
    return 0;
}

