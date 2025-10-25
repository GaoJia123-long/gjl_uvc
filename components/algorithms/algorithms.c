
#include "algorithms.h"


extern uint8_t flag_ESP_LOGI;

static const char *TAG = "algorithms";

//单个块的内存
float A_block[BLOCK_SIZE * BLOCK_SIZE]; 

// //db4小波的低通和高通滤波器系数
// const float db4_lowpass[5] = { 0.48296, 0.8365, 0.22414, -0.12940, -0.03645 };
// const float db4_highpass[5] = { -0.03645, -0.12940, 0.22414, 0.8365, 0.48296 };

/*****************************************************************************************************************************************/
/************************************************************排序************************************************************************/
/*****************************************************************************************************************************************/

// //排序数组 --- 数据多时效率太差，不用了
// void sortArray_int16(int16_t *array, int size) 
// {
//     for (int i = 0; i < size - 1; i++) 
//     {
//         for (int j = 0; j < size - i - 1; j++) 
//         {
//             if (array[j] > array[j + 1]) 
//             {
//                 int16_t temp = array[j];
//                 array[j] = array[j + 1];
//                 array[j + 1] = temp;
//             }
//         }
//     }
// }

// 快速排序的分区函数
int partition(int16_t *array, int low, int high) 
{
    int16_t pivot = array[high];  // 选取数组的最后一个元素作为pivot
    int i = (low - 1);  // 最小元素的索引

    // 遍历数组，将小于 pivot 的元素移到数组的左侧
    for (int j = low; j < high; j++) 
    {
        if (array[j] < pivot) 
        {
            i++;
            // 交换 array[i] 和 array[j]
            int16_t temp = array[i];
            array[i] = array[j];
            array[j] = temp;
        }
    }

    // 将 pivot 元素放置到正确的位置
    int16_t temp = array[i + 1];
    array[i + 1] = array[high];
    array[high] = temp;

    return (i + 1);
}

// 快速排序的递归函数
void quickSort(int16_t *array, int low, int high) 
{
    if (low < high) 
    {
        // 分区索引，数组被分成两部分
        int pi = partition(array, low, high);

        // 递归排序左右两部分
        quickSort(array, low, pi - 1);
        quickSort(array, pi + 1, high);
    }
}

// 快速排序，封装函数接口，调用时直接使用
void sortArray_int16(int16_t *array, int size) 
{
    quickSort(array, 0, size - 1);
}


// 排序数组
void sortArray_float(float *array, int size) 
{
    for (int i = 0; i < size - 1; i++) 
    {
        for (int j = 0; j < size - i - 1; j++) 
        {
            if (array[j] > array[j + 1]) 
            {
                float temp = array[j];
                array[j] = array[j + 1];
                array[j + 1] = temp;
            }
        }
    }
}

/*****************************************************************************************************************************************/
/************************************************************滑窗************************************************************************/
/*****************************************************************************************************************************************/
//初始化滑动窗口
void initSlidingWindow(SlidingWindow *window) 
{
    for (int i = 0; i < WINDOW_SIZE; i++) 
    {
        window->buffer[i] = 0;  //初始化缓冲区
    }
    window->head = 0;           //初始化写入位置
    window->count = 0;          //有效数据数量初始化为 0
}

//初始化滑动窗口
void initSlidingWindow_filter(SlidingWindow_filter *window) 
{
    for (int i = 0; i < WINDOW_SIZE_Filter; i++) 
    {
        window->buffer[i] = 0;  //初始化缓冲区
    }
    window->head = 0;           //初始化写入位置
    window->count = 0;          //有效数据数量初始化为 0
}

//初始化滑动窗口
void initSlidingWindow_filter2(SlidingWindow_filter2 *window) 
{
    for (int i = 0; i < WINDOW_SIZE_Filter2; i++) 
    {
        window->buffer[i] = 0;  //初始化缓冲区
    }
    window->head = 0;           //初始化写入位置
    window->count = 0;          //有效数据数量初始化为 0
}

//添加数据到滑动窗口
void addToSlidingWindow(SlidingWindow *window, float value) 
{
    //将新值写入环形缓冲区的当前位置
    window->buffer[window->head] = value;

    //更新环形缓冲区的写入位置
    window->head = (window->head + 1) % WINDOW_SIZE;

    //如果窗口未满，增加有效数据计数
    if (window->count < WINDOW_SIZE) 
    {
        window->count++;
    }
}

//添加数据到滑动窗口
void addToSlidingWindow4Filter(SlidingWindow_filter *window, float value) 
{
    //将新值写入环形缓冲区的当前位置
    window->buffer[window->head] = value;

    //更新环形缓冲区的写入位置
    window->head = (window->head + 1) % WINDOW_SIZE_Filter;

    //如果窗口未满，增加有效数据计数
    if (window->count < WINDOW_SIZE_Filter) 
    {
        window->count++;
    }
}

//添加数据到滑动窗口
void addToSlidingWindow4Filter2(SlidingWindow_filter2 *window, float value) 
{
    //将新值写入环形缓冲区的当前位置
    window->buffer[window->head] = value;

    //更新环形缓冲区的写入位置
    window->head = (window->head + 1) % WINDOW_SIZE_Filter2;

    //如果窗口未满，增加有效数据计数
    if (window->count < WINDOW_SIZE_Filter2) 
    {
        window->count++;
    }
}

/*****************************************************************************************************************************************/
/************************************************小波变换和中位均值加权滤波*****************************************************************/
/*****************************************************************************************************************************************/
// //进行小波分解的函数
// void wavedec(float* signal, float* d, float* a, int level, int length) 
// {
//     int i, j;
//     float temp_signal[length];       //临时信号存储

//     for (i = 0; i < length; i++) 
//     {
//         temp_signal[i] = signal[i];  //初始化临时信号
//     }

//     for (i = 0; i < level; i++) 
//     {
//         //在每一层进行滤波和下采样
//         for (j = 0; j < length / 2; j++) 
//         {
//             //对信号进行低通和高通滤波（这里使用db4系数）
//             d[j] = 0;
//             a[j] = 0;
//             for (int k = 0; k < 5; k++) 
//             {
//                 if (2 * j - k >= 0 && 2 * j - k < length) 
//                 {
//                     d[j] += temp_signal[2 * j - k] * db4_highpass[k];
//                     a[j] += temp_signal[2 * j - k] * db4_lowpass[k];
//                 }
//             }
//         }
//     }
// }

// //小波重构的函数
// void wrcoef(float* d, float* a, float* signal, int level, int length) 
// {
//     int i, j;
//     float temp_signal[length];       //临时信号存储

//     for (i = 0; i < length; i++) 
//     {
//         temp_signal[i] = signal[i];  //初始化临时信号
//     }

//     for (i = 0; i < level; i++) 
//     {
//         //重构信号
//         for (j = 0; j < length / 2; j++) 
//         {
//             signal[j] = 0;
//             for (int k = 0; k < 5; k++) 
//             {
//                 if (2 * j - k >= 0 && 2 * j - k < length) 
//                 {
//                     signal[j] += d[j] * db4_highpass[k] + a[j] * db4_lowpass[k];
//                 }
//             }
//         }
//     }
// }

//计算滑动窗口中位加权均值
float calculateWeightedMedianMean_window(SlidingWindow_filter *window)    ///no use
{
    if (window->count == 0) 
    {
        return 0.0f;  //如果窗口为空，返回0
    }

    // 1. 复制缓冲区到临时数组进行排序
    float tempBuffer[WINDOW_SIZE_Filter];
    for (int i = 0; i < window->count; i++) 
    {
        tempBuffer[i] = window->buffer[i];
    }
    sortArray_float(tempBuffer, window->count);

    // 2. 计算中位数
    float median;
    if (window->count % 2 == 0) 
    {
        median = (tempBuffer[window->count / 2 - 1] + tempBuffer[window->count / 2]) / 2.0f;
    } else 
    {
        median = tempBuffer[window->count / 2];
    }

    // 3. 加权计算
    float weightedSum = 0.0f;
    float weightSum = 0.0f;
    for (int i = 0; i < window->count; i++) 
    {
        float weight = 1.0f / (1.0f + abs((int)(tempBuffer[i] - median)));  //权重公式
        weightedSum += tempBuffer[i] * weight;
        weightSum += weight;
    }

    return weightedSum / weightSum;  //返回加权均值
}

//计算滑动窗口中位加权均值
float calculateWeightedMedianMean_float(float *buff,int length)     //
{
    // 1. 排序
    sortArray_float(buff, length);
    // 2. 计算中位数
    float median;
    if (length % 2 == 0) 
    {
        median = (buff[length / 2 - 1] + buff[length / 2]) / 2.0f;
    } 
    else 
    {
        median = buff[length / 2];
    }

    // 3. 加权计算
    float weightedSum = 0.0f;
    float weightSum = 0.0f;
    for (int i = 0; i < length; i++) 
    {
        float weight = 1.0f / (1.0f + abs((int)(buff[i] - median)));  //权重公式
        weightedSum += buff[i] * weight;
        weightSum += weight;
    }

    return weightedSum / weightSum;  //返回加权均值
}

//计算中位位加权均值
float calculateWeightedMedianMean_int16(int16_t *buff,int length) 
{
    //1.排序
    sortArray_int16(buff, length);

    //2.计算中位数
    float median;
    if (length % 2 == 0) 
    {
        median = (buff[length / 2 - 1] + buff[length / 2]) / 2.0f;
    } 
    else 
    {
        median = buff[length / 2];
    }

    //3.加权计算
    float weightedSum = 0.0;
    float weightSum = 0.0;
    for (int i = 0; i < length; i++) 
    {
        float weight = 1.0f / (1.0f + abs((int)(buff[i] - median)));  //权重
        weightedSum += buff[i] * weight;
        weightSum += weight;
    }

    return weightedSum / weightSum;  //返回加权均中位均值
}


/*****************************************************************************************************************************************/
/************************************************箱线图底噪识别和异常值剔除算法**************************************************************/
/*****************************************************************************************************************************************/
//实时箱线图最大值底噪识别
int16_t CalculateXiangxiantuMax_int16(int16_t *buff,int length) 
{
    // 1.排序
    sortArray_int16(buff,length);

    // 2.计算 Q1 与 Q3
    int Q1_index = length * 0.15f;  // 一四分位数索引
    int Q3_index = length * 0.85f;  // 三四分位数索引
    int16_t Q1 = buff[Q1_index];
    int16_t Q3 = buff[Q3_index];

    // 3.计算 IQR 和上下限
    int16_t IQR = Q3 - Q1;
    int16_t lower_limit = Q1 - 1.5f * IQR;
    int16_t upper_limit = Q3 + 1.5f * IQR;

    // 4.返回箱体的有效大小
	//return (abs(upper_limit) > abs(lower_limit)) ? abs(upper_limit) : abs(lower_limit); //(按键0切换)
	return abs(upper_limit-lower_limit); 
}

//算法实现：实时滑动窗口箱线图有效幅值识别
float CalculateXiangxiantuMax_Window(SlidingWindow_filter *window) 
{
    if (window->count < WINDOW_SIZE_Filter) 
	{
        //如果窗口未满，无法剔除异常值，直接接受数据
        return -1.0;
    }

    // 1. 复制缓冲区到临时数组进行排序
    float tempBuffer[WINDOW_SIZE_Filter];
    for (int i = 0; i < WINDOW_SIZE_Filter; i++) 
    {
        tempBuffer[i] = window->buffer[i];
    }
    sortArray_float(tempBuffer, WINDOW_SIZE_Filter);

    // 2. 计算 Q1 和 Q3
    int Q1_index = WINDOW_SIZE_Filter * 0.25f;  //第一四分位数索引
    int Q3_index = WINDOW_SIZE_Filter * 0.75f;  //第三四分位数索引
    float Q1 = tempBuffer[Q1_index];
    float Q3 = tempBuffer[Q3_index];

    // 3. 计算 IQR 和上下限
    float IQR = Q3 - Q1;
    float lower_limit = Q1 - 1.5f * IQR;
    float upper_limit = Q3 + 1.5f * IQR;

    // 4. 返回箱体的最大值
	//return (abs(upper_limit) > abs(lower_limit)) ? abs(upper_limit) : abs(lower_limit); //(按键0切换)
	return (upper_limit-lower_limit); 
}

//算法实现：实时滑动窗口箱线图异常值剔除
int isValueWithinBounds(SlidingWindow *window, float current_value)   ///为用到
{
    if (window->count < WINDOW_SIZE) 
    {
        //如果窗口未满，无法剔除异常值，直接接受数据
        return 1;
    }

    // 1. 复制缓冲区到临时数组进行排序
    float tempBuffer[WINDOW_SIZE];
    for (int i = 0; i < WINDOW_SIZE; i++) 
    {
        tempBuffer[i] = window->buffer[i];
    }
    sortArray_float(tempBuffer, WINDOW_SIZE);

    // 2. 计算 Q1 和 Q3
    int Q1_index = WINDOW_SIZE * 0.25f;  // 第一四分位数索引
    int Q3_index = WINDOW_SIZE * 0.75f;  // 第三四分位数索引
    float Q1 = tempBuffer[Q1_index];
    float Q3 = tempBuffer[Q3_index];

    // 3. 计算 IQR 和上下限
    float IQR = Q3 - Q1;
    float lower_limit = Q1 - 1.5f * IQR;
    float upper_limit = Q3 + 1.5f * IQR;

    // 4. 判断当前值是否在上下限范围内
    if (current_value >= lower_limit && current_value <= upper_limit) 
    {
        return 1;  // 值在范围内
    } 
    else 
    {
        return 0;  // 值为异常值
    }
}
 
//箱线图异常值剔除  算法实现：计算 IQR 并剔除异常值
void calculateIQR(float *data, int size, float *filtered_data, int *filtered_size)                   
{
    // 1. 排序数据（简单冒泡排序，适合小规模数据）
    for (int i = 0; i < size - 1; i++) 
    {
        for (int j = 0; j < size - i - 1; j++) 
        {
            if (data[j] > data[j + 1]) 
            {
                float temp = data[j];
                data[j] = data[j + 1];
                data[j + 1] = temp;
            }
        }
    }

    // 2. 计算 Q1 和 Q3
    int Q1_index = size * 0.15f; // 第一四分位数索引  0.25
    int Q3_index = size * 0.85f; // 第三四分位数索引  0.75
    float Q1 = data[Q1_index];
    float Q3 = data[Q3_index];

    // 3. 计算 IQR 和上下限
    float IQR = Q3 - Q1;
    float lower_limit = Q1 - 1.5f * IQR;
    float upper_limit = Q3 + 1.5f * IQR;

    // 4. 筛选非异常值数据
    *filtered_size = 0;
    for (int i = 0; i < size; i++) 
    {
        if (data[i] >= lower_limit && data[i] <= upper_limit) 
        {
            filtered_data[*filtered_size] = data[i];
            (*filtered_size)++;
        }
    }
}


 
//箱线图异常值剔除  算法实现：计算 IQR 并剔除异常值
void calculateIQR4Live(float *data, int size, float *filtered_data, int *filtered_size)                   
{
    // 1. 排序数据（简单冒泡排序，适合小规模数据）
    for (int i = 0; i < size - 1; i++) 
    {
        for (int j = 0; j < size - i - 1; j++) 
        {
            if (data[j] > data[j + 1]) 
            {
                float temp = data[j];
                data[j] = data[j + 1];
                data[j + 1] = temp;
            }
        }
    }

    // 2. 计算 Q1 和 Q3
    int Q1_index = size * 0.3f; // 第一四分位数索引  0.25
    int Q3_index = size * 0.7f; // 第三四分位数索引  0.75
    float Q1 = data[Q1_index];
    float Q3 = data[Q3_index];

    // 3. 计算 IQR 和上下限
    float IQR = Q3 - Q1;
    float lower_limit = Q1 - 1.5f * IQR;
    float upper_limit = Q3 + 1.5f * IQR;

    // 4. 筛选非异常值数据
    *filtered_size = 0;
    for (int i = 0; i < size; i++) 
    {
        if (data[i] >= lower_limit && data[i] <= upper_limit) 
        {
            filtered_data[*filtered_size] = data[i];
            (*filtered_size)++;
        }
    }
}

/*****************************************************************************************************************************************/
/********************************************************直方图峰值加权算法*****************************************************************/
/*****************************************************************************************************************************************/
//直方图计算函数
void compute_histogram(float *data, int data_length, int bins, int *counts, float *bin_centers) 
{
    //找到数据的最小值和最大值
    float min_val = data[0];
    float max_val = data[0];
    for (int i = 1; i < data_length; i++) 
    {
        if (data[i] < min_val) 
        {
            min_val = data[i];
        }
        if (data[i] > max_val) 
        {
            max_val = data[i];
        }
    }

    //计算每个分箱的宽度
    float bin_width = (max_val - min_val) / bins;

    //初始化计数数组和分箱中心
    for (int i = 0; i < bins; i++) 
    {
        counts[i] = 0;  //初始化计数为 0
        bin_centers[i] = min_val + (i + 0.5f) * bin_width;  //分箱中心
    }

    //遍历数据，将数据分配到分箱中
    for (int i = 0; i < data_length; i++) 
    {
        int bin_index = (int)((data[i] - min_val) / bin_width);
        if (bin_index >= bins) 
        {
            bin_index = bins - 1;  //将最大值分配到最后一个分箱
        }
        counts[bin_index]++;
    }
}

//直方图计算函数（固定7个分箱）
void compute_histogram7(float *data, int data_length, int *counts, float *bin_centers) 
{
    //固定分箱数量为7
    const int bins = 7;

    //找到数据的最小值和最大值
    float min_val = data[0];
    float max_val = data[0];
    for (int i = 1; i < data_length; i++) 
    {
        if (data[i] < min_val) 
        {
            min_val = data[i];
        }
        if (data[i] > max_val) 
        {
            max_val = data[i];
        }
    }

    //计算每个分箱的宽度
    float bin_width = (max_val - min_val) / bins;

    //初始化计数数组和分箱中心
    for (int i = 0; i < bins; i++) 
    {
        counts[i] = 0;  //初始化计数为 0
        bin_centers[i] = min_val + (i + 0.5f) * bin_width;  //分箱中心
    }

    //遍历数据，将数据分配到分箱中
    for (int i = 0; i < data_length; i++) 
    {
        int bin_index = (int)((data[i] - min_val) / bin_width);
        if (bin_index >= bins) 
        {
            bin_index = bins - 1;  //将最大值分配到最后一个分箱
        }
        counts[bin_index]++;
    }
}


int compare(const void *a, const void *b) 
{
    Peak *peakA = (Peak *)a;
    Peak *peakB = (Peak *)b;
    return (int)(peakB->value - peakA->value);  // 降序排列
}

float CalcHistogram5From7PeakMean(const int *H_vector, const float *C_vector, int n, float *max7_center, int *max7_counts) 
{
    // 输入检查
    if (n < 7 || H_vector == NULL || C_vector == NULL) 
    {
        if(flag_ESP_LOGI) ESP_LOGI(TAG,"Error: Invalid input or data length is too small.\n");
        return -1.0;  // 错误返回
    }

    // 找到前七个最大值及其索引
    Peak peaks[n];
    for (int i = 0; i < n; i++) 
    {
        peaks[i].value = H_vector[i];
        peaks[i].index = i;
    }

    // 排序（降序排列）
    qsort(peaks, n, sizeof(Peak), compare);

    // 检查是否有足够的峰值
    if (n < 7) 
    {
        if(flag_ESP_LOGI) ESP_LOGI(TAG,"Error: Failed to find top 7 peaks.\n");
        return -1.0;
    }

    /*******************输出按照色值降序排列*********************************** */
    float temp_max7_center[7];
    float temp_max7_counts[7];
    //输出按照中心升序排列
    for (int i = 0; i < 7; i++) 
    {
        temp_max7_counts[i] = peaks[i].value;
        temp_max7_center[i] = C_vector[peaks[i].index];
    }

    // 找到前七个最大值及其索引
    Peak peaks7[7];
    for (int i = 0; i < 7; i++) 
    {
        peaks7[i].value = temp_max7_center[i];
        peaks7[i].index = i;
    }
    // 排序（降序排列）
    qsort(peaks7, 7, sizeof(Peak), compare);

    for (int i = 0; i < 7; i++) 
    {
        max7_center[i] = peaks7[i].value;
        max7_counts[i] = temp_max7_counts[peaks7[i].index];
    }
    /****************************************************** */

    // 找到有效的峰值（如果某个峰值小于最大值的 0.707 倍，替换为最大值）
    for (int i = 1; i < 5; i++) 
    {
        if (peaks[i].value < round(0.707f * peaks[0].value)) 
        {
            peaks[i] = peaks[0];  // 用最大值替换
        }
    }

    // 获取相应的中心值
    float m[5];
    for (int i = 0; i < 5; i++) 
    {
        m[i] = C_vector[peaks[i].index];
    }

    // 加权计算
    float core[] = {5.0f, 4.0f, 3.0f, 2.0f, 1.0f};  // 权重
    float sum_core = 15.0f;  // sum_core 是固定的，不需要每次计算
    float weighted_mean = 0.0f;
    for (int i = 0; i < 5; i++) 
    {
        weighted_mean += m[i] * core[i];
    }

    weighted_mean /= sum_core;  // 计算加权平均值

    return weighted_mean;
}


//计算直方图的五个个尖峰值并求加权平均值
float CalcHistogram5PeakMean(const int *H_vector, const float *C_vector, int n)     
{
    // 输入检查
    if (n < 5 || H_vector == NULL || C_vector == NULL) 
    {
        if(flag_ESP_LOGI) ESP_LOGI(TAG,"Error: Invalid input or data length is too small.\n");
        return -1.0;  // 错误返回
    }

    // 找到前七个最大值及其索引
    Peak peaks[n];
    for (int i = 0; i < n; i++) 
    {
        peaks[i].value = H_vector[i];
        peaks[i].index = i;
    }

    // 排序（降序排列）
    qsort(peaks, n, sizeof(Peak), compare);

    // 检查是否有足够的峰值
    if (n < 5) 
    {
        //printf("Error: Failed to find top 5 peaks.\n");
        if(flag_ESP_LOGI) ESP_LOGI(TAG,"Error: Failed to find top 5 peaks.\n");
        return -1.0;
    }

    // 找到有效的峰值（如果某个峰值小于最大值的 0.707 倍，替换为最大值）
    for (int i = 1; i < 5; i++) 
    {
        if (peaks[i].value < round(0.707f * peaks[0].value)) 
        {
            peaks[i] = peaks[0];  // 用最大值替换
        }
    }

    // 获取相应的中心值
    float m[5];
    for (int i = 0; i < 5; i++) 
    {
        m[i] = C_vector[peaks[i].index];
    }

    // 加权计算
    float core[] = {5.0f, 4.0f, 3.0f, 2.0f, 1.0f};  // 权重
    float sum_core = 15.0f;  // sum_core 是固定的，不需要每次计算
    float weighted_mean = 0.0f;
    for (int i = 0; i < 5; i++) 
    {
        weighted_mean += m[i] * core[i];
    }

    weighted_mean /= sum_core;  // 计算加权平均值

    return weighted_mean;
}

//计算直方图的三个个尖峰值并求加权平均值
float CalcHistogram3PeakMean(const int *H_vector, const float *C_vector, int n)          ////////////////实时计算时可以考虑用这个//////////////////
{
    //输入检查
    if (n < 5 || H_vector == NULL || C_vector == NULL) 
	{
        if(flag_ESP_LOGI) ESP_LOGI(TAG,"Error: Invalid input or data length is too small. \n");
        return -1.0;  //错误返回
    }

    //找到前三个最大值
    int max1 = -1, max2 = -1, max3 = -1;        //保存前三个最大值
    int index1 = -1, index2 = -1, index3 = -1;  //对应索引

    for (int i = 0; i < n; i++) 
	{
        if (H_vector[i] > max1) 
		{
            //最大值下移
            max3 = max2;
            index3 = index2;
            max2 = max1;
            index2 = index1;
            max1 = H_vector[i];
            index1 = i;
        } 
		else if (H_vector[i] > max2) 
		{
            //第二大值下移
            max3 = max2;
            index3 = index2;
            max2 = H_vector[i];
            index2 = i;
        } 
		else if (H_vector[i] > max3) 
		{
            max3 = H_vector[i];
            index3 = i;
        }
    }	
	//printf("max1: %d max2: %d max3: %d \n", max1,max2,max3);

    //输出检查
    if (index1 == -1 || index2 == -1 || index3 == -1) 
	{
		if(flag_ESP_LOGI) ESP_LOGI(TAG,"Error: Failed to find top 3 peaks. \n");
        return -1.0;
    }

    //定义直方频率有效的次数，如果小于频率次数最多的0.707倍，则用最多频率次数取代之
	if(max2 < round(0.707f*max1)) index2 =index1;
	if(max3 < round(0.707f*max1)) index3 =index1;
	//printf("index1: %d index2: %d index3: %d \n", index1,index2,index3);
		
	//找到对应的中心值
    float m[3];
    m[0] = C_vector[index1]; //直方图峰值最大(出现频率次数最多)对应的值
    m[1] = C_vector[index2]; //直方图峰值二大(出现频率次多)对应的值
    m[2] = C_vector[index3]; //直方图峰值三大(出现频率第三多)对应的值
	//printf("m[0]: %.2f m[1]: %.2f m[2]: %.2f \n", m[0],m[1],m[2]);

    //加权计算
    float core[] = {3.0f, 2.0f, 1.0f};    //权重
    float sum_core = 1.0f + 2.0f + 3.0f;  //权重和
    float weighted_mean = (m[0] * core[0] + m[1] * core[1] + m[2] * core[2]) / sum_core;

    return weighted_mean;
}

//函数实现：计算直方图的峰值
int CalcHistogramPeak(const int *H_vector, const double *C_vector, int n, double *Res) 
{
    //输入检查，需要数据大于100个
    if (n <5 || H_vector == NULL || C_vector == NULL || Res == NULL) 
	{
        return -1; //错误条件
    }

    //找到最大值
    int max_value = H_vector[0];
    for (int i = 1; i < n; i++) 
	{
        if (H_vector[i] > max_value) 
		{
            max_value = H_vector[i];
        }
    }

    //找到最大值的索引
    int index = -1;
    for (int i = 0; i < n; i++) 
	{
        if (H_vector[i] == max_value) 
		{
            index = i;
            break; //找到第一个匹配的索引即可
        }
    }

    if (index != -1) 
	{
        *Res = C_vector[index]; //直方图峰值对应的中心值
        return 0;  //成功
    } 	
    else 
	{
        return -1; //未找到
    }
}

/*****************************************************************************************************************************************/
/****************************************************二维双调和样条插值算法*****************************************************************/
/*****************************************************************************************************************************************/
//径向基函数 (薄板样条)
float radial_basis_function(float r) 
{
    if (r == 0.0) return 0.0;
    return r * r * log(r);
}

//分块计算插值矩阵
void compute_interpolation_matrix_block(int n, const float *x, const float *y, float *C_block, int row_start, int col_start, int block_size) 
{
    for (int i = 0; i < block_size; i++) 
    {
        for (int j = 0; j < block_size; j++) 
        {
            int row = row_start + i;
            int col = col_start + j;
            if (row < n && col < n) 
            {
                if (row == col) 
                {
                    C_block[i * block_size + j] = 0.0f; //对角线设置为 0
                } 
                else 
                {
                    float dx = x[row] - x[col];
                    float dy = y[row] - y[col];
                    float r = sqrt(dx * dx + dy * dy);
                    C_block[i * block_size + j] = radial_basis_function(r);
                    //C_block[i * block_size + j] = gaussian_basis_function(r,0.5);
                }
            } 
            else 
            {
                C_block[i * block_size + j] = 0.0f; //超出矩阵范围的部分填充 0
            }
        }
    }
}

//分块构造插值矩阵
void compute_interpolation_matrix(int n, const float *x, const float *y, float *C) 
{
    int num_blocks = (n + BLOCK_SIZE - 1) / BLOCK_SIZE; //计算块的数量
    for (int block_row = 0; block_row < num_blocks; block_row++) 
    {
        for (int block_col = 0; block_col < num_blocks; block_col++) 
        {
            int row_start = block_row * BLOCK_SIZE;
            int col_start = block_col * BLOCK_SIZE;
            compute_interpolation_matrix_block(n, x, y, A_block, row_start, col_start, BLOCK_SIZE);

            //将块数据写回全局矩阵 C
            for (int i = 0; i < BLOCK_SIZE; i++) 
            {
                for (int j = 0; j < BLOCK_SIZE; j++) 
                {
                    int row = row_start + i;
                    int col = col_start + j;
                    if (row < n && col < n) 
                    {
                        C[row * n + col] = A_block[i * BLOCK_SIZE + j];
                    }
                }
            }
        }
    }
}

//分块高斯消元法
void gaussian_elimination(int n, float *C, float *B, float *X) 
{
    for (int i = 0; i < n; i++) 
    {
        //查找主元
        int max_row = i;
        for (int k = i + 1; k < n; k++) 
        {
            if (fabs(C[k * n + i]) > fabs(C[max_row * n + i])) 
            {
                max_row = k;
            }
        }

        //交换当前行和主元行
        for (int k = i; k < n; k++) 
        {
            float temp = C[i * n + k];
            C[i * n + k] = C[max_row * n + k];
            C[max_row * n + k] = temp;
        }
        float temp = B[i];
        B[i] = B[max_row];
        B[max_row] = temp;

        //如果主元接近零，矩阵可能是奇异的
        if (fabs(C[i * n + i]) < 1e-10) 
        {
            if(flag_ESP_LOGI) ESP_LOGI(TAG,"Matrix is singular or nearly singular\n");
        }

        //消元过程
        for (int k = i + 1; k < n; k++) 
        {
            float factor = C[k * n + i] / C[i * n + i];
            for (int j = i; j < n; j++) 
            {
                C[k * n + j] -= factor * C[i * n + j];
            }
            B[k] -= factor * B[i];
        }
    }

    //回代过程
    for (int i = n - 1; i >= 0; i--) 
    {
        X[i] = B[i];
        for (int j = i + 1; j < n; j++) 
        {
            X[i] -= C[i * n + j] * X[j];
        }
        X[i] /= C[i * n + i];
    }
}

//插值计算
float interpolate(int n, const float *x, const float *y, const float *lambda, float x0, float y0) 
{
    float result = 0.0f;
    for (int i = 0; i < n; i++) 
    {
        float dx = x0 - x[i];
        float dy = y0 - y[i];
        float r = sqrt(dx * dx + dy * dy);
        result += lambda[i] * radial_basis_function(r);
    }
    return result;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// // 高斯核函数
// float gaussian_weight(float distance, float sigma) 
// {
//     return expf(-0.5f * powf(distance / sigma, 2));
// }

// // 线性外推
// float extrapolate(float x0, float x1, float y0, float y1, float t) 
// {
//     float slope = (x1 - x0) / (y1 - y0);
//     return y0 + slope * (t - x0);
// }

// // 高斯核函数一维插值 + 线性外推
// float gaussian_interpolate(float x[], float y[], int n, float t, float sigma) 
// {
//     if (t < x[0]) 
//     {
//         return extrapolate(x[0], x[1], y[0], y[1], t);  // 左边外推
//     } 
//     else if (t > x[n - 1]) 
//     {
//         return extrapolate(x[n - 1], x[n - 2], y[n - 1], y[n - 2], t);  // 右边外推
//     }

//     // 正常插值
//     float sum = 0.0;
//     float weight_sum = 0.0;

//     for (int i = 0; i < n; i++) 
//     {
//         float distance = fabs(x[i] - t);
//         if (distance == 0) 
//         {
//             return y[i];
//         }
//         float weight = gaussian_weight(distance, sigma);
//         sum += weight * y[i];
//         weight_sum += weight;
//     }

//     return (weight_sum == 0.0) ? 0.0 : sum / weight_sum;  // 避免除以0
// }


//一维三次样条插值
void computeSplineCoeffs(float x[], float y[], SplineCoeffs* spline) 
{
    int i;
    float h[N_spline], alpha[N_spline], l[N_spline], mu[N_spline], z[N_spline];

    for (i = 0; i < N_spline; i++) 
    {
        spline->x[i] = x[i];
        spline->a[i] = y[i];
    }

    for (i = 0; i < N_spline - 1; i++)
        h[i] = x[i + 1] - x[i];

    for (i = 1; i < N_spline - 1; i++)
        alpha[i] = (3.0f / h[i]) * (spline->a[i + 1] - spline->a[i]) - (3.0f / h[i - 1]) * (spline->a[i] - spline->a[i - 1]);

    l[0] = 1.0f; mu[0] = 0.0f; z[0] = 0.0f;

    for (i = 1; i < N_spline - 1; i++) 
    {
        l[i] = 2.0f * (x[i + 1] - x[i - 1]) - h[i - 1] * mu[i - 1];
        mu[i] = h[i] / l[i];
        z[i] = (alpha[i] - h[i - 1] * z[i - 1]) / l[i];
    }

    l[N_spline - 1] = 1.0f; z[N_spline - 1] = 0.0f; spline->c[N_spline - 1] = 0.0f;

    for (i = N_spline - 2; i >= 0; i--) 
    {
        spline->c[i] = z[i] - mu[i] * spline->c[i + 1];
        spline->b[i] = (spline->a[i + 1] - spline->a[i]) / h[i] - h[i] * (spline->c[i + 1] + 2.0f * spline->c[i]) / 3.0f;
        spline->d[i] = (spline->c[i + 1] - spline->c[i]) / (3.0f * h[i]);
    }
}


float evalSpline(SplineCoeffs* spline, float x_val) 
{
    int i;

    // 外推：小于最小值
    if (x_val < spline->x[0]) 
    {
        // 使用第一个区间的系数进行外推
        float dx = x_val - spline->x[0];
        return spline->a[0] + spline->b[0] * dx + spline->c[0] * dx * dx + spline->d[0] * dx * dx * dx;
    }

    // 外推：大于最大值
    if (x_val > spline->x[N_spline - 1]) 
    {
        // 使用最后一个区间的系数进行外推
        float dx = x_val - spline->x[N_spline - 2]; // 使用倒数第二个点的系数进行外推
        return spline->a[N_spline - 2] + spline->b[N_spline - 2] * dx + spline->c[N_spline - 2] * dx * dx + spline->d[N_spline - 2] * dx * dx * dx;
    }

    // 正常插值
    for (i = N_spline - 2; i >= 0; i--) 
    {
        if (x_val >= spline->x[i]) break;
    }

    float dx = x_val - spline->x[i];
    return spline->a[i] + spline->b[i] * dx + spline->c[i] * dx * dx + spline->d[i] * dx * dx * dx;
}

//一维三次样条插值
void computeSplineCoeffs0(float x[], float y[], SplineCoeffs0* spline) 
{
    int i;
    float h[N0_spline], alpha[N0_spline], l[N0_spline], mu[N0_spline], z[N0_spline];

    for (i = 0; i < N0_spline; i++) 
    {
        spline->x[i] = x[i];
        spline->a[i] = y[i];
    }

    for (i = 0; i < N0_spline - 1; i++)
        h[i] = x[i + 1] - x[i];

    for (i = 1; i < N0_spline - 1; i++)
        alpha[i] = (3.0f / h[i]) * (spline->a[i + 1] - spline->a[i]) - (3.0f / h[i - 1]) * (spline->a[i] - spline->a[i - 1]);

    l[0] = 1.0f; mu[0] = 0.0f; z[0] = 0.0f;

    for (i = 1; i < N0_spline - 1; i++) 
    {
        l[i] = 2.0f * (x[i + 1] - x[i - 1]) - h[i - 1] * mu[i - 1];
        mu[i] = h[i] / l[i];
        z[i] = (alpha[i] - h[i - 1] * z[i - 1]) / l[i];
    }

    l[N0_spline - 1] = 1.0f; z[N0_spline - 1] = 0.0f; spline->c[N0_spline - 1] = 0.0f;

    for (i = N0_spline - 2; i >= 0; i--) 
    {
        spline->c[i] = z[i] - mu[i] * spline->c[i + 1];
        spline->b[i] = (spline->a[i + 1] - spline->a[i]) / h[i] - h[i] * (spline->c[i + 1] + 2.0f * spline->c[i]) / 3.0f;
        spline->d[i] = (spline->c[i + 1] - spline->c[i]) / (3.0f * h[i]);
    }
}


float evalSpline0(SplineCoeffs0* spline, float x_val) 
{
    int i;

    // 外推：小于最小值
    if (x_val < spline->x[0]) 
    {
        // 使用第一个区间的系数进行外推
        float dx = x_val - spline->x[0];
        return spline->a[0] + spline->b[0] * dx + spline->c[0] * dx * dx + spline->d[0] * dx * dx * dx;
    }

    // 外推：大于最大值
    if (x_val > spline->x[N0_spline - 1]) 
    {
        // 使用最后一个区间的系数进行外推
        float dx = x_val - spline->x[N0_spline - 2]; // 使用倒数第二个点的系数进行外推
        return spline->a[N0_spline - 2] + spline->b[N0_spline - 2] * dx + spline->c[N0_spline - 2] * dx * dx + spline->d[N0_spline - 2] * dx * dx * dx;
    }

    // 正常插值
    for (i = N0_spline - 2; i >= 0; i--) 
    {
        if (x_val >= spline->x[i]) break;
    }

    float dx = x_val - spline->x[i];
    return spline->a[i] + spline->b[i] * dx + spline->c[i] * dx * dx + spline->d[i] * dx * dx * dx;
}

    // SplineCoeffs spline;
    // computeSplineCoeffs(MD_X, MD_GY, &spline);

    // // 测试插值输出
    // float test_x[] = {5.0, 6.0, 10.0, 17.0};
    // for (int i = 0; i < 4; i++) {
    //     float y = evalSpline(&spline, test_x[i]);
    //     printf("Spline(%.2f) = %.4f\n", test_x[i], y);
    // }


void moving_average_filter_valid_only(int16_t* input, int16_t* output, int len, int window_size) {
    for (int i = 0; i < len; i++) {
        int32_t sum = 0;    // 防止溢出，使用 32 位
        int count = 0;

        for (int j = i; j > i - window_size && j >= 0; j--) {
            int16_t val = input[j];
            if (val != 0 && val != 32767) {
                sum += val;
                count++;
            }
        }

        if (count > 0)
            output[i] = (int16_t)((sum + (count / 2)) / count);  // 四舍五入
        else
            output[i] = 0;  // 全部是无效值
    }
}



int detect_signal_clusters_int16(int16_t* data, int len, int16_t* starts_out, int max_clusters, int16_t threshold, int zero_tolerance) {
    int count = 0;
    bool in_cluster = false;
    int zero_count = 0;

    for (int i = 8; i < len-20; i++) {
        int16_t val = data[i];

        // 忽略 32767
        if (val == 32767) {
            continue;  // 跳过，不参与任何逻辑
        }

        int16_t amp = abs(val);

        if (!in_cluster) {
            if (amp >= threshold) {
                // 新信号段开始
                if (count < max_clusters) {
                    starts_out[count++] = i;
                }
                in_cluster = true;
                zero_count = 0;
            }
        } else {
            if (amp < threshold) {
                zero_count++;
                if (zero_count >= zero_tolerance) {
                    in_cluster = false;  // 连续静默，结束段
                }
            } else {
                zero_count = 0;
            }
        }
    }

    return count;
}



// 排序比较函数
static int cmp_int16(const void* a, const void* b) {
    int16_t ia = *(const int16_t*)a;
    int16_t ib = *(const int16_t*)b;
    return (ia > ib) - (ia < ib);
}

int16_t median_mean_exclude_32767(const int16_t arr[], int size) {
    int16_t temp[size];
    int count = 0;
    int sum = 0;

    // 筛选有效值（非 0 且 ≠ 32767）
    for (int i = 0; i < size; i++) {
        if (arr[i] != 0 && arr[i] != 32767) {
            temp[count++] = arr[i];
            sum += arr[i];
        }
    }

    if (count == 0) return 0;  // 全部被剔除

    // 均值
    float mean = (float)sum / count;

    // 排序以计算中位数
    qsort(temp, count, sizeof(int16_t), cmp_int16);

    float median;
    if (count % 2 == 0) {
        median = (temp[count/2 - 1] + temp[count/2]) / 2.0f;
    } else {
        median = temp[count/2];
    }

    // 计算中位均值
    float result = (mean + median) / 2.0f;

    // 四舍五入并钳位
    if (result > 32767.0f) result = 32767.0f;
    if (result < -32768.0f) result = -32768.0f;

    return (int16_t)(result + 0.5f);
}


void rotate_left(int16_t* input, int16_t* output, int n, int len) 
{
    if (n <= 0 || n >= len) {
        memcpy(output, input, len * sizeof(int16_t));
        return;
    }

    // 将 input[n..len-1] 复制到 output[0..len-n-1]
    memcpy(output, input + n, (len - n) * sizeof(int16_t));

    // 将 input[0..n-1] 复制到 output[len-n..len-1]
    memcpy(output + (len - n), input, n * sizeof(int16_t));
}

// void rotate_left(int16_t* buf, int n, int len) 
// {
//     if (n <= 0 || n >= len) return; // 无需操作

//     int16_t temp[n];
//     memcpy(temp, buf, n * sizeof(int16_t));                    // 复制前 n 个元素
//     memmove(buf, buf + n, (len - n) * sizeof(int16_t));        // 后面的元素前移
//     memcpy(buf + (len - n), temp, n * sizeof(int16_t));        // 拼接前 n 个到最后
// }