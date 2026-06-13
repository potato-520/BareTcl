#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "esp_system.h"
#include <fcntl.h>
#include "driver/uart_vfs.h"
#include <termios.h>
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"

// -------------------------------------------------------------
// BareTcl Integration Definitions
// -------------------------------------------------------------
#define TCL_YIELD_HOOK() do { \
    static uint32_t __yield_cnt = 0; \
    if (++__yield_cnt >= 2000) { \
        __yield_cnt = 0; \
        vTaskDelay(1); \
    } \
} while(0)

#include "../../src/tcl_core.c"
#include "../../src/extcmd.c"
#include "../../src/baretcl_shell.c"
#include "esp32_lib.c"

// Forward declaration of C command handlers
tcl_i32 tcl_cmd_gpio_mode(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values);
tcl_i32 tcl_cmd_digital_write(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values);
tcl_i32 tcl_cmd_digital_read(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values);
tcl_i32 tcl_cmd_tcl_shell_ansi(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values);
tcl_i32 tcl_cmd_log(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values);


// Memory Arena Physical Layout
#define TCL_ARENA_SIZE (48 * 1024)
static char tcl_arena[TCL_ARENA_SIZE];

// HAL Put String redirection
void tcl_hal_puts(const tcl_u8 *s) {
    printf("%s", (const char *)s);
    fflush(stdout);
}

// -------------------------------------------------------------
// Hardware & Thread Safety State
// -------------------------------------------------------------
static SemaphoreHandle_t relayMutex = NULL;
volatile bool tcl_task_running = false;
volatile int tcl_task_create_res = -99;
volatile bool enable_log_print = false;
volatile bool log_explicitly_set = false;


const gpio_num_t buttonPins[4] = {GPIO_NUM_10, GPIO_NUM_9, GPIO_NUM_6, GPIO_NUM_8};
const gpio_num_t relayPins[4] = {GPIO_NUM_3, GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_7};

static uint64_t relayOnMillis[4] = {0};
static volatile bool relayOn[4] = {false};

// -------------------------------------------------------------
// GPIO Tcl Extension Commands
// -------------------------------------------------------------
tcl_i32 tcl_cmd_gpio_mode(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 3) return TCL_ERROR;
    int pin = atoi((const char *)TO_PTR(context, arg_values[1]));
    int mode = atoi((const char *)TO_PTR(context, arg_values[2]));
    
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .pin_bit_mask = (1ULL << pin),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE
    };
    if (mode == 0) { // INPUT
        io_conf.mode = GPIO_MODE_INPUT;
    } else if (mode == 1) { // OUTPUT
        io_conf.mode = GPIO_MODE_OUTPUT;
    } else if (mode == 2) { // INPUT_PULLUP
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    } else {
        return TCL_ERROR;
    }
    gpio_config(&io_conf);
    return TCL_OK;
}

tcl_i32 tcl_cmd_digital_write(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 3) return TCL_ERROR;
    int pin = atoi((const char *)TO_PTR(context, arg_values[1]));
    int val = atoi((const char *)TO_PTR(context, arg_values[2]));
    
    gpio_set_level(pin, val);
    
    if (xSemaphoreTake(relayMutex, portMAX_DELAY) == pdTRUE) {
        for (int i = 0; i < 4; i++) {
            if (relayPins[i] == pin) {
                if (val == 0) { // Low triggers relay
                    relayOn[i] = true;
                    relayOnMillis[i] = esp_timer_get_time() / 1000;
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
    int val = gpio_get_level(pin);
    
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

tcl_i32 tcl_cmd_log(TclCtx *context, tcl_i32 arg_count, tcl_u32 *arg_values) {
    if (arg_count < 2) {
        tcl_u32 res_offset = tcl_alc_p(context, 16);
        if (res_offset != TCL_NULL) {
            char *dest = (char *)TO_PTR(context, res_offset);
            if (dest != NULL) {
                if (enable_log_print) {
                    dest[0] = 'o'; dest[1] = 'n'; dest[2] = '\0';
                } else {
                    dest[0] = 'o'; dest[1] = 'f'; dest[2] = 'f'; dest[3] = '\0';
                }
                context->result = res_offset;
            }
        }
        return TCL_OK;
    }
    const char *action = (const char *)TO_PTR(context, arg_values[1]);
    if (strcmp(action, "on") == 0) {
        enable_log_print = true;
        log_explicitly_set = true;
        esp_log_level_set("wifi", ESP_LOG_INFO);
        esp_log_level_set("WIFI", ESP_LOG_INFO);
        esp_log_level_set("*", ESP_LOG_INFO);
        printf("[INFO] Log printing turned ON\n");
    } else if (strcmp(action, "off") == 0) {
        enable_log_print = false;
        log_explicitly_set = true;
        esp_log_level_set("wifi", ESP_LOG_NONE);
        esp_log_level_set("WIFI", ESP_LOG_NONE);
        esp_log_level_set("*", ESP_LOG_WARN);
        printf("[INFO] Log printing turned OFF\n");
    } else {
        return TCL_ERROR;
    }
    return TCL_OK;
}

// -------------------------------------------------------------
// FreeRTOS Tcl Task
// -------------------------------------------------------------
void tcl_task(void *pvParameters) {
    tcl_task_running = true;

    // Disable stdin/stdout buffering for this task's reent structure in FreeRTOS
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stdin, NULL, _IONBF, 0);

    printf("[DIAG] tcl_task started!\n");
    fflush(stdout);

    TclCtx *ctx = (TclCtx *)tcl_arena;
    tcl_init(tcl_arena, TCL_ARENA_SIZE);
    
    tcl_register_ext_cmds(ctx);
    tcl_register_c_cmd((const tcl_u8 *)"gpio_mode", tcl_cmd_gpio_mode);
    tcl_register_c_cmd((const tcl_u8 *)"digital_write", tcl_cmd_digital_write);
    tcl_register_c_cmd((const tcl_u8 *)"digital_read", tcl_cmd_digital_read);
    tcl_register_c_cmd((const tcl_u8 *)"tcl_shell_ansi", tcl_cmd_tcl_shell_ansi);
    tcl_register_c_cmd((const tcl_u8 *)"log", tcl_cmd_log);

    // Load standard Tcl bootstrap library (defines 'for', 'foreach', 'incr', 'lappend', etc.)
    if (tcl_load_bootstrap(ctx) != TCL_OK) {
        printf("[ERROR] Failed to load standard bootstrap library: %s\n", (const char *)tcl_get_result(ctx));
    }

    // Evaluate the compiled Tcl bootstrap script (defines help, etc.)
    tcl_eval(ctx, (const tcl_u8 *)esp32_bootstrap);

    static TclShell tcl_sh;
    shell_init(&tcl_sh);
    baretcl_use_ansi = 1; // Default on since putty/idf_monitor supports ANSI

    printf("\r\n==============================================\r\n");
    printf("BareTcl Shell for ESP32 (ESP-IDF Native C)\r\n");
    printf("==============================================\r\n");
    printf("Use 'gpio_mode', 'digital_write', 'digital_read' to control HW.\r\n");
    printf("Standard monitor: ANSI enabled. Run 'tcl_shell_ansi 0' to disable ANSI.\r\n\x1b[0m> ");
    fflush(stdout);

    // Set stdin to non-blocking mode and print diagnostics
    int fd = fileno(stdin);
    int fcntl_res = fcntl(fd, F_SETFL, O_NONBLOCK);
    printf("[DIAG] stdin fd: %d, fcntl set non-block result: %d\n", fd, fcntl_res);
    fflush(stdout);

    uint64_t last_diag_print = esp_timer_get_time() / 1000;

    while (true) {
        int r;
        bool progress = false;
        while ((r = fgetc(stdin)) != EOF) {
            progress = true;
            uint8_t c = (uint8_t)r;
            if (enable_log_print) {
                printf("[DIAG] Read char: 0x%02X (%c)\n", c, (c >= 32 && c < 127) ? c : ' ');
                fflush(stdout);
            }
            if (shell_handle_char(&tcl_sh, c, "> ") == 1) {
                int status = tcl_eval(ctx, tcl_sh.line);
                
                if (status == TCL_EXIT) {
                    printf("BareTcl exit command triggered. System rebooting...\r\n");
                    vTaskDelay(pdMS_TO_TICKS(500));
                    esp_restart();
                } else if (status == TCL_ERROR) {
                    printf("Error: %s\r\n", tcl_get_result(ctx));
                } else {
                    const tcl_u8 *res = tcl_get_result(ctx);
                    if (res && res[0]) {
                        printf("%s\r\n", res);
                    }
                }
                fflush(stdout);

                // Clear line buffer
                memset(tcl_sh.line, 0, SHELL_MAX_LINE);
                tcl_sh.len = 0;
                tcl_sh.cursor = 0;
                if (baretcl_use_ansi) {
                    printf("\x1b[0m> ");
                } else {
                    printf("> ");
                }
                fflush(stdout);
            }
        }

        uint64_t now = esp_timer_get_time() / 1000;
        if (enable_log_print && (now - last_diag_print >= 5000)) {
            last_diag_print = now;
            printf("[DIAG] tcl_task loop alive, reading stdin...\n");
            fflush(stdout);
        }

        if (!progress) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

// -------------------------------------------------------------
// WebServer HTTP Event Handlers
// -------------------------------------------------------------
static esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_send(req, "1", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t relay_change_handler(httpd_req_t *req) {
    int index = -1;
    if (strstr(req->uri, "change_relay1")) index = 0;
    else if (strstr(req->uri, "change_relay2")) index = 1;
    else if (strstr(req->uri, "change_relay3")) index = 2;
    else if (strstr(req->uri, "change_relay4")) index = 3;

    if (index >= 0 && index < 4) {
        gpio_set_level(relayPins[index], 0); // Active low
        if (xSemaphoreTake(relayMutex, portMAX_DELAY) == pdTRUE) {
            relayOnMillis[index] = esp_timer_get_time() / 1000;
            relayOn[index] = true;
            xSemaphoreGive(relayMutex);
        }
        httpd_resp_send(req, "0", HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Relay not found");
    }
    return ESP_OK;
}

httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_root = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler };
        httpd_register_uri_handler(server, &uri_root);

        httpd_uri_t uri_relay1 = { .uri = "/change_relay1", .method = HTTP_GET, .handler = relay_change_handler };
        httpd_register_uri_handler(server, &uri_relay1);
        httpd_uri_t uri_relay2 = { .uri = "/change_relay2", .method = HTTP_GET, .handler = relay_change_handler };
        httpd_register_uri_handler(server, &uri_relay2);
        httpd_uri_t uri_relay3 = { .uri = "/change_relay3", .method = HTTP_GET, .handler = relay_change_handler };
        httpd_register_uri_handler(server, &uri_relay3);
        httpd_uri_t uri_relay4 = { .uri = "/change_relay4", .method = HTTP_GET, .handler = relay_change_handler };
        httpd_register_uri_handler(server, &uri_relay4);
    }
    return server;
}

// -------------------------------------------------------------
// Wi-Fi Connection Management
// -------------------------------------------------------------
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        if (enable_log_print) {
            ESP_LOGI("WIFI", "Retrying AP connection...");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        if (enable_log_print) {
            ESP_LOGI("WIFI", "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        }
    }
}

void wifi_init_sta(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "CMCC-SUbF",
            .password = "kuuf7747",
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

// -------------------------------------------------------------
// Hardware Initialization
// -------------------------------------------------------------
void init_gpio(void) {
    // Config buttons (Input, Pull-Up)
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << GPIO_NUM_10) | (1ULL << GPIO_NUM_9) | (1ULL << GPIO_NUM_6) | (1ULL << GPIO_NUM_8),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE
    };
    gpio_config(&io_conf);

    // Config relays (Output)
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_NUM_3) | (1ULL << GPIO_NUM_4) | (1ULL << GPIO_NUM_5) | (1ULL << GPIO_NUM_7);
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    // Turn off all relays initially (High)
    for (int i = 0; i < 4; i++) {
        gpio_set_level(relayPins[i], 1);
        vTaskDelay(pdMS_TO_TICKS(100)); // Sequential delay to reduce current spikes
    }
}

void init_uart(void) {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    // Install UART driver on UART0 for reading console input
    uart_driver_install(UART_NUM_0, 2048, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &uart_config);
    // Bind VFS to the UART driver
    uart_vfs_dev_use_driver(UART_NUM_0);
}

void init_usb_serial_jtag(void) {
    usb_serial_jtag_driver_config_t usb_serial_jtag_config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    // Install driver with default configuration (internal FIFO buffers size 512 by default)
    usb_serial_jtag_driver_install(&usb_serial_jtag_config);
    // Tell VFS to use the installed driver for console
    usb_serial_jtag_vfs_use_driver();
}

// -------------------------------------------------------------
// Main Application Entry
// -------------------------------------------------------------
void app_main(void) {
    // Disable all verbose/info logs immediately upon startup
    esp_log_level_set("wifi", ESP_LOG_NONE);
    esp_log_level_set("WIFI", ESP_LOG_NONE);
    esp_log_level_set("*", ESP_LOG_WARN);

    // Initialize NVS for Wi-Fi storage
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    relayMutex = xSemaphoreCreateMutex();
    init_gpio();
    init_uart();
    init_usb_serial_jtag();

    // Disable stdin/stdout buffering for real-time character echo after VFS UART driver is registered
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stdin, NULL, _IONBF, 0);

    // Disable canonical mode (ICANON) and echo (ECHO) on stdin to enable real-time raw input
    struct termios t;
    int getattr_res = tcgetattr(fileno(stdin), &t);
    printf("[DIAG] tcgetattr result: %d, errno: %d\n", getattr_res, getattr_res == 0 ? 0 : errno);
    fflush(stdout);
    if (getattr_res == 0) {
        t.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG | IEXTEN);
        int setattr_res = tcsetattr(fileno(stdin), TCSANOW, &t);
        printf("[DIAG] tcsetattr result: %d, errno: %d\n", setattr_res, setattr_res == 0 ? 0 : errno);
        fflush(stdout);
    }

    wifi_init_sta();
    start_webserver();

    // Start BareTcl Task
    tcl_task_create_res = xTaskCreate(tcl_task, "tcl_task", 8192, NULL, 5, NULL);

    // Main Control Loop (Handles button polling & automatic relay shut-off)
    bool lastButtonState[4] = { 1, 1, 1, 1 };
    uint64_t previousMillis = esp_timer_get_time() / 1000;
    uint64_t lastPrintMillis = esp_timer_get_time() / 1000;
    const uint64_t resetInterval = 1 * 60 * 60 * 1000; // 1 hour reboot
    const uint64_t printInterval = 1000; // 1 second prints
    const uint64_t relayOnDuration = 10000; // 10 second auto shut-off

    while (true) {
        uint64_t currentMillis = esp_timer_get_time() / 1000;

        // Auto restart check
        if (currentMillis - previousMillis >= resetInterval) {
            esp_restart();
        }

        // Print runtime and task state
        if (enable_log_print && (currentMillis - lastPrintMillis >= printInterval)) {
            lastPrintMillis = currentMillis;
            printf("Running time: %lld s, task_created: %d, task_running: %d\n", 
                   currentMillis / 1000, tcl_task_create_res, tcl_task_running);
            fflush(stdout);
        }

        // Poll button presses
        for (int i = 0; i < 4; i++) {
            bool buttonState = gpio_get_level(buttonPins[i]);
            if (buttonState == 0 && lastButtonState[i] == 1) { // Pressed (Active Low)
                // Toggle relay state
                if (xSemaphoreTake(relayMutex, portMAX_DELAY) == pdTRUE) {
                    int currentVal = gpio_get_level(relayPins[i]);
                    int nextVal = !currentVal;
                    gpio_set_level(relayPins[i], nextVal);
                    if (nextVal == 0) {
                        relayOn[i] = true;
                        relayOnMillis[i] = esp_timer_get_time() / 1000;
                    } else {
                        relayOn[i] = false;
                    }
                    xSemaphoreGive(relayMutex);
                }
            }
            lastButtonState[i] = buttonState;

            // Handle automatic relay turn-off timing
            bool shouldClose = false;
            if (xSemaphoreTake(relayMutex, portMAX_DELAY) == pdTRUE) {
                if (relayOn[i] && (currentMillis - relayOnMillis[i] >= relayOnDuration)) {
                    relayOn[i] = false;
                    shouldClose = true;
                }
                xSemaphoreGive(relayMutex);
            }
            
            if (shouldClose) {
                gpio_set_level(relayPins[i], 1); // Turn off (High)
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
