# Clay

![Clay Logo](icon.png)  
*像黏土一样灵活塑造你的代码历史 / Shape your code history like clay*  

---

## 🌟 项目概述 / Overview  

Clay 是一个专为**快速原型开发**设计的轻量级版本控制系统，相比 Git 更加简单直观，特别适合：  
- 项目初期频繁迭代  
- 实验性代码探索  
- 教学/学习场景  

Clay is a lightweight version control system designed for **rapid prototyping**, simpler and more intuitive than Git, perfect for:  
- Early-stage project iteration  
- Experimental coding  
- Teaching/learning scenarios  

```bash
# 自动记录每次更改 / Auto-saves changes
clay init my_project  

# 随时回溯到任意时间点 / Rewind to any point in time  
clay rewind 10min  
```

---

## 🚀 核心功能 / Key Features  

| 功能 / Feature | 命令示例 / Example Command |  
|---------------|---------------------------|  
| **自动快照** / Auto-snapshots | 每30秒自动保存代码状态 |  
| **时间旅行** / Time travel | `clay rewind 14:30` |  
| **临时分支** / Temp branches | `clay branch --temp` |  
| **一键撤销** / One-click undo | `clay undo` |  

---

## 📦 安装指南 / Installation  

Note: bsdiff和bspatch已包含在项目中，但是可能需要手动安装lzm4

### Linux/macOS  (Test on Ubuntu22.04)
```bash
git clone https://github.com/MengAiDev/clay.git  
cd clay
git clone https://github.com/lz4/lz4 third_party && cd third_party/lz4
make
cd ../..
mkdir build && cd build  
cmake .. && make  
sudo make install  
```

### Windows 
NOT support.

---

## 🛠️ 基本使用 / Basic Usage  

```bash
# 初始化仓库 / Initialize repository
clay init  

# 查看历史时间轴 / View timeline  
clay timeline  

# 手动创建快照 / Manual snapshot  
clay commit "重构用户模块"  

# 回到5分钟前的状态 / Rewind 5 minutes  
clay rewind 5min  

# 创建临时实验分支 / Create temp branch  
clay branch --temp  
```

---

## 🆚 与 Git 对比 / VS Git  

| 场景 / Scenario       | Git                     | **Clay**               |  
|-----------------------|-------------------------|------------------------|  
| 保存当前状态 / Save    | `git add . && git commit -m "..."` | **自动完成** / Auto-saved |  
| 试验性编码 / Experiment | 需新建分支 / Need new branch | `clay branch --temp` (内存分支 / In-memory) |  
| 存储开销 / Storage    | 对象库膨胀 / Bloat      | **差异压缩** / Delta-compressed |  

---

## 🌍 设计理念 / Philosophy  

- 🧱 **零配置**：开箱即用，无需复杂设置  
- ⚡ **瞬时回溯**：像 CTRL+Z 一样回退代码版本  
- 🧪 **实验友好**：临时分支不污染主代码库  

- 🧱 **Zero-config**: Works out of the box  
- ⚡ **Instant rewind**: Version control like CTRL+Z  
- 🧪 **Experiment-friendly**: Temp branches won't pollute main code  

---

## 📜 开源协议 / License  

[MIT License](LICENSE)  

--- 

## 🤝 贡献指南 / Contributing  

欢迎提交 PR 或 Issue！  
We welcome PRs and issues!  
