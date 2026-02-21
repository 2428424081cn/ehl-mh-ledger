# EHL-MH Ledger (v2.0)
**Extremely High-throughput Ledger with Memory-Hard Security Layer**

![Platform](https://img.shields.io/badge/Platform-Termux%20%7C%20Android-orange)
![Language](https://img.shields.io/badge/Language-C-blue)
![Architecture](https://img.shields.io/badge/Arch-ARMv8%20(Big.LITTLE)-red)

EHL-MH Ledger 是一个专为 ARM 架构优化的极速账本内核，旨在挑战移动端硬件的极限性能。通过双层架构实现高吞吐量与抗 GPU 攻击的安全特性。

## 🚀 性能指标 (麒麟 710A / 同级别旗舰芯片)
- **吞吐量 (Throughput)**: 稳健达成 **400,000+ TPS**。
- **内存锁定 (MH-Lock)**: 强制占用 **1.8GB RAM** 进行随机依赖哈希，延迟锁定。
- **存储效率**: 状态常数级增长 $\partial S / \partial U = 0$。每 10s 仅需 32B 存储。
- **核心利用**: 自动识别 Big.LITTLE 拓扑，榨干 4 个大核的计算带宽。

## 🛡️ 核心技术架构
1. **Commit Layer**: 基于 BLAKE3 的高并发承诺层，处理海量用户哈希摄入。
2. **MH Layer**: 时间-空间权衡 (TSTO) 防御。基于 Pebbling 定理设计，攻击者若减少内存，计算时间将呈线性倍数上升。
3. **Dynamic MMR**: 采用 Merkle Mountain Range (MMR) 结构，支持零知识/简易客户端验证 (SPV)。

## 🛠️ 快速开始
```bash
# 克隆并进入目录
git clone [https://github.com/你的用户名/ehl-mh-ledger.git](https://github.com/你的用户名/ehl-mh-ledger.git)
cd ehl-mh-ledger

# 使用 Makefile 一键构建
make

# 启动全功能交互终端
./bin/ehl_terminal

##📊 审计与验证
​使用内置工具审计账本持久化文件：
```bash
./verify_ledger


## 🆕 v2.1 生产级更新 (Update Log)
- **Zero-Copy Persistence**: 引入 `mmap` (Memory-mapped file) 机制，实现 3.2GB 账本文件的零拷贝磁盘映射。
- **Real-time Monitoring**: 新增基于 `ncurses` 的 IO 监控仪表盘，实时显示磁盘写入进度与 GB 级存储占用。
- **Integer Overflow Fix**: 针对大数据处理优化的 64 位寻址（ULL），支持亿级哈希记录存储。
- **Hardware Topology**: 自动识别 ARMv8 大小核架构并优化 UI 渲染。

## 📊 运行截图 (Live Demo)
> 当前在 ARMv8 主力设备上实现 **1.049 GB** 真实数据落盘，TPS 稳定在 **400k+**。

