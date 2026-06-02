/*
 * EventHorizon Engine - ESP32 Platform Port Header
 */

#ifndef ESP32_PORT_H
#define ESP32_PORT_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Memory allocation */
void *eh_esp32_malloc(size_t size);
void *eh_esp32_calloc(size_t nmemb, size_t size);
void eh_esp32_free(void *ptr);

/* Timing */
uint64_t eh_esp32_micros(void);
uint64_t eh_esp32_millis(void);
void eh_esp32_delay_ms(uint32_t ms);
void eh_esp32_delay_us(uint32_t us);

/* Memory statistics */
size_t eh_esp32_free_heap(void);
size_t eh_esp32_free_psram(void);
size_t eh_esp32_min_free_heap(void);

/* System info */
uint32_t eh_esp32_cpu_freq_mhz(void);
void eh_esp32_print_info(void);

/* Watchdog */
void eh_esp32_feed_watchdog(void);

/* Logging */
typedef enum {
    EH_LOG_ERROR,
    EH_LOG_WARN,
    EH_LOG_INFO,
    EH_LOG_DEBUG
} eh_log_level_t;

void eh_esp32_log(eh_log_level_t level, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* ESP32_PORT_H */
