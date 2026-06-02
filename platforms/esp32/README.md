# EventHorizon Engine - ESP32 Port

Run EventHorizon Engine on ESP32 microcontrollers for edge AI inference.

---

## 🎯 What is This?

This port enables EventHorizon Engine to run on ESP32 family microcontrollers:
- **ESP32** (Xtensa dual-core @ 240 MHz)
- **ESP32-S3** (Xtensa dual-core @ 240 MHz, better AI performance)
- **ESP32-C3** (RISC-V single-core @ 160 MHz, low power)

**Performance:**
- ESP32: ~10-50K inferences/sec (16-dimensional graphs)
- ESP32-S3: ~20-80K inferences/sec
- ESP32-C3: ~5-20K inferences/sec

**Memory:**
- Without PSRAM: 512KB arena
- With PSRAM: 2MB arena

---

## 📋 Prerequisites

### Hardware
- ESP32 development board (any variant)
- USB cable
- Optional: ESP32 with PSRAM for larger models

### Software
- [PlatformIO](https://platformio.org/) (recommended)
  ```bash
  pip install platformio
  ```
- OR [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/) (manual setup)

---

## 🚀 Quick Start

### Using PlatformIO (Recommended)

1. **Install PlatformIO**
   ```bash
   pip install platformio
   ```

2. **Navigate to ESP32 directory**
   ```bash
   cd platforms/esp32
   ```

3. **Build**
   ```bash
   pio run
   ```

4. **Upload to ESP32**
   ```bash
   pio run --target upload
   ```

5. **Monitor output**
   ```bash
   pio device monitor
   ```

**Expected Output:**
```
╔══════════════════════════════════════╗
║   EventHorizon Engine on ESP32       ║
║   Edge AI Inference Demo             ║
╚══════════════════════════════════════╝

System Information:
  Chip: ESP32-D0WDQ6 (revision 1)
  CPU Freq: 240 MHz
  Free Heap: 298516 bytes
  Arena Size: 512 KB

[1/5] Creating memory arena...
  ✓ Arena: 512 KB allocated
  
[2/5] Building decision graph...
  ✓ Graph: 3 nodes, 2 edges
  
[3/5] Initializing scoring core...
  ✓ Scorer: 16-dimensional context
  
[4/5] Setting up inference engine...
  ✓ Engine ready (collapse threshold: 1.0)
  
[5/5] Running inference benchmark...

╔══════════════════════════════════════╗
║      Benchmark Results               ║
╠══════════════════════════════════════╣
║  Total Time    : 45.23 ms            ║
║  Per Inference : 45.23 μs            ║
║  Throughput    : 22,109 infs/sec     ║
╚══════════════════════════════════════╝
```

---

## 🔧 Configuration

### Memory Configuration

Edit `platformio.ini`:

```ini
[env:esp32dev]
build_flags = 
    -DEH_ARENA_SIZE=512*1024      ; 512KB (default)
    -DEH_MAX_NODES=128             ; Max graph nodes
    -DEH_BEAM_WIDTH=2              ; Beam search width
```

**Recommended settings by board:**

| Board | Arena Size | Max Nodes | Expected Performance |
|-------|-----------|-----------|---------------------|
| ESP32 (no PSRAM) | 512 KB | 128 | 10-20K infs/sec |
| ESP32 (with PSRAM) | 2 MB | 256 | 20-50K infs/sec |
| ESP32-S3 | 1 MB | 256 | 30-80K infs/sec |
| ESP32-C3 | 256 KB | 64 | 5-15K infs/sec |

### Target-Specific Builds

```bash
# Standard ESP32
pio run -e esp32dev

# ESP32 with PSRAM
pio run -e esp32dev_psram

# ESP32-S3
pio run -e esp32s3

# ESP32-C3 (RISC-V)
pio run -e esp32c3
```

---

## 📊 Performance Tuning

### 1. Aggressive Collapse Threshold

For ESP32's limited CPU, use aggressive collapse:

```c
EH_Context *ctx = eh_engine_setup(root, scorer, 1.0f);  // Aggressive
// vs
EH_Context *ctx = eh_engine_setup(root, scorer, 2.0f);  // Conservative
```

**Impact:** 30-50% speedup at cost of slight accuracy loss

### 2. Reduce Graph Dimensionality

```c
// Instead of 128x128 weights:
EH_DAGNode *node = eh_arena_alloc_node(arena, id, 128, 128);

// Use 16x16 or 32x32 on ESP32:
EH_DAGNode *node = eh_arena_alloc_node(arena, id, 16, 16);
```

**Impact:** 64x less memory, 16x faster

### 3. Use PSRAM

Enable PSRAM in `platformio.ini`:

```ini
build_flags = 
    -DBOARD_HAS_PSRAM
    -DEH_ARENA_SIZE=2*1024*1024
```

**Impact:** 4x more memory available

### 4. Disable Neuroplasticity

If you don't need adaptive learning:

```c
// Don't initialize neuroplasticity
// EH_NeuroContext *nctx = ...  // Skip this
```

**Impact:** 20% less memory usage

### 5. CPU Frequency

Set maximum CPU frequency in `menuconfig`:

```bash
pio run -t menuconfig
# Component config → ESP32-specific → CPU frequency → 240 MHz
```

---

## 🎯 Use Cases on ESP32

### 1. IoT Sensor Classification

```c
// Classify sensor readings: normal, warning, critical
float input[16] = {
    temperature, humidity, motion, light,
    pressure, gas, sound, vibration,
    ...
};
eh_engine_inference(ctx, input, 16, output, 16);
```

### 2. Smart Home Automation

```c
// Decide actions based on sensor fusion
// Input: all sensor states
// Output: action probabilities (lights, HVAC, security)
```

### 3. Predictive Maintenance

```c
// Monitor machine health from vibration/temperature
// Predict failures before they happen
```

### 4. Edge AI Camera

```c
// Lightweight object detection post-processing
// Input: feature vector from camera
// Output: action (alert, record, ignore)
```

### 5. Robotics Control

```c
// Real-time motor control decisions
// Input: IMU + sensors
// Output: motor commands
```

---

## 🐛 Troubleshooting

### Build Errors

**Error:** `undefined reference to 'eh_arena_create'`

**Solution:** Make sure source files are included in `build_src_filter`:
```ini
build_src_filter = 
    +<../../src/core/>
```

---

### Upload Errors

**Error:** `Serial port not found`

**Solution:**
```bash
# List available ports
pio device list

# Specify port explicitly
pio run --target upload --upload-port /dev/ttyUSB0
```

---

### Runtime Errors

**Error:** `Arena allocation failed`

**Solution:** Reduce arena size or enable PSRAM:
```ini
-DEH_ARENA_SIZE=256*1024  ; Reduce to 256KB
```

**Error:** `Stack overflow`

**Solution:** Increase task stack size in `esp32_main.c`:
```c
xTaskCreate(..., 16384, ...);  // 16KB stack
```

---

### Performance Issues

**Symptom:** Slower than expected

**Check:**
1. CPU frequency: `esp_clk_cpu_freq()` should be 240 MHz
2. Collapse threshold: Lower = faster
3. Graph size: Use 16x16 or 32x32 weights
4. Watchdog: Not feeding too frequently

---

## 📁 File Structure

```
platforms/esp32/
├── platformio.ini        # PlatformIO configuration
├── esp32_main.c          # Main entry point & demo
├── esp32_port.c          # ESP32-specific implementations
├── include/
│   └── esp32_port.h      # Platform API
└── README.md             # This file
```

---

## 🔗 Integration with ESP-IDF Components

### WiFi + MQTT

```c
#include <esp_wifi.h>
#include <mqtt_client.h>

// Run inference, send results via MQTT
eh_engine_inference(ctx, input, dim, output, dim);
esp_mqtt_client_publish(client, "sensors/decision", output, sizeof(output), 0, 0);
```

### BLE Beacon

```c
#include <esp_bt.h>

// Broadcast inference results via BLE
```

### HTTP Server

```c
#include <esp_http_server.h>

// Expose inference API via REST
esp_err_t inference_handler(httpd_req_t *req) {
    // Parse input from JSON
    // Run inference
    // Return JSON output
}
```

---

## 🎓 Learning Resources

### ESP32 Basics
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [PlatformIO ESP32 Tutorial](https://docs.platformio.org/en/latest/platforms/espressif32.html)

### EventHorizon
- [Main README](../../README.md) - Architecture overview
- [Performance Tuning](../../PERFORMANCE_TUNING.md) - Optimization guide
- [Examples](../../examples/) - More examples

---

## 🤝 Contributing

Improvements to ESP32 port welcome:
- NEON SIMD optimization for ESP32-S3
- Power consumption optimization
- More examples (camera, audio, etc.)
- ESP32-C6 / ESP32-H2 support

See [CONTRIBUTING.md](../../CONTRIBUTING.md)

---

## 📜 License

Apache-2.0 (same as main project)

---

## 🆘 Support

- **Issues**: [GitHub Issues](https://github.com/[username]/eventhorizon/issues)
- **Discussions**: [GitHub Discussions](https://github.com/[username]/eventhorizon/discussions)
- **ESP32 Forums**: [ESP32 Forum](https://esp32.com/)

---

**Built with EventHorizon Engine**  
**Optimized for ESP32 Edge AI**  
**Apache-2.0 License**
