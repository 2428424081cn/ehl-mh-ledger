#ifndef MMR_STORAGE_H
#define MMR_STORAGE_H

#include <stdint.h>

// 统一结构体定义
typedef struct {
    int fd;
    uint8_t *mmap_ptr;
    uint64_t current_offset; 
    uint64_t max_capacity;
} MMRStorage;

// 函数声明
MMRStorage* init_mmr_storage(const char *filename);
void append_hash(MMRStorage *storage, uint8_t *hash);
int get_hash_by_index(MMRStorage *storage, uint64_t index, uint8_t *out_hash);
void sync_to_disk(MMRStorage *storage);
void close_mmr_storage(MMRStorage *storage);

#endif

