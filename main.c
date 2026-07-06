#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"

// ICM-42688-P I2C設定
#define ICM42688_I2C_ADDR      0x68  // AP_ADO = GND の場合[cite: 1]
#define I2C_PORT               i2c0
#define PIN_SDA                4     // 環境に合わせて変更してください
#define PIN_SCL                5     // 環境に合わせて変更してください

// UART1設定 (GPIO10, GPIO11)
#define UART_PORT              uart1
#define BAUD_RATE              115200
#define PIN_UART_TX            10
#define PIN_UART_RX            11

// ICM-42688-P レジスタマップ（Bank 0）[cite: 1]
#define REG_DEVICE_CONFIG      0x11
#define REG_PWR_MGMT0          0x4E
#define REG_WHO_AM_I           0x75
#define REG_ACCEL_DATA_X1      0x1F

// WHO_AM_I の期待値[cite: 1]
#define WHOAMI_EXPECTED        0x47

// UART送信用の文字列バッファ
char tx_msg[128];

// I2C書き込みヘルパー関数
void icm42688_write_reg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    i2c_write_blocking(I2C_PORT, ICM42688_I2C_ADDR, buf, 2, false);
}

// I2C読み込みヘルパー関数
void icm42688_read_regs(uint8_t reg, uint8_t *buf, uint16_t len) {
    i2c_write_blocking(I2C_PORT, ICM42688_I2C_ADDR, &reg, 1, true);
    i2c_read_blocking(I2C_PORT, ICM42688_I2C_ADDR, buf, len, false);
}

// 初期化関数
bool icm42688_init() {
    uint8_t who_am_i = 0;
    
    // デバイス確認[cite: 1]
    icm42688_read_regs(REG_WHO_AM_I, &who_am_i, 1);
    if (who_am_i != WHOAMI_EXPECTED) {
        snprintf(tx_msg, sizeof(tx_msg), "Error: ICM-42688-P not found. Read WHO_AM_I: 0x%02X\r\n", who_am_i);
        uart_puts(UART_PORT, tx_msg);
        return false;
    }

    // ソフトリセットの実行[cite: 1]
    icm42688_write_reg(REG_DEVICE_CONFIG, 0x01); // SOFT_RESET_CONFIG = 1[cite: 1]
    sleep_ms(2); // リセット後1ms以上待つ[cite: 1]

    // 加速度・ジャイロを「Low Noise (LN) モード」で有効化[cite: 1]
    // PWR_MGMT0 (0x4E): [bit3:2] GYRO_MODE = 11 (LN), [bit1:0] ACCEL_MODE = 11 (LN)[cite: 1]
    icm42688_write_reg(REG_PWR_MGMT0, 0x0F);
    sleep_ms(50); // 起動安定待ち（ジャイロは最小45ms必要）[cite: 1]

    return true;
}

int main() {
    stdio_init_all();
    // 1. UART1の初期化（標準stdioの初期化は不要）
    uart_init(UART_PORT, BAUD_RATE);
    gpio_set_function(PIN_UART_TX, UART_FUNCSEL_NUM(UART_PORT,PIN_UART_TX));
    gpio_set_function(PIN_UART_RX, UART_FUNCSEL_NUM(UART_PORT,PIN_UART_RX));

    // 2. I2C周辺機能の初期化 (400kHz)[cite: 1]
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_SDA);
    gpio_pull_up(PIN_SCL);

    sleep_ms(1000); 
    uart_puts(UART_PORT, "Initializing ICM-42688-P via UART1...\r\n");

    if (!icm42688_init()) {
        while (1) sleep_ms(1000);
    }

    uart_puts(UART_PORT, "ICM-42688-P Initialized Successfully.\r\n");

    uint8_t data_buf[12];
    int16_t accel_x, accel_y, accel_z;
    int16_t gyro_x, gyro_y, gyro_z;

    while (true) {
        // 加速度X1(0x1F)からジャイロZ0(0x2A)までの12バイトを一括読み込み[cite: 1]
        icm42688_read_regs(REG_ACCEL_DATA_X1, data_buf, 12);

        // ビッグエンディアン形式のデータを16ビット符号付き整数に結合[cite: 1]
        accel_x = (int16_t)((data_buf[0] << 8) | data_buf[1]);
        accel_y = (int16_t)((data_buf[2] << 8) | data_buf[3]);
        accel_z = (int16_t)((data_buf[4] << 8) | data_buf[5]);
        
        gyro_x  = (int16_t)((data_buf[6] << 8) | data_buf[7]);
        gyro_y  = (int16_t)((data_buf[8] << 8) | data_buf[9]);
        gyro_z  = (int16_t)((data_buf[10] << 8) | data_buf[11]);

        // 表示文字列を設定（改行コードは一般的なテレタイプ用の \r\n としています）[cite: 1]
        snprintf(tx_msg, sizeof(tx_msg), 
                 "Accel: X=%6d  Y=%6d  Z=%6d  |  Gyro: X=%6d  Y=%6d  Z=%6d\r\n",
                 accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z);

        // UART1へ送信[cite: 1]
        uart_puts(UART_PORT, tx_msg);

        sleep_ms(100);
    }
}