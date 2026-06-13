//该程序实现了WEB API控制四路继电器，并集成交互式 BareTcl 命令行控制系统
#include <WiFi.h>
#include <WebServer.h>

// -------------------------------------------------------------
// BareTcl 移植集成 (Linkage & Context & Hook Definitions)
// -------------------------------------------------------------
extern "C" {
    // 解决冲突：定义解释器单步微延迟钩子，防止密集计算阻塞同级/系统任务并喂狗
    #define TCL_YIELD_HOOK() do { \
        static uint32_t __yield_cnt = 0; \
        if (++__yield_cnt >= 1000) { \
            __yield_cnt = 0; \
            vTaskDelay(pdMS_TO_TICKS(1)); \
        } \
    } while(0)

    #include "../src/tcl_core.c"
    #include "../src/extcmd.c"
    #include "../src/baretcl_shell.c"

    // 声明 GPIO 命令 C 链接函数
    tcl_i32 tcl_cmd_gpio_mode(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values);
    tcl_i32 tcl_cmd_digital_write(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values);
    tcl_i32 tcl_cmd_digital_read(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values);
    tcl_i32 tcl_cmd_tcl_shell_ansi(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values);
}

// 内存 Arena 静态物理布局
#define TCL_ARENA_SIZE (48 * 1024)
static char tcl_arena[TCL_ARENA_SIZE];

// HAL 层串口物理输出对接
extern "C" void tcl_hal_puts(const tcl_u8 *s) {
    Serial.print((const char *)s);
}

// -------------------------------------------------------------
// DEFECT-03: 无锁单生产者单消费者 (SPSC) 环形缓冲区 (防粘贴溢出丢包)
// -------------------------------------------------------------
#define RX_BUF_SIZE 2048
static char rx_buf[RX_BUF_SIZE];
// volatile 可阻止编译器对变量重排和优化，保证 SPSC 无锁多任务并发安全
static volatile uint32_t rx_head = 0;
static volatile uint32_t rx_tail = 0;

static void rx_push(char c) {
    uint32_t next = (rx_head + 1) % RX_BUF_SIZE;
    if (next != rx_tail) {
        rx_buf[rx_head] = c;
        rx_head = next; // 保证数据完全写入后再推高指针
    }
}

static int rx_pop(void) {
    if (rx_head == rx_tail) {
        return -1;
    }
    char c = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) % RX_BUF_SIZE;
    return c;
}

static int rx_available(void) {
    return (rx_head - rx_tail + RX_BUF_SIZE) % RX_BUF_SIZE;
}

// -------------------------------------------------------------
// DEFECT-02: 互斥锁同步保护区定义 (防继电器控制竞态)
// -------------------------------------------------------------
SemaphoreHandle_t relayMutex = NULL;

// 原有控制及网络全局变量定义
const char* ssid = "CMCC-SUbF";
const char* password = "kuuf7747";
WebServer esp_server(80);

const int buttonPins[4] = {10, 9, 6, 8};
const int relayPins[4] = {3, 4, 5, 7};

unsigned long previousMillis = 0;
unsigned long relayOnMillis[4] = {0};
volatile bool relayOn[4] = {false};
bool lastButtonState[4] = { HIGH, HIGH, HIGH, HIGH };

const unsigned long resetInterval = 1 * 60 * 60 * 1000;
const unsigned long relayOnDuration = 10000;
unsigned long lastPrintMillis = 0;
const unsigned long printInterval = 1000;

// -------------------------------------------------------------
// GPIO Tcl 扩展命令实现 (与继电器及控制逻辑对齐)
// -------------------------------------------------------------
tcl_i32 tcl_cmd_gpio_mode(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 3) return TCL_ERROR;
    int pin = atoi((const char *)TO_PTR(context, arg_values[1]));
    int mode = atoi((const char *)TO_PTR(context, arg_values[2]));
    
    if (mode == 0) {
        pinMode(pin, INPUT);
    } else if (mode == 1) {
        pinMode(pin, OUTPUT);
    } else if (mode == 2) {
        pinMode(pin, INPUT_PULLUP);
    } else {
        return TCL_ERROR;
    }
    return TCL_OK;
}

tcl_i32 tcl_cmd_digital_write(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 3) return TCL_ERROR;
    int pin = atoi((const char *)TO_PTR(context, arg_values[1]));
    int val = atoi((const char *)TO_PTR(context, arg_values[2]));
    
    digitalWrite(pin, val);
    
    // 对齐原有继电器状态，并加互斥锁防并发竞态
    if (xSemaphoreTake(relayMutex, portMAX_DELAY) == pdTRUE) {
        for (int i = 0; i < 4; i++) {
            if (relayPins[i] == pin) {
                if (val == LOW) { // 继电器为低电平触发开启
                    relayOn[i] = true;
                    relayOnMillis[i] = millis();
                } else {
                    relayOn[i] = false;
                }
            }
        }
        xSemaphoreGive(relayMutex);
    }
    return TCL_OK;
}

tcl_i32 tcl_cmd_digital_read(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 2) return TCL_ERROR;
    int pin = atoi((const char *)TO_PTR(context, arg_values[1]));
    int val = digitalRead(pin);
    
    tcl_u32 res_offset = tcl_alc_p(context, 12);
    if (res_offset != TCL_NULL) {
        itoa(val, (char *)TO_PTR(context, res_offset), 10);
        context->result = res_offset;
    }
    return TCL_OK;
}

tcl_i32 tcl_cmd_tcl_shell_ansi(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 2) return TCL_ERROR;
    int val = atoi((const char *)TO_PTR(context, arg_values[1]));
    baretcl_use_ansi = val;
    return TCL_OK;
}

// -------------------------------------------------------------
// FreeRTOS Tcl 独立运行任务 (解决 DEFECT-01 & DEFECT-02)
// -------------------------------------------------------------
TaskHandle_t tclTaskHandle = NULL;

void tcl_task(void *pvParameters) {
    TclCtx *ctx = (TclCtx *)tcl_arena;
    tcl_init(ctx, tcl_arena, TCL_ARENA_SIZE);
    
    // 注册标准及扩展命令
    tcl_register_ext_cmds(ctx);
    tcl_register_c_cmd(ctx, (const tcl_u8 *)"gpio_mode", tcl_cmd_gpio_mode);
    tcl_register_c_cmd(ctx, (const tcl_u8 *)"digital_write", tcl_cmd_digital_write);
    tcl_register_c_cmd(ctx, (const tcl_u8 *)"digital_read", tcl_cmd_digital_read);
    tcl_register_c_cmd(ctx, (const tcl_u8 *)"tcl_shell_ansi", tcl_cmd_tcl_shell_ansi);

    static TclShell tcl_sh;
    shell_init(&tcl_sh);

    // 默认关闭 ANSI 兼容模式以迎合 Arduino 串口监视器，防止乱码
    baretcl_use_ansi = 0;

    tcl_hal_puts((const tcl_u8 *)"\r\n==============================================\r\n");
    tcl_hal_puts((const tcl_u8 *)"BareTcl Shell for ESP32 (RTOS & WDT Protected)\r\n");
    tcl_hal_puts((const tcl_u8 *)"==============================================\r\n");
    tcl_hal_puts((const tcl_u8 *)"Use 'gpio_mode', 'digital_write', 'digital_read' to control HW.\r\n");
    tcl_hal_puts((const tcl_u8 *)"Standard monitor mode: ANSI disabled. Run 'tcl_shell_ansi 1' to enable ANSI.\r\n> ");

    while (true) {
        if (rx_available() > 0) {
            char c = rx_pop();
            if (shell_handle_char(&tcl_sh, c, "> ") == 1) {
                int status = tcl_eval(ctx, tcl_sh.line);
                
                if (status == TCL_EXIT) {
                    tcl_hal_puts((const tcl_u8 *)"BareTcl exit command triggered. System rebooting...\r\n");
                    vTaskDelay(pdMS_TO_TICKS(500));
                    ESP.restart();
                } else if (status == TCL_ERROR) {
                    tcl_hal_puts((const tcl_u8 *)"Error: ");
                    tcl_hal_puts(tcl_get_result(ctx));
                    tcl_hal_puts((const tcl_u8 *)"\r\n");
                } else {
                    const tcl_u8 *res = tcl_get_result(ctx);
                    if (res && res[0]) {
                        tcl_hal_puts(res);
                        tcl_hal_puts((const tcl_u8 *)"\r\n");
                    }
                }

                // 清除行缓冲区并输出新提示符
                for (uint32_t i = 0; i < SHELL_MAX_LINE; i++) tcl_sh.line[i] = 0;
                tcl_sh.len = 0;
                tcl_sh.cursor = 0;
                tcl_hal_puts((const tcl_u8 *)"> ");
            }
        } else {
            // 无串口输入数据时睡眠挂起，交出 CPU 控制权
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

// -------------------------------------------------------------
// WebServer 回调函数
// -------------------------------------------------------------
void handleRoot() {
    esp_server.send(200, "text/plain", "1");
}

void handleNotFound() {
    esp_server.send(404, "text/plain", "404: Not found");
}

void changeRelay(int index) {
    digitalWrite(relayPins[index], LOW); // 低电平开启
    
    // 对齐状态，加锁防止并发写竞态
    if (xSemaphoreTake(relayMutex, portMAX_DELAY) == pdTRUE) {
        relayOnMillis[index] = millis();
        relayOn[index] = true;
        xSemaphoreGive(relayMutex);
    }
    esp_server.send(200, "text/plain", "0");
}

void toggleRelay(int index) {
    // 对齐状态并进行互斥同步保护，解决按键自锁不进入自动关闭保护的缺陷
    if (xSemaphoreTake(relayMutex, portMAX_DELAY) == pdTRUE) {
        int currentVal = digitalRead(relayPins[index]);
        int nextVal = !currentVal;
        digitalWrite(relayPins[index], nextVal);
        if (nextVal == LOW) { // 继电器开启
            relayOn[index] = true;
            relayOnMillis[index] = millis();
        } else {
            relayOn[index] = false;
        }
        xSemaphoreGive(relayMutex);
    }
}

void reset() {
    ESP.restart();
}

// -------------------------------------------------------------
// Setup & Loop
// -------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    
    // 创建继电器互斥锁
    relayMutex = xSemaphoreCreateMutex();
    
    // 初始化物理引脚
    for (int i = 0; i < 4; i++) {
        pinMode(buttonPins[i], INPUT_PULLUP);
        pinMode(relayPins[i], OUTPUT);
        digitalWrite(relayPins[i], HIGH); // 默认关闭
        delay(100); // 间隔启动，减少冲击电流
    }

    // 初始化 WiFi
    WiFi.begin(ssid, password);
    int conn_ticks = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        if (++conn_ticks >= 40) {
            Serial.println("\nWiFi connection timed out. Booting shell only...");
            break;
        }
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected successfully!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    }

    // 设置 WebServer 路由
    esp_server.on("/", handleRoot);
    for (int i = 0; i < 4; i++) {
        String path = "/change_relay" + String(i + 1);
        esp_server.on(path, [i]() { changeRelay(i); });
    }
    esp_server.onNotFound(handleNotFound);
    esp_server.begin();

    // 启动 FreeRTOS 任务来运行 BareTcl
    // L2级安全断言：BareTcl采用完全无栈的FSM设计模式，函数调用层级极浅且所有环境数据均静态存储于 Arena 中，
    // C语言物理系统堆栈几乎无消耗，所以分配 8KB 堆栈属于绝对冗余的安全规格，杜绝了 Stack Overflow 隐患。
    xTaskCreateUniversal(
        tcl_task,           // 任务入口函数
        "tcl_task",         // 任务名称
        8192,               // 任务堆栈大小
        NULL,               // 参数
        1,                  // 任务优先级
        &tclTaskHandle,     // 任务句柄
        1                   // 绑定核心 (与 loopTask 共用核心 1，通过 vTaskDelay 共享 CPU)
    );
}

void loop() {
    // 轮询 Web 客户端请求
    esp_server.handleClient();

    // 异步搬运串口数据 (SPSC 队列物理侧无锁搬运，防止粘贴溢出丢包)
    while (Serial.available() > 0) {
        rx_push(Serial.read());
    }

    unsigned long currentMillis = millis();

    // 检查定时复位
    if (currentMillis - previousMillis >= resetInterval) {
        previousMillis = currentMillis;
        reset();
    }

    // 打印当前系统运行秒数
    if (currentMillis - lastPrintMillis >= printInterval) {
        lastPrintMillis = currentMillis;
        Serial.print("Running time: ");
        Serial.print(currentMillis / 1000);
        Serial.println(" s");
    }

    // 检测按钮及继电器定时自动关闭逻辑
    for (int i = 0; i < 4; i++) {
        bool buttonState = digitalRead(buttonPins[i]);
        if (buttonState == LOW && lastButtonState[i] == HIGH) {
            toggleRelay(i);
        }
        lastButtonState[i] = buttonState;

        // 互斥同步读取继电器守护计时，防止状态并发损坏
        bool shouldClose = false;
        if (xSemaphoreTake(relayMutex, portMAX_DELAY) == pdTRUE) {
            if (relayOn[i] && (currentMillis - relayOnMillis[i] >= relayOnDuration)) {
                relayOn[i] = false;
                shouldClose = true;
            }
            xSemaphoreGive(relayMutex);
        }
        
        if (shouldClose) {
            digitalWrite(relayPins[i], HIGH); // 自动关闭继电器
        }
    }

    // 主动让出 1ms 的 CPU 资源，使同等优先级的 tcl_task 能顺畅执行
    vTaskDelay(pdMS_TO_TICKS(1));
}