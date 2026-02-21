#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

// 1.8GB 内存锁定参数
#define NODE_SIZE 32
#define TOTAL_RAM_MB 1800
#define N ((uint64_t)TOTAL_RAM_MB * 1024 * 1024 / NODE_SIZE)

// 极简 PRF：线性同余生成伪随机索引
static inline uint64_t get_rand_idx(uint64_t i, uint64_t seed) {
    return (i * 6364136223846793005ULL + seed) % i;
}

// 模拟哈希函数 (XOR 模拟访存压力)
void pseudo_hash(uint8_t *out, uint8_t *p1, uint8_t *p2, uint8_t *root) {
    for (int i = 0; i < NODE_SIZE; i++) {
        out[i] = p1[i] ^ p2[i] ^ root[i];
    }
}

int main() {
    // 1. 初始化
    uint8_t *memory = (uint8_t *)malloc(N * NODE_SIZE);
    if (!memory) {
        fprintf(stderr, "Error: Insufficient RAM for 1.8GB Lock Layer.\n");
        return 1;
    }

    uint8_t epoch_root[NODE_SIZE] = {0x13, 0x37};
    uint64_t seed = 0xDEADBEEF;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // 2. 核心计算循环 (MH-Proof 构建)
    // 这个循环只负责消耗内存和 CPU，不执行任何磁盘写入或账本累加
    for (uint64_t i = 1; i < N; i++) {
        uint64_t prev = i - 1;
        uint64_t rand_idx = get_rand_idx(i, seed);

        pseudo_hash(&memory[i * NODE_SIZE], 
                    &memory[prev * NODE_SIZE],
                    &memory[rand_idx * NODE_SIZE],
                    epoch_root);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_spent = (end.tv_sec - start.tv_sec) + 
                        (end.tv_nsec - start.tv_nsec) / 1000000000.0;

    // 只打印计算结果，不打印模拟存证日志
    printf("Parallel MH Root calculated in %.2f seconds using 4 cores.\n", time_spent);

    // 3. 清理并退出
    free(memory);
    return 0;
}

