# RoastSensePRO - 功能说明

## 已实现功能

### 1. UVC摄像头LCD实时显示 ✅
- **分辨率**: 320x240 @ 30fps
- **编码**: MJPEG硬件解码（ESP_VIDEO_CODEC）
- **显示**: LCD屏幕实时显示
- **优化**: 双缓冲防撕裂、字节序自动转换

### 2. AVI视频录制 ✅
- **格式**: AVI + MJPEG
- **存储**: SD卡 (SDMMC 4-bit模式)
- **控制**: 触摸按钮一键录制
- **状态**: 实时显示录制状态
- **文件名**: 自动时间戳 `video_YYYYMMDD_HHMMSS.avi`

### 3. 其他功能
- WiFi / HTTPS (保留用于配置和控制)
- BLE (蓝牙低功耗)
- LCD触摸屏 (410x502, CO5300驱动)

## 硬件要求

### 主控芯片
- **型号**: ESP32-P4
- **PSRAM**: 必需，≥8MB
- **Flash**: ≥16MB

### 外设
- **UVC摄像头**: 支持MJPEG格式
- **LCD屏幕**: 410x502, SPI接口
- **触摸屏**: CST9217
- **SD卡**: Class 10+, FAT32, ≥4GB

### SD卡接线
```
CMD  = GPIO_44
CLK  = GPIO_43
D0   = GPIO_39
D1   = GPIO_40
D2   = GPIO_41
D3   = GPIO_42
```

## 用户界面

### 录制按钮 (左上角)
- **红色 "REC"**: 点击开始录制
- **绿色 "STOP"**: 点击停止录制

### 信息按钮 (右上角)
- **蓝色 "INFO"**: 预留功能

### 状态显示 (底部中央)
- **"Ready"**: 就绪状态
- **"Recording..."**: 正在录制
- **"Saved: N frames"**: 录制完成

## 快速开始

### 1. 编译
```bash
idf.py build
```

### 2. 烧录
```bash
idf.py flash monitor
```

### 3. 使用
1. 插入SD卡
2. 连接UVC摄像头
3. LCD显示实时画面
4. 点击"REC"开始录像
5. 点击"STOP"停止录像

### 4. 播放录像
- 取出SD卡，插入电脑
- 使用VLC等播放器打开AVI文件

## 性能指标

| 参数 | 数值 |
|------|------|
| 视频分辨率 | 320x240 |
| 帧率 | 30 fps |
| 编码格式 | MJPEG |
| 平均码率 | 300-600 KB/s |
| 1分钟视频 | ~18-36 MB |
| 内存占用 | ~750KB PSRAM |

## 文件结构

### 核心文件
- `main/app_uvc.c` - UVC摄像头控制和显示
- `main/app_lcd.c` - LCD和UI控制
- `main/app_sd_card.c` - SD卡驱动
- `main/app_avi_recorder.c` - AVI录制模块
- `main/app_main.c` - 主程序入口

### 配置文件
- `partitions.csv` - 自定义分区表
- `sdkconfig` - ESP-IDF配置

## 关键配置

### 必需配置
```
CONFIG_SPIRAM=y                        # PSRAM支持
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y     # 16MB Flash
CONFIG_LV_COLOR_16_SWAP=y              # LVGL颜色字节序
CONFIG_PARTITION_TABLE_CUSTOM=y        # 自定义分区表
CONFIG_USB_HOST_CONTROL_TRANSFER_MAX_SIZE=1024  # USB控制传输
```

### SD卡配置
```
CONFIG_FATFS_LFN_HEAP=y                # 长文件名
CONFIG_FATFS_MAX_LFN=255               # 最大文件名
```

## 技术亮点

1. **ESP_VIDEO_CODEC** - 使用ESP官方视频编解码库，硬件加速
2. **双缓冲机制** - 消除画面撕裂和闪烁
3. **MJPEG直通** - 录制时直接保存压缩帧，无需重新编码
4. **异步处理** - 显示和录制并行，互不影响
5. **标准AVI格式** - 兼容所有主流播放器

## 疑难问题解决记录

### 已解决的问题
1. ✅ 依赖路径错误 - 删除dependencies.lock重新生成
2. ✅ Flash容量不足 - 配置16MB
3. ✅ BLE 5.0冲突 - 禁用BLE 5.0功能
4. ✅ `compare`函数类型错误 - 改为`int`返回值
5. ✅ USB枚举失败 - 增加控制传输大小到1024
6. ✅ 内存不足 - 启用PSRAM
7. ✅ 缓冲区对齐错误 - 使用`esp_video_codec_align_alloc`
8. ✅ 颜色字节序错误 - 启用`CONFIG_LV_COLOR_16_SWAP`
9. ✅ 画面倒置 - LCD驱动Y轴镜像
10. ✅ 画面闪烁花纹 - 双缓冲+锁机制

## 维护建议

### 定期检查
- SD卡剩余空间
- 录像文件完整性
- PSRAM使用情况

### 日志监控
- UVC连接状态
- 解码成功率
- SD卡写入错误

### 更新计划
- 添加视频回放功能
- 实现WiFi视频传输
- 优化录制码率
- 添加音频支持



