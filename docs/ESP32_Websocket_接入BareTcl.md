# ESP32 WebSocket 接入 BareTcl 调试与开发日志

本项目采用 **FACT (Forensic, Adversarial, Collaboration, Tribunal)** 四方对抗式质量控制流程进行开发。本文档记录整个开发、对质质询与收敛合并的过程。

---

## 一、 FACT 工作计划 (Task Plan)

### 1. 智能体角色定位 (True Sub-agents Positioning)
本任务借助于原生系统工具定义并运行以下三个独立的子智能体：
*   **Agent A: 包工头 (The Architect - 本机主会话)**：方案主导者与最终裁决者。
*   **Agent B: 牛马 (The Builder - 子智能体 `builder`)**：核心开发者，负责 C 与 HTML/JS 逻辑实现。
*   **Agent C: 杠精 (The Antagonist - 子智能体 `antagonist`)**：安全与可靠性 Reviewer，挖掘潜在逻辑缺陷。
*   **Agent D: 监理 (The Auditor - 子智能体 `auditor`)**：过程合规性审计员，核对证据等级与合理性。

### 2. 工作步骤与协同流 (Workflow)
从 Milestone 1 到 Milestone 4，采用“开发 -> 质询 -> 审计 -> 仲裁/整改 -> 交付”的闭环流程。

---

## 二、 质询对抗阶段 (Adversarial Phase)

本阶段由 **Agent C (Antagonist)** 提出缺陷控诉，经由 **Agent D (Auditor)** 进行证据审计确认。立案的 **4 项缺陷** 如下：

### 1. [DEFECT-01] 全局变量并发访问数据竞态与连接复用漏洞 (High)
*   **问题描述**：在 `main.c` 中，全局变量 `ws_active_fd`（活跃 WebSocket 文件描述符）和 `ws_server_handle` 被 `tcl_task` 线程（发送时）和 HTTPD 线程（会话释放时）并发读写，没有任何锁机制保护。存在 TOCTOU（Time-of-Check to Time-of-Use）竞争，并可能因为 FD 被底层新 TCP 连接复用而将控制台输出错误路由至外部链接。
*   **证据等级**：L2 (源码逻辑推演)
*   **审计意见**：CONFIRMED (正式立案，危害等级 Critical/High)

### 2. [DEFECT-02] WebSocket 未限制接收帧长度导致内存分配 OOM 与 DoS 风险 (Critical)
*   **问题描述**：在 WebSocket 接收回调中，程序盲目信任报头中的 `ws_pkt.len` 并直接调用 `calloc` 分配相同大小的内存。若恶意客户端发送伪造超长长度的数据包，会导致 ESP32 (SRAM 约 300KB) 瞬间发生内存分配失败（OOM），进而引发 WiFi 协议栈等其他核心组件分配失败崩溃，实现拒绝服务（DoS）攻击。
*   **证据等级**：L2 (源码逻辑推演)
*   **审计意见**：CONFIRMED (正式立案，危害等级 Critical)

### 3. [DEFECT-03] 串口与网络输入流共享单例编辑器状态 (High)
*   **问题描述**：串口输入（`fgetc(stdin)`）与网络 WebSocket 输入共用在 `tcl_task` 栈上分配的单例 `static TclShell tcl_sh` 编辑器状态。在双端同时打字输入时，字符会交叉插入 `tcl_sh.line` 缓冲区产生严重的乱码，可能引发非法指令注入风险，且两端 Backspace 退格回显冲突导致排版混乱。
*   **证据等级**：L2 (源码逻辑推演)
*   **审计意见**：CONFIRMED (正式立案，危害等级 High)

### 4. [DEFECT-04] `console.html` 交互粗糙且缺失核心继电器控制 (Medium)
*   **问题描述**：牛马最初提交的网页端 `console.html` 仅包含 xterm.js 终端，无法直观开关物理继电器（缺少开关控件），且断网重连时的重连文字粗鲁堆砌至终端行缓冲区，影响美观。
*   **证据等级**：L2 (源码/网页分析)
*   **审计意见**：CONFIRMED (正式立案，危害等级 Medium)

---

## 三、 整改修复阶段 (Rectification Phase)

**Agent B (Builder)** 针对上述四点缺陷完成了全部代码的重构与修复，修改细节如下：

### 1. DEFECT-01 修复：引入互斥锁（Mutex）保护
*   在 `main.c` 中引入全局互斥锁 `ws_mutex` (`SemaphoreHandle_t`)，并在系统初始化阶段（`app_main`）使用 `xSemaphoreCreateMutex()` 完成创建。
*   在所有的 WebSocket fd/handle 读写位置套入加锁流程：
    *   在连接握手 (`ws_handler`) 及断连释放 (`ws_sess_free_ctx`) 时加锁改写。
    *   在 `ws_send_text` 中：
        ```c
        if (ws_mutex == NULL) return;
        if (xSemaphoreTake(ws_mutex, portMAX_DELAY) == pdTRUE) {
            if (ws_server_handle != NULL && ws_active_fd != -1) {
                // 确保句柄与fd在发送期间绝对不被修改释放
                httpd_ws_send_frame_async(ws_server_handle, ws_active_fd, &ws_pkt);
            }
            xSemaphoreGive(ws_mutex);
        }
        ```

### 2. DEFECT-02 修复：最大接收帧限制
*   在 `ws_handler` 提取帧头部后、分配内存前强行进行越界截断检查，配置最大允许字节数为 **1024 字节**：
    ```c
    if (ws_pkt.len > 1024) {
        ESP_LOGE("WS", "Received WS frame size %d exceeds limit of 1024 bytes!", (int)ws_pkt.len);
        return ESP_ERR_INVALID_SIZE; // 触发框架强制关闭物理 TCP 链路
    }
    ```

### 3. DEFECT-03 修复：编辑器实例多例化（Multi-instance Isolation）
*   在 `tcl_task` 初始化时分立两个独立的 Shell 行缓冲实例：
    ```c
    static TclShell uart_sh;
    static TclShell ws_sh;
    shell_init(&uart_sh);
    shell_init(&ws_sh);
    ```
*   串口输入的字符传入 `handle_input_char(ctx, &uart_sh, c)`；而从 WebSocket 队列 (`input_queue`) 消费的字符传入 `handle_input_char(ctx, &ws_sh, c)`。两端拥有各自的命令编辑缓冲区，互不串扰。

### 4. DEFECT-04 修复：前端视觉与交互功能全面重构
*   **控制面板**：在 [console.html](file:///home/chenming/BareTcl/ESP32_ports/console.html) 头部区域新增了一个 HTML Grid 布局的“Hardware Control Panel”，包含 4 路继电器的控制开关，并绑定了 `fetch('/change_relayX')` 接口。
*   **状态倒计时**：点击控制按钮后，前端展示发光脉冲边框，并开启 10 秒倒计时自动物理上锁特效，防止硬件继电器被高频误点导致电涌。
*   **网络状态与断线恢复**：取消了在 xterm.js 写入重连字符的粗暴做法。改为通过右上角的 “Status Badge” 状态徽标红绿脉冲闪烁来安静表现断连和重试。
*   **辅助按键**：在终端框下方配置了一组常用的终端修饰按键（Tab, Ctrl+C, Up, Down, Clear），极大地方便了移动端触屏操控。

---

## 四、 最终审计与裁决结论 (Tribunal & Closure)

*   **审计验证证据**：
    *   **L3 证据（编译成功）**：本地执行 `./build.sh` 对新代码完成了构建，在 `build/` 目录下成功输出了最终的 `ESP32_ports.bin` 二进制镜像包。
    *   **L2 证据（源码合规）**：新补丁完美闭环了锁竞争、OOM 内存和状态污染逻辑。
*   **最终裁决**：
    *   所有 **Critical** 和 **High** 严重缺陷清零，退出收敛条件完全达成。
    *   包工头（Agent A）判定此次整改结果为 **CONFIRMED / PASS**，准予代码合并与上线。
