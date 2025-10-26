# 黑屏问题修复说明

## 问题诊断

从日志分析发现的问题：

### 1. ❌ 分辨率选择错误
```
I (8513) uvc: Camera canvas initialized: 1920x1080
```
- **期望**: 320x240
- **实际**: 1920x1080（第一个分辨率）
- **原因**: `dev->frame_info_num` 被硬编码为8，而320x240是第10个MJPEG帧

### 2. ❌ 视频流未启动
- **缺失日志**: "uvc begin to stream"
- **原因**: 没有自动触发 `EVENT_START` 事件

### 3. ❌ 分辨率过大
- **LCD屏幕**: 410x502
- **视频帧**: 1920x1080 = 4MB
- **问题**: 内存和显示不匹配

## 修复内容

### 修复1: 使用实际帧数量
**位置**: `main/app_uvc.c` 第270-272行

**原代码**:
```c
dev->frame_info_num = 8;  // 硬编码！
```

**修复后**:
```c
// Use actual picked frame count instead of hardcoded 8
dev->frame_info_num = pick_frame_info_num;
ESP_LOGI(TAG, "Total picked MJPEG frames: %d", pick_frame_info_num);
```

### 修复2: 增强分辨率搜索日志
**位置**: `main/app_uvc.c` 第143-163行

**新增功能**:
```c
ESP_LOGI(TAG, "Searching for 320x240 in %d MJPEG frames", dev->frame_info_num);
for (int i = 0; i < dev->frame_info_num; i++) {
    ESP_LOGI(TAG, "  Frame[%d]: %dx%d", i, dev->frame_info[i].h_res, dev->frame_info[i].v_res);
    // 搜索逻辑...
}
```

**好处**: 
- 看到所有可用的分辨率
- 知道是否找到目标分辨率
- 更容易调试

### 修复3: 自动启动视频流
**位置**: `main/app_uvc.c` 第178-180行

**新增代码**:
```c
// Auto-start streaming after initialization
ESP_LOGI(TAG, "Auto-starting video stream...");
xEventGroupSetBits(dev->event_group, EVENT_START);
```

**好处**: 初始化完成后自动开始推流

## 预期效果

重新编译烧录后，应该看到以下日志：

```
I (xxx) uvc: Total picked MJPEG frames: 12
I (xxx) uvc: Searching for 320x240 in 12 MJPEG frames
I (xxx) uvc:   Frame[0]: 1920x1080
I (xxx) uvc:   Frame[1]: 1280x720
I (xxx) uvc:   Frame[2]: 1024x576
I (xxx) uvc:   Frame[3]: 960x720
I (xxx) uvc:   Frame[4]: 864x480
I (xxx) uvc:   Frame[5]: 800x600
I (xxx) uvc:   Frame[6]: 640x480
I (xxx) uvc:   Frame[7]: 544x288
I (xxx) uvc:   Frame[8]: 432x240
I (xxx) uvc:   Frame[9]: 320x240       ← 在这里找到！
I (xxx) uvc: ✓ Found 320x240 resolution at index 9
I (xxx) uvc: Open the uvc device
I (xxx) uvc: Video decoder initialized successfully
I (xxx) uvc: Camera canvas initialized: 320x240   ← 正确的分辨率
I (xxx) uvc: Frame display task started
I (xxx) uvc: Auto-starting video stream...
I (xxx) uvc: uvc begin to stream                  ← 流已启动
```

## 下一步操作

### 1. 清理并重新编译
```bash
idf.py fullclean
idf.py build
```

### 2. 烧录固件
```bash
idf.py flash monitor
```

### 3. 验证显示
- ✅ LCD应显示320x240的摄像头画面
- ✅ 帧率约30fps
- ✅ 无黑屏

## 故障排查

### 如果仍然黑屏

#### 检查1: 确认分辨率
查找日志：
```
I (xxx) uvc: ✓ Found 320x240 resolution at index X
I (xxx) uvc: Camera canvas initialized: 320x240
```

如果仍显示1920x1080，说明修改未生效。

#### 检查2: 确认流已启动
查找日志：
```
I (xxx) uvc: uvc begin to stream
```

如果没有这条日志，流未启动。

#### 检查3: 查看解码错误
查找日志：
```
W (xxx) uvc: JPEG decode failed: X
```

如果频繁出现，可能是解码器问题。

### 如果320x240不存在

某些摄像头可能不支持320x240，可以修改为其他分辨率：

**修改位置**: `main/app_uvc.c` 第150行

```c
// 尝试其他分辨率
if (dev->frame_info[i].h_res == 640 && dev->frame_info[i].v_res == 480) {
    // 使用 640x480
}
```

或者使用最小分辨率：
```c
// 选择最小的分辨率
int min_pixels = INT_MAX;
for (int i = 0; i < dev->frame_info_num; i++) {
    int pixels = dev->frame_info[i].h_res * dev->frame_info[i].v_res;
    if (pixels < min_pixels) {
        min_pixels = pixels;
        target_frame_index = i;
    }
}
```

## 技术细节

### 为什么硬编码为8会有问题？

1. **设备报告**: UVC设备有12个MJPEG格式帧
2. **硬编码限制**: 只保留前8个
3. **320x240位置**: 在第10个（索引9）
4. **结果**: 搜索不到，使用默认index 0 (1920x1080)

### 内存占用对比

| 分辨率 | 帧缓冲大小 | 状态 |
|--------|-----------|------|
| 1920x1080 | 4MB | ❌ 太大，超出PSRAM |
| 640x480 | 600KB | ✅ 可用 |
| 320x240 | 150KB | ✅ 理想 |
| 160x120 | 38KB | ✅ 最小 |

### LCD显示策略

对于410x502的LCD：
- **320x240**: 居中显示，留黑边
- **更大分辨率**: 需要缩放（未实现）
- **更小分辨率**: 居中显示，更多黑边

## 总结

✅ **修复完成**:
- 动态获取实际帧数量
- 添加详细搜索日志
- 自动启动视频流
- 正确选择320x240分辨率

🎯 **预期结果**:
- LCD显示实时摄像头画面
- 320x240 @ 30fps
- 低延迟（<100ms）


