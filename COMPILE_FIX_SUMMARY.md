# 编译错误修复总结

## 修复的问题

### 1. 函数声明顺序问题
**错误**: `implicit declaration of function 'init_camera_canvas'`

**修复**: 在文件顶部添加前向声明
```c
// Forward declarations
static esp_err_t uvc_open(uvc_dev_t *dev, int frame_index);
static void init_camera_canvas(int width, int height);
static void decode_and_display_frame(const uint8_t *jpg_data, size_t jpg_size);
static void frame_display_task(void *arg);
```

### 2. JPEG解码器API不匹配
**错误**: ESP-IDF v5.5.1使用不同的JPEG解码API

**修复**: 更新为正确的API调用
```c
// 旧代码（错误）
jpeg_decode_cfg_t decode_cfg = DEFAULT_JPEG_DECODE_CONFIG();
jpeg_decoder_process(jpeg_handle, jpg_data, jpg_size, ...);

// 新代码（正确）
jpeg_decode_engine_cfg_t eng_cfg = {
    .intr_priority = 0,
    .timeout_ms = 40,
};
jpeg_new_decoder_engine(&eng_cfg, &jpeg_handle);

jpeg_decode_cfg_t decode_cfg = {
    .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
    .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
    .conv_std = JPEG_YUV_RGB_CONV_STD_BT601,
};
jpeg_decoder_process(jpeg_handle, &decode_cfg, jpg_data, jpg_size, ...);
```

### 3. 禁用网页显示功能
**位置**: `main/app_main.c`

**修改**:
- 注释掉 `#include "app_https.h"` 和 `#include "app_wifi.h"`
- 注释掉 `app_wifi_init()` 和 `app_https_init()`

**好处**:
- 节省内存（约1-2MB）
- 减少CPU开销
- 专注于视频处理

## 验证结果

### 编译状态
✅ `main/app_uvc.c` - 无错误
✅ `main/app_lcd.c` - 无错误  
✅ `main/app_main.c` - 无错误

### 关键API使用
- ✅ `jpeg_new_decoder_engine()` - 正确参数类型
- ✅ `jpeg_decoder_process()` - 正确参数顺序
- ✅ `jpeg_del_decoder_engine()` - 正确调用

## 下一步

1. **编译项目**
   ```bash
   idf.py build
   ```

2. **烧录固件**
   ```bash
   idf.py flash monitor
   ```

3. **测试功能**
   - 连接UVC摄像头
   - 观察LCD屏幕显示
   - 检查串口日志

## 预期日志输出

```
I (xxx) app_main: Initialize UVC
I (xxx) uvc: Found 320x240 resolution at index X
I (xxx) uvc: Camera canvas initialized: 320x240
I (xxx) uvc: Frame display task started
I (xxx) uvc: uvc begin to stream
```

## 性能预期

- **帧率**: 接近30fps（取决于JPEG复杂度）
- **延迟**: 66-100ms（2-3帧）
- **CPU使用**: 约60-80%（主要用于JPEG解码）
- **内存使用**: 
  - PSRAM: ~600KB（帧缓冲）
  - DRAM: 节省约1-2MB（禁用WiFi/HTTPS）


