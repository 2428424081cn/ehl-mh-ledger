#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

// 使用 ULL 后缀防止 32 位溢出
#define HASH_SIZE 32ULL
#define MAX_LEAVES 100000000ULL 
#define FILE_SIZE (MAX_LEAVES * HASH_SIZE)

typedef struct {
    int fd;
    uint8_t *mmap_ptr;
    uint64_t current_offset;
    uint64_t max_capacity;
} MMRStorage;

// 初始化生产级 mmap 持久化层
MMRStorage* init_mmr_storage(const char *filename) {
    MMRStorage *storage = malloc(sizeof(MMRStorage));
    if (!storage) return NULL;

    // 1. 打开文件
    storage->fd = open(filename, O_RDWR | O_CREAT, 0644);
    if (storage->fd < 0) { free(storage); return NULL; }

    // 2. 预分配磁盘空间 (Sparse File)
    if (ftruncate(storage->fd, FILE_SIZE) == -1) {
        close(storage->fd); free(storage); return NULL;
    }

    // 3. 建立映射
    storage->mmap_ptr = mmap(NULL, FILE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, storage->fd, 0);
    if (storage->mmap_ptr == MAP_FAILED) {
        close(storage->fd); free(storage); return NULL;
    }

    storage->max_capacity = MAX_LEAVES;
    storage->current_offset = 0;

    return storage;
}

// 零拷贝写入磁盘映射区
void append_hash(MMRStorage *storage, uint8_t *hash) {
    if (storage->current_offset < storage->max_capacity) {
        memcpy(storage->mmap_ptr + (storage->current_offset * HASH_SIZE), hash, HASH_SIZE);
        storage->current_offset++;
    }
}

// 强制同步到 SSD
void sync_to_disk(MMRStorage *storage) {
    msync(storage->mmap_ptr, FILE_SIZE, MS_ASYNC);
}

void close_mmr_storage(MMRStorage *storage) {
    munmap(storage->mmap_ptr, FILE_SIZE);
    close(storage->fd);
    free(storage);
}

