#include "mmr_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>

#define HASH_SIZE 32
#define DEFAULT_CAPACITY (3ULL * 1024 * 1024 * 1024) // 约 3GB

// 1. 实现初始化函数
MMRStorage* init_mmr_storage(const char *filename) {
    MMRStorage *storage = (MMRStorage *)malloc(sizeof(MMRStorage));
    if (!storage) return NULL;

    // 打开或创建文件
    storage->fd = open(filename, O_RDWR | O_CREAT, 0644);
    if (storage->fd < 0) { free(storage); return NULL; }

    // 获取文件当前大小
    struct stat st;
    fstat(storage->fd, &st);
    storage->current_offset = st.st_size;
    storage->max_capacity = DEFAULT_CAPACITY;

    // 确保文件大小足够映射
    if (st.st_size < storage->max_capacity) {
        ftruncate(storage->fd, storage->max_capacity);
    }

    // 内存映射
    storage->mmap_ptr = (uint8_t *)mmap(NULL, storage->max_capacity, 
                                       PROT_READ | PROT_WRITE, MAP_SHARED, storage->fd, 0);
    
    if (storage->mmap_ptr == MAP_FAILED) {
        close(storage->fd);
        free(storage);
        return NULL;
    }

    return storage;
}

// 2. 实现读取函数
int get_hash_by_index(MMRStorage *storage, uint64_t index, uint8_t *out_hash) {
    if (!storage || !storage->mmap_ptr) return -1;
    
    uint64_t target_pos = index * HASH_SIZE;
    
    // 关键检查：不能读取超过当前写入位置的数据
    if (target_pos + HASH_SIZE > storage->current_offset) {
        return -1; 
    }

    memcpy(out_hash, storage->mmap_ptr + target_pos, HASH_SIZE);
    return 0;
}

// 3. 实现写入函数
void append_hash(MMRStorage *storage, uint8_t *hash) {
    if (!storage || !storage->mmap_ptr) return;
    
    if (storage->current_offset + HASH_SIZE > storage->max_capacity) return;

    memcpy(storage->mmap_ptr + storage->current_offset, hash, HASH_SIZE);
    storage->current_offset += HASH_SIZE;
}

// 4. 实现刷盘函数
void sync_to_disk(MMRStorage *storage) {
    if (storage && storage->mmap_ptr) {
        msync(storage->mmap_ptr, storage->max_capacity, MS_ASYNC);
        // 顺便更新文件实际大小，防止重启后偏移量丢失
        ftruncate(storage->fd, storage->current_offset);
    }
}

// 5. 实现关闭函数
void close_mmr_storage(MMRStorage *storage) {
    if (storage) {
        sync_to_disk(storage);
        munmap(storage->mmap_ptr, storage->max_capacity);
        close(storage->fd);
        free(storage);
    }
}

