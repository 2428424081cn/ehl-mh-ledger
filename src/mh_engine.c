#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

// 参数定义
#define NODE_SIZE 32           // 32字节 (256-bit)
#define TOTAL_RAM_MB 1800      // 申请 1800MB
#define N ((uint64_t)TOTAL_RAM_MB * 1024 * 1024 / NODE_SIZE)

// 极简 PRF：线性同余生成伪随机索引 (减少计算开销，突出访存延迟)
static inline uint64_t get_rand_idx(uint64_t i, uint64_t seed) {
    return (i * 6364136223846793005ULL + seed) % i;
}

// 模拟哈希函数 (实际应使用 BLAKE3，此处先用简单 XOR 模拟访存压力)
void pseudo_hash(uint8_t *out, uint8_t *p1, uint8_t *p2, uint8_t *root) {
    for (int i = 0; i < NODE_SIZE; i++) {
        out[i] = p1[i] ^ p2[i] ^ root[i];
    }
}

int main() {
    printf("EHL-MH Engine Initializing...\n");
    printf("Target Nodes: %lu (Total RAM: %d MB)\n", N, TOTAL_RAM_MB);

    // 1. 分配大内存
    uint8_t *memory = (uint8_t *)malloc(N * NODE_SIZE);
    if (!memory) {
        printf("Error: Could not allocate memory!\n");
        return 1;
    }
    
    uint8_t epoch_root[NODE_SIZE] = {0x13, 0x37}; // 模拟上层 Commit Root
    uint64_t seed = 0xDEADBEEF;

    // 2. 计时开始
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    printf("Starting Memory-Hard Computation...\n");

    // 3. 核心计算循环
    for (uint64_t i = 1; i < N; i++) {
        uint64_t prev = i - 1;
        uint64_t rand_idx = get_rand_idx(i, seed);
        
        pseudo_hash(&memory[i * NODE_SIZE], 
                    &memory[prev * NODE_SIZE], 
                    &memory[rand_idx * NODE_SIZE], 
                    epoch_root);
        
        // 每 10M 节点打印一次进度
        if (i % 10000000 == 0) printf("Progress: %lu / %lu nodes\n", i, N);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("\nSuccess! Root calculated in %.2f seconds.\n", elapsed);
    printf("Memory Bandwidth Efficiency Test Complete.\n");

    free(memory);
    return 0;
}

