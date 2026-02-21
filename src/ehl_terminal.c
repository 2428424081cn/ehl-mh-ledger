#include "mmr_storage.h"
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/mman.h>

#define UDP_PORT 8888
#define LOG_MAX 10

typedef struct {
    uint64_t total_commits;
    uint64_t total_audits_recv;
    uint64_t total_audits_sent;
    int is_running;
} LedgerState;

LedgerState state = {0, 0, 0, 1};
MMRStorage *global_storage = NULL;
pthread_mutex_t storage_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct { char msg[128]; } Trace;
Trace logs[LOG_MAX];
int log_ptr = 0;

void add_trace(const char *fmt, ...) {
    pthread_mutex_lock(&storage_mutex);
    va_list args;
    va_start(args, fmt);
    vsnprintf(logs[log_ptr].msg, 128, fmt, args);
    va_end(args);
    log_ptr = (log_ptr + 1) % LOG_MAX;
    pthread_mutex_unlock(&storage_mutex);
}

void* network_listener(void* arg) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in servaddr, cliaddr;
    uint8_t buffer[128];

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(UDP_PORT);
    bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr));

    while(state.is_running) {
        socklen_t len = sizeof(cliaddr);
        ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&cliaddr, &len);
        if (n <= 0) continue;

        if (buffer[0] == 0x01 && n == 33) {
            pthread_mutex_lock(&storage_mutex);
            // 【核心修复】永远以当前文件真实的物理偏移量计算索引
            uint64_t real_idx = global_storage->current_offset / 32; 
            
            append_hash(global_storage, buffer + 1);
            state.total_commits = global_storage->current_offset / 32; // 强同步
            msync(global_storage->mmap_ptr, global_storage->max_capacity, MS_ASYNC); 
            pthread_mutex_unlock(&storage_mutex);

            uint8_t receipt[9];
            receipt[0] = 0x05;
            memcpy(receipt + 1, &real_idx, 8);
            sendto(sockfd, receipt, 9, 0, (struct sockaddr *)&cliaddr, len);
            add_trace("[WRITE] Data saved to Index #%lu", real_idx);
        } 
        else if (buffer[0] == 0x02 && n == 9) {
            uint64_t target_idx;
            memcpy(&target_idx, buffer + 1, 8);
            state.total_audits_recv++;

            uint8_t resp[33];
            resp[0] = 0x04; 
            memset(resp + 1, 0, 32);

            pthread_mutex_lock(&storage_mutex);
            int res = get_hash_by_index(global_storage, target_idx, resp + 1);
            pthread_mutex_unlock(&storage_mutex);

            if (res == 0) {
                resp[0] = 0x03; 
                state.total_audits_sent++;
                add_trace("[SUCCESS] Verified Index #%lu", target_idx);
            } else {
                add_trace("[FAIL] Index #%lu NOT FOUND", target_idx);
            }
            sendto(sockfd, resp, 33, 0, (struct sockaddr *)&cliaddr, len);
        }
    }
    return NULL;
}

void *ui_thread(void *arg) {
    initscr(); noecho(); curs_set(0); start_color();
    init_pair(1, COLOR_CYAN, COLOR_BLACK);
    while(state.is_running) {
        clear();
        mvprintw(1, 2, "EHL-MH LEDGER | ARCHITECTURE FIXED");
        mvprintw(3, 2, "Logical Commits: %lu (Physical Offset: %lu bytes)", state.total_commits, global_storage->current_offset);
        attron(COLOR_PAIR(1));
        mvprintw(5, 2, "Audits Recv/Sent: %lu / %lu", state.total_audits_recv, state.total_audits_sent);
        attroff(COLOR_PAIR(1));
        mvprintw(7, 2, "--- Trace Log ---");
        for(int i=0; i<LOG_MAX; i++) {
            int idx = (log_ptr + i) % LOG_MAX;
            mvprintw(8+i, 4, "> %s", logs[idx].msg);
        }
        refresh(); usleep(100000);
    }
    endwin(); return NULL;
}

int main() {
    global_storage = init_mmr_storage("../mmr_data.bin");
    
    // 【核心修复】启动时，强行根据文件大小恢复 Commit 计数！
    state.total_commits = global_storage->current_offset / 32;
    
    pthread_t ui_tid, net_tid;
    pthread_create(&ui_tid, NULL, ui_thread, NULL);
    pthread_create(&net_tid, NULL, network_listener, NULL);
    while(state.is_running) { sleep(5); sync_to_disk(global_storage); }
    return 0;
}

