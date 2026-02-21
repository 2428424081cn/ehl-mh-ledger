#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

// 保持和 A 端完全一致的确定性哈希生成逻辑
void generate_mock_hash(uint64_t index, uint8_t *out) {
    memset(out, 0, 32);
    snprintf((char*)out, 32, "EHL-DATA-%08lu", index); 
}

void print_hex(uint8_t *data, int len) {
    for(int i=0; i<len; i++) printf("%02x", data[i]);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("用法:\n");
        printf("  上传: %s <IP> upload <index>\n", argv[0]);
        printf("  验证: %s <IP> verify <index>\n", argv[0]);
        return 1;
    }

    char *ip = argv[1];
    char *mode = argv[2];
    uint64_t target_idx = strtoull(argv[3], NULL, 10);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8888);
    inet_pton(AF_INET, ip, &servaddr.sin_addr);

    // 设置 3 秒超时，足够 A 响应了
    struct timeval tv = {3, 0};
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (strcmp(mode, "upload") == 0) {
        printf("正在上传索引 #%lu 的存证数据...\n", target_idx);
        uint8_t packet[33];
        packet[0] = 0x01; // 0x01: 存证协议
        generate_mock_hash(target_idx, packet + 1);
        
        sendto(sockfd, packet, 33, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
        printf("已发出! 请检查手机 A 的 Commit 计数是否增加。\n");

    } else if (strcmp(mode, "verify") == 0) {
        printf("正在验证索引 #%lu 的存证数据...\n", target_idx);
        uint8_t query[9];
        query[0] = 0x02; // 0x02: 查询协议
        memcpy(query + 1, &target_idx, 8);
        
        sendto(sockfd, query, 9, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));

        uint8_t resp[33];
        int n = recvfrom(sockfd, resp, 33, 0, NULL, NULL);
        if (n == 33 && resp[0] == 0x03) {
            uint8_t expected[32];
            generate_mock_hash(target_idx, expected);
            
            printf("\n预期值: "); print_hex(expected, 32);
            printf("\n收到值: "); print_hex(resp + 1, 32);
            printf("\n\n");

            if (memcmp(resp + 1, expected, 32) == 0) {
                printf("\033[32;1m[ SUCCESS ] 逻辑完全匹配！这就是你的“幼儿园毕业证”。\033[0m\n");
            } else {
                printf("\033[31;1m[ FAILED ] 数据不一致！请检查 A 端的存储偏移量逻辑。\033[0m\n");
            }
        } else {
            printf("\033[31m[ ERROR ] 未收到回包或超时。A 端可能正在忙或网络不通。\033[0m\n");
        }
    }
    close(sockfd);
    return 0;
}

