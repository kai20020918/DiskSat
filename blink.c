#include "pico/stdlib.h"

// 使用するGPIOピンの定義
#define LED_PIN_13 13
#define LED_PIN_15 15
#define LED_DELAY_MS 500

int main() {
    // 標準入出力（USBシリアルなど）の初期化（デバッグ用に便利です）
    stdio_init_all();

    // GPIO 13 の初期化と出力設定
    gpio_init(LED_PIN_13);
    gpio_set_dir(LED_PIN_13, GPIO_OUT);

    // GPIO 15 の初期化と出力設定
    gpio_init(LED_PIN_15);
    gpio_set_dir(LED_PIN_15, GPIO_OUT);

    while (true) {
        // --- 13をON / 15をOFF ---
        gpio_put(LED_PIN_13, 1);
        gpio_put(LED_PIN_15, 0);
        sleep_ms(LED_DELAY_MS);

        // --- 13をOFF / 15をON ---
        gpio_put(LED_PIN_13, 0);
        gpio_put(LED_PIN_15, 1);
        sleep_ms(LED_DELAY_MS);
    }
}