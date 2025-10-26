# UVC 摄像头LCD显示修改说明

## 概述
将UVC摄像头的视频流从WiFi网页显示改为在LCD屏幕上直接显示，使用ESP32-P4的硬件JPEG解码器进行解码。
同时禁用了WiFi和HTTPS功能以节省资源。

## 修改内容

### 1. `main/app_uvc.c` - 主要修改

#### 1.1 添加了全局变量和外部引用
```c
// 外部引用来自 app_lcd.c 的变量
extern SemaphoreHandle_t lvgl_mux;
extern esp_lcd_panel_handle_t panel_handle;

// LVGL canvas 用于显示摄像头画面
static lv_obj_t *camera_canvas = NULL;
static lv_img_dsc_t camera_img_dsc;
static uint16_t *camera_frame_buffer = NULL;
```

#### 1.2 修改分辨率选择
- 自动搜索并选择 **320x240** 分辨率（接近用户要求的240x240）
- 原来默认使用 index 0，现在会智能查找合适的分辨率

#### 1.3 新增函数

**`init_camera_canvas(int width, int height)`**
- 初始化LVGL canvas用于显示摄像头画面
- 在PSRAM中分配RGB565格式的帧缓冲区（320x240x2 = 153600字节）
- 创建LVGL image对象并居中显示

**`decode_and_display_frame(const uint8_t *jpg_data, size_t jpg_size)`**
- 使用ESP32-P4硬件JPEG解码器解码MJPEG帧
- 将解码后的RGB565数据直接写入帧缓冲区
- 通知LVGL更新显示

**`frame_display_task(void *arg)`**
- 专门的任务用于处理帧显示
- 从队列接收UVC帧
- 调用解码和显示函数
- 将帧返回给UVC驱动

#### 1.4 修改流程
在 `uvc_task` 中：
1. 选择320x240分辨率
2. 初始化camera canvas
3. 创建frame_display_task任务
4. 开始UVC流

### 2. `main/app_lcd.c` - 次要修改

#### 2.1 禁用Demo界面
- 注释掉 `lv_demo_widgets()` 调用
- 设置黑色背景作为摄像头画面的底色
- 保留LVGL的初始化和触摸功能

### 3. `main/app_main.c` - 禁用网页显示

#### 3.1 注释掉WiFi和HTTPS
- 注释掉 `app_wifi_init()` 和 `app_https_init()`
- 注释掉相关头文件引用
- 节省内存和CPU资源用于视频处理

## 技术细节

### 分辨率说明
从UVC设备描述符中，选择的分辨率：
- **Frame Index 10**: 320x240 @ 30fps
- 格式：MJPEG
- 帧缓冲区大小：约 150KB (153600字节)

### 内存分配
- 使用PSRAM分配帧缓冲区（需要启用CONFIG_SPIRAM）
- RGB565格式：每像素2字节
- 320x240分辨率：153600字节

### JPEG解码
- 使用ESP32-P4硬件JPEG解码器（driver/jpeg_decode.h）
- 输入：MJPEG压缩数据
- 输出：RGB565格式（直接兼容LVGL显示）

### 任务优先级
- `frame_display_task`: 优先级4，栈大小8KB
- 处理30fps视频流

### 线程安全
- 使用`lvgl_mux`互斥锁保护LVGL API调用
- 队列大小：2帧（平衡延迟和内存使用）

## 编译配置要求

确保以下配置已启用：

1. **PSRAM支持**（必需）
   ```
   CONFIG_SPIRAM=y
   ```

2. **USB Host控制传输大小**（至少512字节）
   ```
   CONFIG_USB_HOST_CONTROL_TRANSFER_MAX_SIZE=1024
   ```

3. **Flash大小**（至少16MB）
   ```
   CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
   ```

4. **BLE配置**（如果使用ESP32协处理器）
   ```
   CONFIG_BT_NIMBLE_50_FEATURE_SUPPORT=n
   ```

5. **分区表**
   ```
   CONFIG_PARTITION_TABLE_CUSTOM=y
   CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
   CONFIG_PARTITION_TABLE_OFFSET=0x8000
   ```

## 性能特点

- **帧率**: 目标30fps（取决于JPEG解码速度）
- **延迟**: 约2-3帧（66-100ms @ 30fps）
- **内存使用**: 
  - 帧缓冲区: 150KB (PSRAM)
  - UVC缓冲区: 3 x 150KB = 450KB (PSRAM)
  - 总计: 约600KB PSRAM

## 下一步优化建议

1. **调整分辨率**: 如需更小分辨率，可选择160x120（Frame Index 11）
2. **帧率控制**: 可以在frame_display_task中添加延迟来降低CPU使用率
3. **图像处理**: 可在解码后添加缩放、旋转等处理
4. **UI叠加**: 可在摄像头画面上叠加LVGL控件（按钮、标签等）

## 测试步骤

1. 连接UVC摄像头到ESP32-P4的USB接口
2. 编译并烧录固件
3. 观察串口输出，应看到：
   - "Found 320x240 resolution at index X"
   - "Camera canvas initialized: 320x240"
   - "Frame display task started"
4. LCD屏幕应显示摄像头实时画面

## 故障排查

**问题**: LCD显示黑屏
- 检查摄像头是否正常连接
- 查看串口是否有"JPEG decode failed"错误
- 确认PSRAM已启用并正常工作

**问题**: 帧率低或卡顿
- 检查CPU使用率
- 考虑降低分辨率
- 增加frame_display_task的优先级

**问题**: 内存不足
- 确保PSRAM已启用
- 检查PSRAM容量（需要至少8MB）
- 减少NUMBER_OF_FRAME_BUFFERS的值


