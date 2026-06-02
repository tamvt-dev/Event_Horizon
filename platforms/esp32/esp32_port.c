/*
 * EventHorizon Engine - ESP32 Platform Port
 * 
 * Platform-specific implementations for ESP32
 * - Memory allocation wrappers
 * - Timing functions
 * - Logging
 */

#include <esp_system.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "esp32_port.h"

static const char *TAG = "EH_ESP32";

/*
 * Memory allocation for ESP32
 * Prefer PSRAM if available, fall back to internal RAM
 */
void *eh_esp32_malloc(size_t size) {
    void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (!ptr) {
        ptr = heap_caps_malloc(size, MALLOC_CAP_8BIT);
    }
    return ptr;
}

void *eh_esp32_calloc(size_t nmemb, size_t size) {
    void *ptr = heap_caps_calloc(nmemb, size, MALLOC_CAP_SPIRAM);
    if (!ptr) {
        ptr = heap_caps_calloc(nmemb, size, MALLOC_CAP_8BIT);
    }
    return ptr;
}

void eh_esp32_free(void *ptr) {
    if (ptr) {
        heap_caps_free(ptr);
    }
}

/*
 * High-resolution timing (microseconds)
 */
uint64_t eh_esp32_micros(void) {
    return esp_timer_get_time();
}

uint64_t eh_esp32_millis(void) {
    return esp_timer_get_time() / 1000;
}

/*
 * Memory statistics
 */
size_t eh_esp32_free_heap(void) {
    return esp_get_free_heap_size();
}

size_t eh_esp32_free_psram(void) {
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

size_t eh_esp32_min_free_heap(void) {
    return esp_get_minimum_free_heap_size();
}

/*
 * Task delay (FreeRTOS)
 */
void eh_esp32_delay_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void eh_esp32_delay_us(uint32_t us) {
    ets_delay_us(us);
}

/*
 * Logging wrapper
 */
void eh_esp32_log(eh_log_level_t level, const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    switch (level) {
        case EH_LOG_ERROR:
            esp_log_writev(ESP_LOG_ERROR, TAG, format, args);
            break;
        case EH_LOG_WARN:
            esp_log_writev(ESP_LOG_WARN, TAG, format, args);
            break;
        case EH_LOG_INFO:
            esp_log_writev(ESP_LOG_INFO, TAG, format, args);
            break;
        case EH_LOG_DEBUG:
            esp_log_writev(ESP_LOG_DEBUG, TAG, format, args);
            break;
    }
    
    va_end(args);
}

/*
 * CPU frequency
 */
uint32_t eh_esp32_cpu_freq_mhz(void) {
    return esp_clk_cpu_freq() / 1000000;
}

/*
 * Watchdog control
 */
void eh_esp32_feed_watchdog(void) {
    // Feed task watchdog
    // Note: In actual use, configure TWDT properly
    taskYIELD();
}

/*
 * Print system info
 */
void eh_esp32_print_info(void) {
    ESP_LOGI(TAG, "ESP32 System Information:");
    ESP_LOGI(TAG, "  IDF Version : %s", esp_get_idf_version());
    ESP_LOGI(TAG, "  CPU Freq    : %d MHz", eh_esp32_cpu_freq_mhz());
    ESP_LOGI(TAG, "  Free Heap   : %zu bytes", eh_esp32_free_heap());
    ESP_LOGI(TAG, "  Free PSRAM  : %zu bytes", eh_esp32_free_psram());
    ESP_LOGI(TAG, "  Min Free    : %zu bytes", eh_esp32_min_free_heap());
}
