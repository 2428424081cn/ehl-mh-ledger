#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

// 模拟生成 32 字节的最终证明
void generate_final_root(uint8_t *out) {
    // 实际应为 H(MMR_Root || MH_Root)
    // 此处使用当前时间戳模拟生成
    uint64_t ts = (uint64_t)time(NULL);
    memcpy(out, &ts, 8);
    for(int i=8; i<32; i++) out[i] = out[i-8] ^ 0xAA;
}

int main() {
    uint8_t epoch_proof[32];
    uint32_t epoch_count = 0;

    printf("=== EHL-MH Ledger v1.1 (Persistent Mode) ===\n");
    printf("Storage Efficiency: 32 bytes per 10s -> ~276KB/day\n\n");

    while(1) {
        // 1. 执行 MH 锁定层 (3.5s - 4.5s)
        printf("[Epoch %u] Starting MH Lock Layer...\n", epoch_count);
        system("./mh_parallel");

        // 2. 模拟 Commit 层处理 (剩下的窗口)
        printf("[Epoch %u] Finalizing Commit Layer (400k TPS)...\n", epoch_count);
        
        // 3. 产生最终证明
        generate_final_root(epoch_proof);

        // 4. 持久化：追加到 ledger.db
        FILE *db = fopen("ledger.db", "ab");
        if (db) {
            fwrite(epoch_proof, 1, 32, db);
            fclose(db);
            printf("[Storage] Proof anchored to ledger.db (Total size: %ld bytes)\n", 
                    (long)(++epoch_count * 32));
        }

        printf("--- Epoch Complete. Sleeping for next cycle ---\n\n");
        sleep(5); // 保持约 10s 的整体步调
    }
    return 0;
}

