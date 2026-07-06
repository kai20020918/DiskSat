#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"

// ICM-42688P I2C設定
#define ICM42688_I2C_ADDR      0x18  // SDO/SA0ピン = GND の場合[cite: 1]
#define I2C_PORT               i2c0
#define PIN_SDA                4     // 環境に合わせて変更してください
#define PIN_SCL                5     // 環境に合わせて変更してください

// UART1設定 (GPIO10, GPIO11)
#define UART_PORT              uart1
#define BAUD_RATE              115200
#define PIN_UART_TX            10
#define PIN_UART_RX            11

// ICM-42688P レジスタマップ[cite: 1]
#define REG_WHO_AM_I           0x01  // WHO_AM_Iレジスタ[cite: 1]
#define REG_ACCEL_DATA_XH      0x0C  // 加速度X軸 高バイト[cite: 1]
#define REG_SOFT_RST           0x4A  // ソフトリセットレジスタ[cite: 1]
#define REG_PWR_CTRL           0x7D  // 電源制御レジスタ[cite: 1]

// WHO_AM_I の期待値[cite: 1]
#define WHOAMI_EXPECTED        0x6A  // チップID期待値は0x6A[cite: 1]

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
        snprintf(tx_msg, sizeof(tx_msg), "Error: ICM-42688P not found. Read WHO_AM_I: 0x%02X\r\n", who_am_i);
        uart_puts(UART_PORT, tx_msg);
        return false;
    }

    // ソフトリセットの実行[cite: 1]
    icm42688_write_reg(REG_SOFT_RST, 0xA5); // 0xA5を書き込んで回路全体をリセット[cite: 1]
    sleep_ms(10); // リセット後の安定待ち

    // 電源制御と有効化[cite: 1]
    // データシートの指示通り、まずは0x7Dに0x0Eを書き込んで10ms延時します[cite: 1]
    // 0x0E = 温度センサ有効(TEMP_EN=1), 加速度計有効(ACC_EN=1), 陀螺儀有効(GYR_EN=1)[cite: 1]
    icm42688_write_reg(REG_PWR_CTRL, 0x0E); 
    sleep_ms(10); // 出荷時校正値の自動ロードと起動安定待ち[cite: 1]

    return true;
}

int main() {
    stdio_init_all();
    // 1. UART1の初期化
    uart_init(UART_PORT, BAUD_RATE);
    gpio_set_function(PIN_UART_TX, UART_FUNCSEL_NUM(UART_PORT, PIN_UART_TX));
    gpio_set_function(PIN_UART_RX, UART_FUNCSEL_NUM(UART_PORT, PIN_UART_RX));

    // 2. I2C周辺機能の初期化 (400kHz快速モード対応)[cite: 1]
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_SDA);
    gpio_pull_up(PIN_SCL);

    sleep_ms(1000); 
    uart_puts(UART_PORT, "Initializing ICM-42688P via UART1...\r\n");

    if (!icm42688_init()) {
        while (1) sleep_ms(1000);
    }

    uart_puts(UART_PORT, "ICM-42688P Initialized Successfully.\r\n");

    uint8_t data_buf[12];
    int16_t accel_x, accel_y, accel_z;
    int16_t gyro_x, gyro_y, gyro_z;

    while (true) {
        // 加速度XH(0x0C)から陀螺儀ZL(0x17)までの12バイトを一括読み込み（アドレス自動インクリメント）[cite: 1]
        icm42688_read_regs(REG_ACCEL_DATA_XH, data_buf, 12);

        // 2の補数形式のデータを16ビット符号付き整数に結合（高バイト -> 低バイトの順）[cite: 1]
        accel_x = (int16_t)((data_buf[0] << 8) | data_buf[1]);  // 0x0C, 0x0D[cite: 1]
        accel_y = (int16_t)((data_buf[2] << 8) | data_buf[3]);  // 0x0E, 0x0F[cite: 1]
        accel_z = (int16_t)((data_buf[4] << 8) | data_buf[5]);  // 0x10, 0x11[cite: 1]
        
        gyro_x  = (int16_t)((data_buf[6] << 8) | data_buf[7]);  // 0x12, 0x13[cite: 1]
        gyro_y  = (int16_t)((data_buf[8] << 8) | data_buf[9]);  // 0x14, 0x15[cite: 1]
        gyro_z  = (int16_t)((data_buf[10] << 8) | data_buf[11]);// 0x16, 0x17[cite: 1]

        // 表示文字列を設定
        snprintf(tx_msg, sizeof(tx_msg), 
                 "Accel: X=%6d  Y=%6d  Z=%6d  |  Gyro: X=%6d  Y=%6d  Z=%6d\r\n",
                 accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z);

        // UART1へ送信[cite: 1]
        uart_puts(UART_PORT, tx_msg);

        sleep_ms(100);
    }
}