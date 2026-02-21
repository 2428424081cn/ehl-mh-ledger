#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

void print_hex(uint8_t *hash, int len) {
    for (int i = 0; i < len; i++) {
        printf("%02x", hash[i]);
    }
}

int main() {
    FILE *db = fopen("ledger.db", "rb");
    if (!db) {
        printf("Error: Could not open ledger.db\n");
        return 1;
    }

    uint8_t buffer[32];
    int epoch = 0;

    printf("=== EHL-MH Ledger Audit Tool ===\n");
    printf("%-10s | %-64s\n", "Epoch", "Final Proof (Root)");
    printf("-----------|----------------------------------------------------------------\n");

    // 逐个读取 32 字节的证明并显示
    while (fread(buffer, 1, 32, db) == 32) {
        printf("Epoch %-4d | ", epoch++);
        print_hex(buffer, 32);
        printf("\n");
    }

    fclose(db);
    printf("-----------|----------------------------------------------------------------\n");
    printf("Audit Complete. Total Epochs found: %d\n", epoch);

    return 0;
}

