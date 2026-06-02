/*
 * EventHorizon Engine - ESP32 Main Entry Point
 * 
 * Demonstrates EventHorizon running on ESP32 microcontroller
 * 
 * Hardware Requirements:
 *   - ESP32 Dev Board (any variant)
 *   - Optional: PSRAM for larger models (2MB+)
 * 
 * Performance Expectations:
 *   - ESP32 @ 240 MHz: ~10-50K inferences/sec
 *   - ESP32-S3 @ 240 MHz: ~20-80K inferences/sec
 *   - ESP32-C3 @ 160 MHz: ~5-20K inferences/sec
 * 
 * Memory Configuration:
 *   - Without PSRAM: 512KB arena (limited)
 *   - With PSRAM: 2MB arena (comfortable)
 */

#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>

#include "eh_arena.h"
#include "eh_dag.h"
#include "eh_engine.h"
#include "eh_scoring.h"

static const char *TAG = "EventHorizon";

// ESP32 configuration
#ifndef EH_ARENA_SIZE
#define EH_ARENA_SIZE (512 * 1024)  // 512KB default
#endif

#ifndef EH_MAX_NODES
#define EH_MAX_NODES 128
#endif

// Demo task
void eventhorizon_demo_task(void *pvParameters) {
    ESP_LOGI(TAG, "╔══════════════════════════════════════╗");
    ESP_LOGI(TAG, "║   EventHorizon Engine on ESP32       ║");
    ESP_LOGI(TAG, "║   Edge AI Inference Demo             ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════╝");
    
    // Print system info
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "System Information:");
    ESP_LOGI(TAG, "  Chip: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "  CPU Freq: %d MHz", esp_clk_cpu_freq() / 1000000);
    ESP_LOGI(TAG, "  Free Heap: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "  Free PSRAM: %d bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG, "  Arena Size: %d KB", EH_ARENA_SIZE / 1024);
    
    // Step 1: Create arena
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[1/5] Creating memory arena...");
    
    // Try PSRAM first, fall back to internal RAM
    EH_Arena *arena = NULL;
    void *arena_mem = heap_caps_malloc(EH_ARENA_SIZE, MALLOC_CAP_SPIRAM);
    if (arena_mem) {
        ESP_LOGI(TAG, "  ✓ Using PSRAM for arena");
    } else {
        arena_mem = heap_caps_malloc(EH_ARENA_SIZE, MALLOC_CAP_8BIT);
        if (arena_mem) {
            ESP_LOGI(TAG, "  ✓ Using internal RAM for arena");
        } else {
            ESP_LOGE(TAG, "  ✗ Failed to allocate arena!");
            vTaskDelete(NULL);
            return;
        }
    }
    
    arena = eh_arena_create(EH_ARENA_SIZE);
    if (!arena) {
        ESP_LOGE(TAG, "  ✗ Failed to create arena!");
        free(arena_mem);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "  ✓ Arena: %d KB allocated", EH_ARENA_SIZE / 1024);
    
    // Step 2: Build small DAG (IoT sensor decision)
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[2/5] Building decision graph...");
    ESP_LOGI(TAG, "  Scenario: IoT sensor data classification");
    ESP_LOGI(TAG, "  Graph: IDLE → MONITOR → ALERT");
    
    EH_DAGNode *idle    = eh_arena_alloc_node(arena, 0, 16, 16);  // Small: 16x16
    EH_DAGNode *monitor = eh_arena_alloc_node(arena, 1, 16, 16);
    EH_DAGNode *alert   = eh_arena_alloc_node(arena, 2, 16, 16);
    
    if (!idle || !monitor || !alert) {
        ESP_LOGE(TAG, "  ✗ Failed to allocate nodes!");
        eh_arena_destroy(arena);
        free(arena_mem);
        vTaskDelete(NULL);
        return;
    }
    
    eh_dag_connect_nodes(idle, monitor);
    eh_dag_connect_nodes(monitor, alert);
    
    ESP_LOGI(TAG, "  ✓ Graph: 3 nodes, 2 edges");
    
    // Step 3: Initialize scoring
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[3/5] Initializing scoring core...");
    EH_ScoringCore *scorer = eh_scoring_init(16);
    if (!scorer) {
        ESP_LOGE(TAG, "  ✗ Failed to initialize scorer!");
        eh_arena_destroy(arena);
        free(arena_mem);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "  ✓ Scorer: 16-dimensional context");
    
    // Step 4: Setup engine (aggressive collapse for ESP32)
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[4/5] Setting up inference engine...");
    EH_Context *ctx = eh_engine_setup(idle, scorer, 1.0f);  // Aggressive collapse
    if (!ctx) {
        ESP_LOGE(TAG, "  ✗ Failed to setup engine!");
        eh_arena_destroy(arena);
        free(arena_mem);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "  ✓ Engine ready (collapse threshold: 1.0)");
    
    // Step 5: Run benchmark
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[5/5] Running inference benchmark...");
    
    float input[16] = {0};
    float output[16] = {0};
    
    // Simulate sensor readings
    input[0] = 0.5f;  // Temperature
    input[1] = 0.3f;  // Humidity
    input[2] = 0.8f;  // Motion detected
    input[3] = 0.1f;  // Light level
    
    const int num_inferences = 1000;
    int64_t start = esp_timer_get_time();
    
    for (int i = 0; i < num_inferences; i++) {
        eh_engine_inference(ctx, input, 16, output, 16);
        
        // Vary input slightly to simulate real sensor data
        input[0] += 0.01f * (i % 10);
        input[2] = (i % 100 < 20) ? 1.0f : 0.0f;  // Motion spikes
    }
    
    int64_t end = esp_timer_get_time();
    float elapsed_ms = (end - start) / 1000.0f;
    float per_inference_us = (end - start) / (float)num_inferences;
    float inferences_per_sec = 1000000.0f / per_inference_us;
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔══════════════════════════════════════╗");
    ESP_LOGI(TAG, "║      Benchmark Results               ║");
    ESP_LOGI(TAG, "╠══════════════════════════════════════╣");
    ESP_LOGI(TAG, "║  Total Time    : %.2f ms         ", elapsed_ms);
    ESP_LOGI(TAG, "║  Per Inference : %.2f μs         ", per_inference_us);
    ESP_LOGI(TAG, "║  Throughput    : %.0f infs/sec  ", inferences_per_sec);
    ESP_LOGI(TAG, "╚══════════════════════════════════════╝");
    
    // Get engine stats
    EH_EngineStats stats = eh_engine_get_stats(ctx);
    float collapse_ratio = 100.0f * stats.collapsed_nodes_evaluated / 
                          (float)stats.total_nodes_evaluated;
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Engine Statistics:");
    ESP_LOGI(TAG, "  Total inferences  : %llu", stats.total_inferences);
    ESP_LOGI(TAG, "  Nodes evaluated   : %llu", stats.total_nodes_evaluated);
    ESP_LOGI(TAG, "  Collapsed nodes   : %llu", stats.collapsed_nodes_evaluated);
    ESP_LOGI(TAG, "  Collapse ratio    : %.2f%%", collapse_ratio);
    
    // Memory stats
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Memory Statistics:");
    ESP_LOGI(TAG, "  Arena capacity : %zu bytes", arena->capacity);
    ESP_LOGI(TAG, "  Arena used     : %zu bytes (%.1f%%)", 
             arena->used, 100.0f * arena->used / arena->capacity);
    ESP_LOGI(TAG, "  Free heap      : %d bytes", esp_get_free_heap_size());
    
    // Cleanup
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "[Cleanup] Shutting down...");
    eh_engine_shutdown(ctx);
    eh_arena_destroy(arena);
    free(arena_mem);
    
    ESP_LOGI(TAG, "✓ Demo complete!");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "EventHorizon Engine is ready for ESP32 deployment.");
    ESP_LOGI(TAG, "Modify this code for your IoT/Edge AI use case.");
    
    // Keep task alive
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));  // 10 seconds
    }
}

void app_main(void) {
    // Create demo task with sufficient stack
    xTaskCreate(
        eventhorizon_demo_task,
        "eventhorizon_demo",
        8192,  // 8KB stack
        NULL,
        5,     // Priority
        NULL
    );
}
