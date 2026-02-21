#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

// --- 神奇的“数数”模拟器 ---
// 将索引(Index)转化为 32 字节的确定性数据
// 例如 index=100 会生成 "EHL-DATA-00000100" 并在后面补零
void generate_mock_hash(uint64_t index, uint8_t *out) {
    memset(out, 0, 32);
    snprintf((char*)out, 32, "EHL-DATA-%08lu", index); 
}

// 辅助打印十六进制
void print_hex(uint8_t *data, int len) {
    for(int i=0; i<len; i++) printf("%02x", data[i]);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("=========================================\n");
        printf(" EHL-MH Dual-Protocol Tester / Auditor\n");
        printf("=========================================\n");
        printf("Usage:\n");
        printf("  1. Auto Pipeline : %s <IP> auto\n", argv[0]);
        printf("  2. Manual Verify : %s <IP> verify <index>\n", argv[0]);
        return 1;
    }

    char *ip = argv[1];
    char *mode = argv[2];

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8888);
    inet_pton(AF_INET, ip, &servaddr.sin_addr);

    // 【模式 1】：自动化流水线 (一边发包，一边自动抽查)
    if (strcmp(mode, "auto") == 0) {
        printf("\033[36m[AUTO] Starting EHL-MH Spammer & Auto-Auditor...\033[0m\n");
        
        // 设置 200ms 的接收超时 (防止等待证明时卡死)
        struct timeval tv = {0, 200000};
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        uint64_t i = 0;
        uint8_t packet[33];
        while(1) {
            // 1. 发送 0x01 存证包
            packet[0] = 0x01;
            generate_mock_hash(i, packet + 1);
            sendto(sockfd, packet, 33, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
            
            // 2. 每 1000 个包，触发一次“突然袭击”
            if (i > 0 && i % 1000 == 0) {
                uint64_t challenge_idx = rand() % i; // 随机抽取一个已经发过的历史索引
                uint8_t query[9];
                query[0] = 0x02; // 0x02 查询请求
                memcpy(query + 1, &challenge_idx, 8);
                
                printf("[AUDIT] Sent 1000 packets. Challenging past index #%lu... ", challenge_idx);
                sendto(sockfd, query, 9, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));

                // 3. 听取手机 A 的辩护 (接收 0x03 证明包)
                uint8_t resp[33];
                int n = recvfrom(sockfd, resp, 33, 0, NULL, NULL);
                if (n == 33 && resp[0] == 0x03) {
                    uint8_t expected[32];
                    generate_mock_hash(challenge_idx, expected);
                    
                    // 核心对比：收到的数据和我们本地算出来的是否一模一样？
                    if (memcmp(resp + 1, expected, 32) == 0) {
                        printf("\033[32mSUCCESS! (Zero-Knowledge Match)\033[0m\n");
                    } else {
                        printf("\033[31mCORRUPTION DETECTED!\033[0m\n");
                    }
                } else {
                    // 如果 A 正在进行 MH-LOCKING 内存硬锁定，它可能没空理我们
                    printf("\033[33mTIMEOUT (Node might be MH-Locking)\033[0m\n");
                }
            }
            i++;
            // 微微限速，防止手机 B 把自己的网卡缓冲区挤爆
            if(i % 100 == 0) usleep(150); 
        }
    } 
    // 【模式 2】：手动零知识审计 (输入特定数字查账)
    else if (strcmp(mode, "verify") == 0 && argc == 4) {
        uint64_t target_idx = strtoull(argv[3], NULL, 10);
        printf("\033[36m[MANUAL] Requesting Proof for index #%lu from %s...\033[0m\n", target_idx, ip);
        
        uint8_t query[9];
        query[0] = 0x02;
        memcpy(query + 1, &target_idx, 8);
        sendto(sockfd, query, 9, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));

        // 手动查询给 2 秒的宽容时间
        struct timeval tv_long = {2, 0};
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv_long, sizeof(tv_long));

        uint8_t resp[33];
        int n = recvfrom(sockfd, resp, 33, 0, NULL, NULL);
        if (n == 33 && resp[0] == 0x03) {
            uint8_t expected[32];
            generate_mock_hash(target_idx, expected);
            
            printf("\n=> Expected : "); print_hex(expected, 32); printf("\n");
            printf("=> Received : "); print_hex(resp + 1, 32); printf("\n\n");

            if (memcmp(resp + 1, expected, 32) == 0) {
                printf("\033[32;1m[ VERIFIED ] The node firmly holds your data! 幼儿园毕业证存在！\033[0m\n");
            } else {
                printf("\033[31;1m[ FAILED ] Data mismatch! 账本被篡改！\033[0m\n");
            }
        } else {
            printf("\033[31m[ ERROR ] No valid proof received. Index may not exist yet or node is busy.\033[0m\n");
        }
    } else {
        printf("Invalid arguments.\n");
    }
    close(sockfd);
    return 0;
}

