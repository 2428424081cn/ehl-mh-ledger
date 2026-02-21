#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#define HASH_SIZE 32ULL
#define MAX_LEAVES 100000000ULL 
#define FILE_SIZE (MAX_LEAVES * HASH_SIZE)

typedef struct {
    int fd;
    uint8_t *mmap_ptr;
    uint64_t current_offset;
    uint64_t max_capacity;
} MMRStorage;

// --- 已有功能：初始化存储 ---
MMRStorage* init_mmr_storage(const char *filename) {
    MMRStorage *storage = malloc(sizeof(MMRStorage));
    if (!storage) return NULL;
    storage->fd = open(filename, O_RDWR | O_CREAT, 0644);
    if (storage->fd < 0) { free(storage); return NULL; }
    
    // 确保文件大小足够支持 1 亿个哈希
    if (ftruncate(storage->fd, FILE_SIZE) == -1) { 
        close(storage->fd); free(storage); return NULL; 
    }
    
    storage->mmap_ptr = mmap(NULL, FILE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, storage->fd, 0);
    if (storage->mmap_ptr == MAP_FAILED) { 
        close(storage->fd); free(storage); return NULL; 
    }
    
    storage->max_capacity = MAX_LEAVES;
    // 实际项目中这里应读取文件头来恢复 offset，此处简化为 0
    storage->current_offset = 0;
    return storage;
}

// --- 已有功能：追加写入 ---
void append_hash(MMRStorage *storage, uint8_t *hash) {
    if (storage->current_offset < storage->max_capacity) {
        memcpy(storage->mmap_ptr + (storage->current_offset * HASH_SIZE), hash, HASH_SIZE);
        storage->current_offset++;
    }
}

// --- 【新增关键功能】：根据索引读取哈希 ---
// 这是证明协议的核心：给定一个序号，从磁盘(mmap)中取出对应的 32 字节指纹
int get_hash_by_index(MMRStorage *storage, uint64_t index, uint8_t *out_hash) {
    if (index >= storage->current_offset) {
        return -1; // 索引超出当前已存范围，证明失败
    }
    // 直接通过指针偏移读取，无需磁盘 I/O（操作系统自动处理 Page Fault）
    memcpy(out_hash, storage->mmap_ptr + (index * HASH_SIZE), HASH_SIZE);
    return 0; // 提取成功
}

// --- 已有功能：刷盘与关闭 ---
void sync_to_disk(MMRStorage *storage) {
    msync(storage->mmap_ptr, FILE_SIZE, MS_ASYNC);
}

void close_mmr_storage(MMRStorage *storage) {
    munmap(storage->mmap_ptr, FILE_SIZE);
    close(storage->fd);
    free(storage);
}

