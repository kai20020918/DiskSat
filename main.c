/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include <stdio.h>

// I2C設定 (GP4, GP5)
#define I2C_PORT i2c0
#define PIN_SDA 4
#define PIN_SCL 5

// UART1設定 (GPIO10, GPIO11)
#define UART_PORT uart1
#define BAUD_RATE 115200
#define PIN_UART_TX 10
#define PIN_UART_RX 11

// UART送信用バッファ
char tx_msg[128];

// 特殊用途の予約アドレスをスキップする判定関数
bool reserved_addr(uint8_t addr) {
  return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

int main() {
  // 1. UART1の初期化
  uart_init(UART_PORT, BAUD_RATE);
  gpio_set_function(PIN_UART_TX, UART_FUNCSEL_NUM(UART_PORT, PIN_UART_TX));
  gpio_set_function(PIN_UART_RX, UART_FUNCSEL_NUM(UART_PORT, PIN_UART_RX));
  // 2. I2C初期化 (100kHz)
  i2c_init(I2C_PORT, 100 * 1000);
  gpio_set_function(PIN_SDA, GPIO_FUNC_I2C);
  gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(PIN_SDA);
  gpio_pull_up(PIN_SCL);

  sleep_ms(2000); // シリアル接続待ちウェイト

  uart_puts(UART_PORT, "\r\n=== I2C Bus Scan (via UART1) ===\r\n");
  uart_puts(UART_PORT,
            "    0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\r\n");

  for (int addr = 0; addr < (1 << 7); ++addr) {
    if (addr % 16 == 0) {
      snprintf(tx_msg, sizeof(tx_msg), "%02x ", addr);
      uart_puts(UART_PORT, tx_msg);
    }

    int ret;
    uint8_t rxdata;
    if (reserved_addr(addr)) {
      ret = PICO_ERROR_GENERIC;
    } else {
      // 1バイトのダミー読み込みを試行
      ret = i2c_read_blocking(I2C_PORT, addr, &rxdata, 1, false);
    }

    // 応答があれば '@'、なければ '.' を出力
    if (ret < 0) {
      uart_puts(UART_PORT, ".");
    } else {
      uart_puts(UART_PORT, "@");
    }

    // 行末の処理
    if (addr % 16 == 15) {
      uart_puts(UART_PORT, "\r\n");
    } else {
      uart_puts(UART_PORT, "  ");
    }
  }

  uart_puts(UART_PORT, "Scan Done.\r\n");

  while (1) {
    sleep_ms(1000);
  }
  return 0;
}