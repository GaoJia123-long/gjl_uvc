
#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"


#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************************************/
//插值使用
#define N  (16*17)   //(39*11)  //(31*9)                   //插值数组大小 116
#define BLOCK_SIZE 10           // 块大小，根据内存限制调整 50
//滑窗使用
#define WINDOW_SIZE 100         //滑动窗口---色值直方图峰值加权滤波
#define WINDOW_SIZE_Filter 50   //滑动窗口---色值箱线图有效振幅检测
#define WINDOW_SIZE_Filter2 10   //滑动窗口---色值显示
// //滤波使用
// #define WAVELET_LEVEL 3
// #define SIGNAL_LENGTH 20  

//滑动窗口结构体
typedef struct 
{
    float buffer[WINDOW_SIZE];     // 环形缓冲区存储滑动窗口数据
    int head;                      // 当前写入位置
    int count;                     // 当前窗口内有效数据数量
} SlidingWindow;


//滑动窗口结构体
typedef struct 
{
    float buffer[WINDOW_SIZE_Filter];  // 环形缓冲区存储滑动窗口数据
    int head;                          // 当前写入位置
    int count;                         // 当前窗口内有效数据数量
} SlidingWindow_filter;

//滑动窗口结构体
typedef struct 
{
    float buffer[WINDOW_SIZE_Filter2];  // 环形缓冲区存储滑动窗口数据
    int head;                          // 当前写入位置
    int count;                         // 当前窗口内有效数据数量
} SlidingWindow_filter2;

typedef struct 
{
    float value;
    int index;
} Peak;

//三次样条插值函数
// 数据点数量
#define N_spline 11 //12 6
#define N0_spline 10

// 三次样条结构
typedef struct 
{
    float a[N_spline], b[N_spline], c[N_spline], d[N_spline], x[N_spline];
} SplineCoeffs;

// 三次样条结构
typedef struct 
{
    float a[N0_spline], b[N0_spline], c[N0_spline], d[N0_spline], x[N0_spline];
} SplineCoeffs0;

//冒泡排序
int partition(int16_t *array, int low, int high) ;
void quickSort(int16_t *array, int low, int high) ;
void sortArray_int16(int16_t *array, int size);
void sortArray_float(float *array, int size);
int compare(const void *a, const void *b);

//滑动窗口使用
void initSlidingWindow(SlidingWindow *window) ;
void initSlidingWindow_filter(SlidingWindow_filter *window);
void initSlidingWindow_filter2(SlidingWindow_filter2 *window);
void addToSlidingWindow(SlidingWindow *window, float value);
void addToSlidingWindow4Filter(SlidingWindow_filter *window, float value);
void addToSlidingWindow4Filter2(SlidingWindow_filter2 *window, float value);

//小波变换和中位加权滤波算法使用
float calculateWeightedMedianMean_int16(int16_t *buff,int length);
float calculateWeightedMedianMean_float(float *buff,int length) ;
float calculateWeightedMedianMean_window(SlidingWindow_filter *window);
// void wavedec(float* signal, float* d, float* a, int level, int length);
// void wrcoef(float* d, float* a, float* signal, int level, int length);

//箱线图底噪识别和异常值剔除算法使用
int isValueWithinBounds(SlidingWindow *window, float current_value) ;
void calculateIQR4Live(float *data, int size, float *filtered_data, int *filtered_size) ;      
void calculateIQR(float *data, int size, float *filtered_data, int *filtered_size) ;
float CalculateXiangxiantuMax_Window(SlidingWindow_filter *window);
int16_t CalculateXiangxiantuMax_int16(int16_t *buff,int length);

//直方图算法使用
float CalcHistogram5From7PeakMean(const int *H_vector, const float *C_vector, int n, float *max7_center, int *max7_counts);
float CalcHistogram5PeakMean(const int *H_vector, const float *C_vector, int n);
float CalcHistogram3PeakMean(const int *H_vector, const float *C_vector, int n);
int CalcHistogramPeak(const int *H_vector, const double *C_vector, int n, double *Res);
void compute_histogram(float *data, int data_length, int bins, int *counts, float *bin_centers) ;
void compute_histogram7(float *data, int data_length, int *counts, float *bin_centers);

//插值算法使用
void compute_interpolation_matrix_block(int n, const float *x, const float *y, float *C_block, int row_start, int col_start, int block_size);
void compute_interpolation_matrix(int n, const float *x, const float *y, float *C) ;
void gaussian_elimination(int n, float *C, float *B, float *X)  ;
float radial_basis_function(float r);
float interpolate(int n, const float *x, const float *y, const float *lambda, float x0, float y0);
// float gaussian_weight(float distance, float sigma) ;
// float extrapolate(float x0, float x1, float y0, float y1, float t);
// float gaussian_interpolate(float x[], float y[], int n, float t, float sigma) ;
void computeSplineCoeffs(float x[], float y[], SplineCoeffs* spline);
float evalSpline(SplineCoeffs* spline, float x_val);
void computeSplineCoeffs0(float x[], float y[], SplineCoeffs0* spline);
float evalSpline0(SplineCoeffs0* spline, float x_val);
void moving_average_filter_valid_only(int16_t* input, int16_t* output, int len, int window_size);
int detect_signal_clusters_int16(int16_t* data, int len, int16_t* starts_out, int max_clusters, int16_t threshold, int zero_tolerance);

static int cmp_int16(const void* a, const void* b);
int16_t median_mean_exclude_32767(const int16_t arr[], int size);
void rotate_left(int16_t* input, int16_t* output, int n, int len) ;
/*********************************************************************************************/

#ifdef __cplusplus
}
#endif
 