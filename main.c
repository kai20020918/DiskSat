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

// AK09918 レジスタ定義
#define AK09918_ADDR     0x0C  // [cite: 2422]
#define REG_WIA1         0x00  // Company ID (期待値: 0x48) [cite: 2569, 2593]
#define REG_WIA2         0x01  // Device ID (期待値: 0x0C) [cite: 2569, 2595]
#define REG_ST1          0x10  // Status 1 (DRDYチェック用) [cite: 2569, 2600]
#define REG_HXL          0x11  // X軸データ下位 (ここから6バイトがXYZ) [cite: 2569, 2616]
#define REG_ST2          0x18  // Status 2 (読み取り終了通知に必須) [cite: 2569, 2632]
#define REG_CNTL2        0x31  // 動作モード設定 [cite: 2569, 2647]
#define REG_CNTL3        0x32  // ソフトリセット [cite: 2569, 2664]

// AK09918 初期化
void ak09918_init() {
    uint8_t buf[2];
    char msg[128];

    // 1. デバイス確認 (WIA1 & WIA2)
    uint8_t wia[2];
    uint8_t reg_wia = REG_WIA1;
    i2c_write_blocking(I2C_PORT, AK09918_ADDR, &reg_wia, 1, true);
    i2c_read_blocking(I2C_PORT, AK09918_ADDR, wia, 2, false);
    
    sprintf(msg, "AK09918 Check: Company=0x%02X, Device=0x%02X\r\n", wia[0], wia[1]);
    uart_puts(UART_ID, msg);

    if (wia[0] != 0x48 || wia[1] != 0x0C) {
        uart_puts(UART_ID, "Error: AK09918 not found!\r\n");
    }

    // 2. ソフトリセット [cite: 2031, 2668]
    buf[0] = REG_CNTL3;
    buf[1] = 0x01; // SRST = 1
    i2c_write_blocking(I2C_PORT, AK09918_ADDR, buf, 2, false);
    sleep_ms(100);

    // 3. 動作モード設定 (継続測定モード4: 100Hz) [cite: 2049, 2654]
    buf[0] = REG_CNTL2;
    buf[1] = 0x08; // Continuous measurement mode 4
    i2c_write_blocking(I2C_PORT, AK09918_ADDR, buf, 2, false);
    
    uart_puts(UART_ID, "AK09918 Initialized (100Hz Mode).\r\n");
}

int main() {
    // UART初期化
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_TX_PIN));
    gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_RX_PIN));

    // I2C初期化 (Fast Mode 400kHz) [cite: 1985]
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ak09918_init();

    while (1) {
        uint8_t status;
        uint8_t reg_st1 = REG_ST1;
        
        // Data Ready (DRDY) チェック [cite: 2176, 2603]
        i2c_write_blocking(I2C_PORT, AK09918_ADDR, &reg_st1, 1, true);
        i2c_read_blocking(I2C_PORT, AK09918_ADDR, &status, 1, false);

        if (status & 0x01) { // DRDY bit
            uint8_t raw_data[6];
            uint8_t reg_hxl = REG_HXL;

            // X, Y, Z各軸のデータを一括読み取り (リトルエンディアン) [cite: 2616, 2619]
            i2c_write_blocking(I2C_PORT, AK09918_ADDR, &reg_hxl, 1, true);
            i2c_read_blocking(I2C_PORT, AK09918_ADDR, raw_data, 6, false);

            // 重要: データ保護を解除するためにST2レジスタを必ず読む 
            uint8_t dummy_st2;
            uint8_t reg_st2 = REG_ST2;
            i2c_write_blocking(I2C_PORT, AK09918_ADDR, &reg_st2, 1, true);
            i2c_read_blocking(I2C_PORT, AK09918_ADDR, &dummy_st2, 1, false);

            // 16ビットデータに結合 (2の補数) [cite: 2619]
            int16_t mag_x = (int16_t)(raw_data[1] << 8 | raw_data[0]);
            int16_t mag_y = (int16_t)(raw_data[3] << 8 | raw_data[2]);
            int16_t mag_z = (int16_t)(raw_data[5] << 8 | raw_data[4]);

            // 実データ(uT)に変換 (0.15 uT/LSB) 
            float ut_x = mag_x * 0.15f;
            float ut_y = mag_y * 0.15f;
            float ut_z = mag_z * 0.15f;

            char out[128];
            sprintf(out, "MAG(uT): X=%7.2f Y=%7.2f Z=%7.2f\r\n", ut_x, ut_y, ut_z);
            uart_puts(UART_ID, out);
        }

        // 表示の負荷を下げるために少し待機
        sleep_ms(50);
    }
}