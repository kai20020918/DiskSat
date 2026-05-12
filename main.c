#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/i2c.h"

// --- 設定 ---
#define UART_ID          uart1
#define BAUD_RATE        115200
#define UART_TX_PIN      10
#define UART_RX_PIN      11

#define I2C_PORT         i2c0
#define I2C_SDA          4
#define I2C_SCL          5

// TMP1075設定 (A0, A1, A2がすべてGNDに接続されている場合)
#define TMP_ADDR         0x48 
#define REG_TMP_TEMP      0x00

// 温度読み取り関数
float read_tmp1075() {
    uint8_t reg = REG_TMP_TEMP;
    uint8_t buf[2];

    // 1. ポインタレジスタをセットして読み取り [cite: 591, 597]
    if (i2c_write_blocking(I2C_PORT, TMP_ADDR, &reg, 1, true) < 0) return -999.0f;
    if (i2c_read_blocking(I2C_PORT, TMP_ADDR, buf, 2, false) < 0) return -999.0f;

    // 2. 12ビットデータの結合 [cite: 526, 970]
    // 上位バイト(buf[0])と下位バイト(buf[1])の上位4ビットを使用
    int16_t raw = (buf[0] << 4) | (buf[1] >> 4);

    // 3. 2の補数形式（負数）の処理 [cite: 527, 970]
    if (raw > 0x7FF) {
        raw |= 0xF000;
    }

    // 4. 分解能 0.0625°C を掛けて実数に変換 [cite: 36, 976]
    return raw * 0.0625f;
}

int main() {
    // UART1の初期化 (GPIO 10, 11)
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_TX_PIN));
    gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_RX_PIN));

    // I2C0の初期化 (GPIO 4, 5)
    i2c_init(I2C_PORT, 100 * 1000); 
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    char out_buf[64];
    uart_puts(UART_ID, "TMP1075 Standalone Monitor Start\r\n");

    while (1) {
        float temp = read_tmp1075();

        if (temp < -900.0f) {
            uart_puts(UART_ID, "I2C Error!\r\n");
        } else {
            sprintf(out_buf, "Temperature: %.4f C\r\n", temp);
            uart_puts(UART_ID, out_buf);
        }

        // 500msごとに更新
        sleep_ms(500);
    }
}