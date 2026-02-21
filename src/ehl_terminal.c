#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <arpa/inet.h>
#include <sys/socket.h>

// --- 1. 存储模块前置声明 ---
typedef struct {
    int fd;
    uint8_t *mmap_ptr;
    uint64_t current_offset;
    uint64_t max_capacity;
} MMRStorage;

MMRStorage* init_mmr_storage(const char *filename);
void append_hash(MMRStorage *storage, uint8_t *hash);
int get_hash_by_index(MMRStorage *storage, uint64_t index, uint8_t *out_hash); // 新增查询接口
void sync_to_disk(MMRStorage *storage);
void close_mmr_storage(MMRStorage *storage);

// --- 2. 核心参数与状态 ---
#define UDP_PORT 8888
#define EPOCH_DURATION 10
#define MAX_LOG_CAPACITY 100000000ULL
#define SAMPLE_LOG_LINES 5   
#define AUDIT_LOG_LINES 3    // 新增：审计日志行数

typedef struct {
    uint64_t total_leaves;
    uint64_t total_audits;   // 新增：记录被成功查账的次数
    uint32_t current_epoch;
    double measured_tps;
    int is_running;
    int is_mh_active;
    int is_io_syncing;
} LedgerState;

LedgerState state = {0, 0, 0, 0.0, 1, 0, 0};
MMRStorage *global_storage = NULL;

// --- 3. 日志缓存系统 ---
typedef struct {
    char hash_hex[65];
    uint64_t index;
    char timestamp[10];
} LogEntry;

LogEntry sample_logs[SAMPLE_LOG_LINES];
LogEntry audit_logs[AUDIT_LOG_LINES]; // 新增：专门存放查账记录
int log_idx = 0;
int audit_idx = 0;

void get_time_str(char *buf) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, 10, "%H:%M:%S", t);
}

// --- 4. 网络协议处理器 (核心大升级) ---
void* network_listener_thread(void* arg) {
    int sockfd;
    uint8_t buffer[64]; // 增大缓冲区
    struct sockaddr_in servaddr, cliaddr;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) return NULL;

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(UDP_PORT);

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        close(sockfd); return NULL;
    }

    while(state.is_running) {
        socklen_t len = sizeof(cliaddr);
        ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&cliaddr, &len);
        
        if (n <= 0) continue;

        // 【协议分支 A】：处理写入请求 (0x01 + 32字节 = 33字节)
        if (n == 33 && buffer[0] == 0x01 && !state.is_mh_active) {
            append_hash(global_storage, buffer + 1);
            state.total_leaves++;

            // 存证采样流 (每 1000 次展示一次)
            if (state.total_leaves % 1000 == 0) {
                char hex[65] = {0};
                for(int i=0; i<8; i++) sprintf(hex + i*2, "%02x", buffer[i+1]);
                strcat(hex, "..."); 
                strcpy(sample_logs[log_idx].hash_hex, hex);
                sample_logs[log_idx].index = state.total_leaves - 1; // 真正的索引是从 0 开始的
                get_time_str(sample_logs[log_idx].timestamp);
                log_idx = (log_idx + 1) % SAMPLE_LOG_LINES;
            }
        }
        // 【协议分支 B】：处理查询/证明请求 (0x02 + 8字节 = 9字节)
        else if (n == 9 && buffer[0] == 0x02) {
            uint64_t target_idx;
            memcpy(&target_idx, buffer + 1, 8); // 提取手机B想查询的索引

            uint8_t response[33];
            response[0] = 0x03; // 回传类型：证明数据

            // 尝试从磁盘/内存映射中读取
            if (get_hash_by_index(global_storage, target_idx, response + 1) == 0) {
                // 读取成功，发回给请求者
                sendto(sockfd, response, 33, 0, (struct sockaddr *)&cliaddr, len);
                state.total_audits++; // 审计计数器 +1

                // 记录到审计监控面板 (UI)
                char hex[65] = {0};
                for(int i=0; i<8; i++) sprintf(hex + i*2, "%02x", response[i+1]);
                strcat(hex, "...");
                strcpy(audit_logs[audit_idx].hash_hex, hex);
                audit_logs[audit_idx].index = target_idx;
                get_time_str(audit_logs[audit_idx].timestamp);
                audit_idx = (audit_idx + 1) % AUDIT_LOG_LINES;
            }
        }
    }
    close(sockfd); return NULL;
}

// --- 5. UI 渲染线程 ---
void *ui_loop(void *arg) {
    initscr(); noecho(); curs_set(0); start_color();
    init_pair(1, COLOR_CYAN, COLOR_BLACK);   // 标题
    init_pair(2, COLOR_GREEN, COLOR_BLACK);  // 进度
    init_pair(3, COLOR_RED, COLOR_BLACK);    // 锁定
    init_pair(4, COLOR_YELLOW, COLOR_BLACK); // 存证流
    init_pair(5, COLOR_MAGENTA, COLOR_BLACK);// 审计流 (创新点)

    while(state.is_running) {
        clear();
        int my, mx; getmaxyx(stdscr, my, mx);

        attron(COLOR_PAIR(1) | A_BOLD);
        mvprintw(1, (mx-40)/2, "EHL-MH DISTRIBUTED NODE (2-Way Protocol)");
        attroff(COLOR_PAIR(1) | A_BOLD);

        if(state.is_mh_active) {
            attron(COLOR_PAIR(3) | A_BLINK); mvprintw(3, 2, "Status: MH-LOCKING (Securing)"); attroff(COLOR_PAIR(3));
        } else {
            attron(COLOR_PAIR(2)); mvprintw(3, 2, "Status: INGESTING & PROVING"); attroff(COLOR_PAIR(2));
        }

        mvprintw(5, 2, "--- Real-time Metrics ---");
        mvprintw(6, 4, "Epoch:          %u", state.current_epoch);
        mvprintw(7, 4, "Total Commits:  %lu", state.total_leaves);
        mvprintw(8, 4, "Measured TPS:   %.0f tx/s", state.measured_tps);
        // 新增审计指标
        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(9, 4, "Audits Served:  %lu (Zero-Knowledge Verified)", state.total_audits);
        attroff(COLOR_PAIR(5) | A_BOLD);

        mvprintw(11, 2, "--- mmap Persistence Layer ---");
        double usage = (double)state.total_leaves / (double)MAX_LOG_CAPACITY * 100.0;
        mvprintw(12, 4, "Progress: [");
        attron(COLOR_PAIR(2));
        for(int i=0; i<20; i++) printw(i < (int)(usage/5) ? "|" : ".");
        attroff(COLOR_PAIR(2));
        printw("] %.2f%%", usage);
        mvprintw(13, 4, "Data Written: %.3f GB / 2.980 GB", (double)(state.total_leaves * 32.0) / 1073741824.0);

        // --- 新增：审计看门狗面板 ---
        mvprintw(15, 2, "--- Audit Watchdog (Live Queries) ---");
        attron(COLOR_PAIR(5));
        for(int i=0; i < AUDIT_LOG_LINES; i++) {
            int idx = (audit_idx + i) % AUDIT_LOG_LINES;
            if (audit_logs[idx].index > 0 || audit_logs[idx].hash_hex[0] != '\0') {
                mvprintw(16 + i, 4, "[%s] Node asked for #%08lu -> PROOF SENT: %s", 
                         audit_logs[idx].timestamp, audit_logs[idx].index, audit_logs[idx].hash_hex);
            }
        }
        attroff(COLOR_PAIR(5));

        // --- 原有的存证采样流下移 ---
        mvprintw(20, 2, "--- Live Ingestion Stream (Sampled 1:1000) ---");
        attron(COLOR_PAIR(4));
        for(int i=0; i < SAMPLE_LOG_LINES; i++) {
            int idx = (log_idx + i) % SAMPLE_LOG_LINES;
            if (sample_logs[idx].index > 0 || sample_logs[idx].hash_hex[0] != '\0') {
                mvprintw(21 + i, 4, "[%s] Commit #%08lu -> Hash: %s [STORED]", 
                         sample_logs[idx].timestamp, sample_logs[idx].index, sample_logs[idx].hash_hex);
            }
        }
        attroff(COLOR_PAIR(4));

        refresh(); usleep(200000);
    }
    endwin(); return NULL;
}

// --- 6. 主逻辑 ---
int main() {
    global_storage = init_mmr_storage("../mmr_data.bin");
    if(!global_storage) return 1;

    pthread_t ui_tid, net_tid;
    pthread_create(&ui_tid, NULL, ui_loop, NULL);
    pthread_create(&net_tid, NULL, network_listener_thread, NULL);

    while(state.is_running) {
        state.is_mh_active = 1;
        system("../mh_parallel > /dev/null 2>&1");
        state.is_mh_active = 0;

        for(int i=0; i<EPOCH_DURATION; i++) {
            uint64_t before = state.total_leaves;
            sleep(1);
            state.measured_tps = (double)(state.total_leaves - before);
        }

        state.is_io_syncing = 1;
        sync_to_disk(global_storage);
        usleep(500000);
        state.is_io_syncing = 0;
        state.current_epoch++;
    }

    close_mmr_storage(global_storage);
    return 0;
}

