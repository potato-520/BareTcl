#!/bin/bash
set -e

# 1. 载入 ESP-IDF 环境变量
if [ -f "$HOME/esp/esp-idf/export.sh" ]; then
    . "$HOME/esp/esp-idf/export.sh"
else
    echo "Error: Cannot find ESP-IDF export.sh at $HOME/esp/esp-idf/export.sh"
    exit 1
fi

# 2. 进入当前脚本所在目录确保路径正确
cd "$(dirname "$0")"

# 2.5. 编译 ESP32 自定义 Tcl 库为 C 文件，以及生成标准自举库
echo "=== Generating standard Tcl library ==="
python3 ../tools/tcl2c.py ../src/tcllib.tcl ../src/tcllib.c

echo "=== Generating ESP32 Tcl library ==="
python3 tcl2c_esp32.py esp32_lib.tcl main/esp32_lib.c esp32_bootstrap
python3 tcl2c_esp32.py console.html main/console_html.c console_html

# 3. 执行编译
echo "=== Starting ESP-IDF Build ==="
idf.py build
echo "=== Build Completed Successfully ==="
