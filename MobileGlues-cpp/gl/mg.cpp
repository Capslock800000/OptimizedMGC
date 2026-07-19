// MobileGlues - gl/mg.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#include <unistd.h>
#include <cstdarg>
#include <cstdio>
#include "mg.h"

#define DEBUG 0

hardware_t hardware;
gl_state_t gl_state;

FUNC_GL_STATE_SIZEI(proxy_width)
FUNC_GL_STATE_SIZEI(proxy_height)
FUNC_GL_STATE_ENUM(proxy_intformat)
FUNC_GL_STATE_UINT(current_program)
FUNC_GL_STATE_UINT(current_tex_unit)
FUNC_GL_STATE_UINT(current_draw_fbo)

#ifndef __APPLE__
FILE* file;
#endif

void start_log() {
#ifndef __APPLE__
    file = fopen(log_file_path, "a");
#endif
}

void write_log(const char* format, ...) {
#ifndef __APPLE__
    if (file == nullptr) {
        return;
    }
    va_list args;
    va_start(args, format);
    vfprintf(file, format, args);
    va_end(args);
    fprintf(file, "\n");
    fflush(file);
#if FORCE_SYNC_WITH_LOG_FILE == 1
    int fd = fileno(file);
    fsync(fd);
#endif
    // Todo: close file
    // fclose(file);
#endif
}

void write_log_n(const char* format, ...) {
#ifndef __APPLE__
    if (file == NULL) {
        return;
    }
    va_list args;
    va_start(args, format);
    vfprintf(file, format, args);
    va_end(args);
    // Todo: close file
    fflush(file);
#endif
}

void clear_log() {
#ifndef __APPLE__
    file = fopen(log_file_path, "w");
    if (file == nullptr) {
        return;
    }
    fclose(file);
#endif
}

GLenum pname_convert(GLenum pname) {
    switch (pname) {
    // TODO: Realize GL_TEXTURE_LOD_BIAS for other devices.
    case GL_TEXTURE_LOD_BIAS:
        return GL_TEXTURE_LOD_BIAS_QCOM;
    }
    return pname;
}

GLenum map_tex_target(GLenum target) {
    switch (target) {
    case GL_TEXTURE_1D:
    case GL_TEXTURE_3D:
    case GL_TEXTURE_RECTANGLE_ARB:
        return GL_TEXTURE_2D;

    case GL_PROXY_TEXTURE_1D:
    case GL_PROXY_TEXTURE_3D:
    case GL_PROXY_TEXTURE_RECTANGLE_ARB:
        return GL_PROXY_TEXTURE_2D;

    default:
        return target;
    }
}

// OptimizedMG merged implementation (kept inside existing file to avoid new build units)

/* BEGIN PerformanceMonitor.cpp */

PerformanceMonitor::PerformanceMonitor()
    : m_currentFrameIndex(0)
    , m_totalFrames(0)
    , m_frameCountSinceLastUpdate(0)
    , m_currentFPS(0.0f)
    , m_averageFrameTime(0.0f)
    , m_minFrameTime(1000.0f) // 初始化为1秒
    , m_maxFrameTime(0.0f)
    , m_frameTimeVariance(0.0f)
    , m_lastFPSUpdate(std::chrono::high_resolution_clock::now()) {
    
    // 初始化帧时间数组
    for (int i = 0; i < FRAME_TIME_HISTORY_SIZE; ++i) {
        m_frameTimes[i] = 0.0f;
    }
}

void PerformanceMonitor::updateFrameTime(float frameTime) {
    // 更新帧时间历史
    m_frameTimes[m_currentFrameIndex] = frameTime;
    m_currentFrameIndex = (m_currentFrameIndex + 1) % FRAME_TIME_HISTORY_SIZE;
    
    // 更新总帧数
    m_totalFrames++;
    m_frameCountSinceLastUpdate++;
    
    // 更新统计数据
    updateStatistics();
    
    // 更新FPS
    updateFPS();
}

void PerformanceMonitor::updateStatistics() {
    int validFrames = std::min(static_cast<int>(m_totalFrames), FRAME_TIME_HISTORY_SIZE);
    if (validFrames == 0) return;
    
    float sum = 0.0f;
    float minTime = m_frameTimes[0];
    float maxTime = m_frameTimes[0];
    
    // 计算总和、最小值和最大值
    for (int i = 0; i < validFrames; ++i) {
        float time = m_frameTimes[i];
        sum += time;
        
        if (time < minTime) minTime = time;
        if (time > maxTime) maxTime = time;
    }
    
    // 更新原子变量
    m_averageFrameTime.store(sum / validFrames, std::memory_order_relaxed);
    m_minFrameTime.store(minTime, std::memory_order_relaxed);
    m_maxFrameTime.store(maxTime, std::memory_order_relaxed);
    
    // 计算方差
    m_frameTimeVariance.store(calculateVariance(), std::memory_order_relaxed);
}

void PerformanceMonitor::updateFPS() {
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        currentTime - m_lastFPSUpdate
    ).count();
    
    // 每秒更新一次FPS
    if (elapsed >= 1000) {
        float fps = (m_frameCountSinceLastUpdate * 1000.0f) / elapsed;
        m_currentFPS.store(fps, std::memory_order_relaxed);
        
        m_frameCountSinceLastUpdate = 0;
        m_lastFPSUpdate = currentTime;
    }
}

float PerformanceMonitor::calculateVariance() const {
    int validFrames = std::min(static_cast<int>(m_totalFrames), FRAME_TIME_HISTORY_SIZE);
    if (validFrames < 2) return 0.0f;
    
    float mean = m_averageFrameTime.load(std::memory_order_relaxed);
    float sumSq = 0.0f;
    
    for (int i = 0; i < validFrames; ++i) {
        float diff = m_frameTimes[i] - mean;
        sumSq += diff * diff;
    }
    
    return sumSq / (validFrames - 1);
}

float PerformanceMonitor::getFrameTimePercentile(float percentile) const {
    int validFrames = std::min(static_cast<int>(m_totalFrames), FRAME_TIME_HISTORY_SIZE);
    if (validFrames == 0) return 0.0f;
    
    // 创建临时数组进行排序
    float sortedTimes[FRAME_TIME_HISTORY_SIZE];
    std::copy(m_frameTimes, m_frameTimes + validFrames, sortedTimes);
    std::sort(sortedTimes, sortedTimes + validFrames);
    
    // 计算百分位索引
    int index = static_cast<int>(percentile * validFrames / 100.0f);
    index = std::max(0, std::min(index, validFrames - 1));
    
    return sortedTimes[index];
}
/* END PerformanceMonitor.cpp */

/* BEGIN math/FastMath.cpp */

namespace OptimizedMath {

// 正弦查找表 (0 到 2PI，1024个采样点)
const float FastMath::SIN_TABLE[1025] = {
};

// 指数查找表 (-8 到 8，256个采样点)
const float FastMath::EXP_TABLE[256] = {
};

float FastMath::fastSqrt(float x) {
    if (x <= 0.0f) return 0.0f;
    
    // 使用位操作进行初始估计
    int32_t i = floatToInt(x);
    i = 0x5f3759df - (i >> 1);
    float y = intToFloat(i);
    
    // 一次牛顿迭代提高精度
    y = y * (1.5f - 0.5f * x * y * y);
    return x * y; // 修正结果
}

float FastMath::fastInverseSqrt(float x) {
    if (x <= 0.0f) return 0.0f;
    
    // 著名的Quake III反平方根算法
    int32_t i = floatToInt(x);
    i = 0x5f3759df - (i >> 1);
    float y = intToFloat(i);
    
    // 一次牛顿迭代提高精度
    y = y * (1.5f - 0.5f * x * y * y);
    return y;
}

float FastMath::fastSin(float x) {
    // 将角度规整到 [0, 2PI)
    x = reduceAngle(x);
    
    // 使用查找表
    float index = x * (1024.0f / TWO_PI);
    int i0 = static_cast<int>(index);
    int i1 = (i0 + 1) & 1023; // 循环索引
    float t = index - i0;
    
    // 线性插值
    return SIN_TABLE[i0] + t * (SIN_TABLE[i1] - SIN_TABLE[i0]);
}

float FastMath::fastCos(float x) {
    return fastSin(x + HALF_PI);
}

void FastMath::fastSinCos(float x, float& sin, float& cos) {
    x = reduceAngle(x);
    
    float index = x * (1024.0f / TWO_PI);
    int i0 = static_cast<int>(index);
    int i1 = (i0 + 1) & 1023;
    float t = index - i0;
    
    sin = SIN_TABLE[i0] + t * (SIN_TABLE[i1] - SIN_TABLE[i0]);
    cos = SIN_TABLE[(i0 + 256) & 1023] + t * (SIN_TABLE[(i1 + 256) & 1023] - SIN_TABLE[(i0 + 256) & 1023]);
}

float FastMath::fastExp(float x) {
    // 限制输入范围
    if (x < -8.0f) return 0.0f;
    if (x > 8.0f) return ::expf(x); // 回退到标准函数
    
    // 使用查找表
    float index = (x + 8.0f) * (255.0f / 16.0f);
    int i0 = static_cast<int>(index);
    int i1 = i0 + 1;
    if (i1 > 255) i1 = 255;
    float t = index - i0;
    
    return EXP_TABLE[i0] + t * (EXP_TABLE[i1] - EXP_TABLE[i0]);
}

float FastMath::fastLog(float x) {
    if (x <= 0.0f) return -INFINITY;
    
    // 使用对数恒等式和对数多项式近似
    int32_t i = floatToInt(x);
    int32_t exponent = ((i >> 23) & 0xFF) - 127;
    float mantissa = intToFloat((i & 0x007FFFFF) | 0x3F800000);
    
    // 对尾数使用多项式近似 log(1 + m)
    float m = mantissa - 1.0f;
    float log_m = m - 0.5f * m * m + 0.333333f * m * m * m - 0.25f * m * m * m * m;
    
    return exponent * 0.6931471805599453f + log_m; // ln(2) ≈ 0.693147
}

float FastMath::fastAtan2(float y, float x) {
    if (x == 0.0f) {
        if (y > 0.0f) return HALF_PI;
        if (y < 0.0f) return -HALF_PI;
        return 0.0f;
    }
    
    float abs_x = x < 0 ? -x : x;
    float abs_y = y < 0 ? -y : y;
    
    // 使用有理函数近似
    float a = (abs_x < abs_y) ? abs_x / abs_y : abs_y / abs_x;
    float s = a * a;
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    
    if (abs_y > abs_x) r = HALF_PI - r;
    if (x < 0) r = PI - r;
    if (y < 0) r = -r;
    
    return r;
}

void FastMath::fastSqrtArray(const float* input, float* output, size_t count) {
    for (size_t i = 0; i < count; i += 4) {
        size_t remaining = count - i;
        if (remaining >= 4) {
            // 使用NEON一次处理4个值
            float32x4_t vec = vld1q_f32(&input[i]);
            float32x4_t sqrt_vec = vmulq_f32(vec, vrsqrteq_f32(vec));
            vst1q_f32(&output[i], sqrt_vec);
        } else {
            // 处理剩余元素
            for (size_t j = i; j < count; ++j) {
                output[j] = fastSqrt(input[j]);
            }
        }
    }
}

void FastMath::fastInverseSqrtArray(const float* input, float* output, size_t count) {
    for (size_t i = 0; i < count; i += 4) {
        size_t remaining = count - i;
        if (remaining >= 4) {
            float32x4_t vec = vld1q_f32(&input[i]);
            float32x4_t inv_sqrt_vec = vrsqrteq_f32(vec);
            vst1q_f32(&output[i], inv_sqrt_vec);
        } else {
            for (size_t j = i; j < count; ++j) {
                output[j] = fastInverseSqrt(input[j]);
            }
        }
    }
}

void FastMath::fastSinArray(const float* input, float* output, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        output[i] = fastSin(input[i]);
    }
}

void FastMath::fastCosArray(const float* input, float* output, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        output[i] = fastCos(input[i]);
    }
}

float FastMath::reduceAngle(float x) {
    // 使用fmodf规整角度到 [0, 2PI)
    x = ::fmodf(x, TWO_PI);
    if (x < 0.0f) x += TWO_PI;
    return x;
}

int32_t FastMath::floatToInt(float x) {
    union {
        float f;
        int32_t i;
    } u;
    u.f = x;
    return u.i;
}

float FastMath::intToFloat(int32_t x) {
    union {
        float f;
        int32_t i;
    } u;
    u.i = x;
    return u.f;
}

} // namespace OptimizedMath
/* END math/FastMath.cpp */

/* BEGIN math/VectorMath.cpp */

namespace OptimizedMath {

// Vector4 实现
Vector4 Vector4::add(const Vector4& a, const Vector4& b) {
    Vector4 result;
    result.neon = vaddq_f32(a.neon, b.neon);
    return result;
}

Vector4 Vector4::subtract(const Vector4& a, const Vector4& b) {
    Vector4 result;
    result.neon = vsubq_f32(a.neon, b.neon);
    return result;
}

Vector4 Vector4::multiply(const Vector4& a, const Vector4& b) {
    Vector4 result;
    result.neon = vmulq_f32(a.neon, b.neon);
    return result;
}

Vector4 Vector4::scale(const Vector4& a, float scalar) {
    Vector4 result;
    float32x4_t scale_vec = vdupq_n_f32(scalar);
    result.neon = vmulq_f32(a.neon, scale_vec);
    return result;
}

float Vector4::dot(const Vector4& a, const Vector4& b) {
    float32x4_t mul = vmulq_f32(a.neon, b.neon);
    float32x2_t sum2 = vadd_f32(vget_high_f32(mul), vget_low_f32(mul));
    return vget_lane_f32(vpadd_f32(sum2, sum2), 0);
}

Vector4 Vector4::cross(const Vector4& a, const Vector4& b) {
    return Vector4(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
        0.0f
    );
}

Vector4 Vector4::normalize(const Vector4& a) {
    float len = length(a);
    if (len > 0.0f) {
        float invLen = 1.0f / len;
        return scale(a, invLen);
    }
    return a;
}

float Vector4::length(const Vector4& a) {
    return FastMath::sqrt(dot(a, a));
}

float Vector4::lengthSquared(const Vector4& a) {
    return dot(a, a);
}

Vector4 Vector4::fastNormalize(const Vector4& a) {
    float len = fastLength(a);
    if (len > 0.0f) {
        float invLen = FastMath::fastInverseSqrt(len * len);
        return scale(a, invLen);
    }
    return a;
}

float Vector4::fastLength(const Vector4& a) {
    return FastMath::fastSqrt(dot(a, a));
}

float Vector4::fastInverseLength(const Vector4& a) {
    return FastMath::fastInverseSqrt(dot(a, a));
}

// Vector3 实现
Vector3 Vector3::add(const Vector3& a, const Vector3& b) {
    return Vector3(a.x + b.x, a.y + b.y, a.z + b.z);
}

Vector3 Vector3::subtract(const Vector3& a, const Vector3& b) {
    return Vector3(a.x - b.x, a.y - b.y, a.z - b.z);
}

Vector3 Vector3::multiply(const Vector3& a, const Vector3& b) {
    return Vector3(a.x * b.x, a.y * b.y, a.z * b.z);
}

Vector3 Vector3::scale(const Vector3& a, float scalar) {
    return Vector3(a.x * scalar, a.y * scalar, a.z * scalar);
}

float Vector3::dot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vector3 Vector3::cross(const Vector3& a, const Vector3& b) {
    return Vector3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

Vector3 Vector3::normalize(const Vector3& a) {
    float len = length(a);
    if (len > 0.0f) {
        float invLen = 1.0f / len;
        return scale(a, invLen);
    }
    return a;
}

float Vector3::length(const Vector3& a) {
    return FastMath::sqrt(dot(a, a));
}

float Vector3::lengthSquared(const Vector3& a) {
    return dot(a, a);
}

Vector3 Vector3::fastNormalize(const Vector3& a) {
    float lenSq = lengthSquared(a);
    if (lenSq > 0.0f) {
        float invLen = FastMath::fastInverseSqrt(lenSq);
        return scale(a, invLen);
    }
    return a;
}

float Vector3::fastLength(const Vector3& a) {
    return FastMath::fastSqrt(dot(a, a));
}

// Vector2 实现
Vector2 Vector2::add(const Vector2& a, const Vector2& b) {
    return Vector2(a.x + b.x, a.y + b.y);
}

Vector2 Vector2::subtract(const Vector2& a, const Vector2& b) {
    return Vector2(a.x - b.x, a.y - b.y);
}

Vector2 Vector2::multiply(const Vector2& a, const Vector2& b) {
    return Vector2(a.x * b.x, a.y * b.y);
}

Vector2 Vector2::scale(const Vector2& a, float scalar) {
    return Vector2(a.x * scalar, a.y * scalar);
}

float Vector2::dot(const Vector2& a, const Vector2& b) {
    return a.x * b.x + a.y * b.y;
}

float Vector2::length(const Vector2& a) {
    return FastMath::sqrt(dot(a, a));
}

float Vector2::lengthSquared(const Vector2& a) {
    return dot(a, a);
}

Vector2 Vector2::normalize(const Vector2& a) {
    float len = length(a);
    if (len > 0.0f) {
        float invLen = 1.0f / len;
        return scale(a, invLen);
    }
    return a;
}

// BatchVectorOperations 实现
void BatchVectorOperations::addVectors4(const Vector4* a, const Vector4* b, Vector4* result, size_t count) {
    // 使用NEON进行批量加法
    for (size_t i = 0; i < count; i += 4) {
        size_t remaining = count - i;
        if (remaining >= 4) {
            // 一次处理4个向量
            float32x4x4_t a4 = vld4q_f32(&a[i].x);
            float32x4x4_t b4 = vld4q_f32(&b[i].x);
            
            float32x4x4_t result4;
            result4.val[0] = vaddq_f32(a4.val[0], b4.val[0]);
            result4.val[1] = vaddq_f32(a4.val[1], b4.val[1]);
            result4.val[2] = vaddq_f32(a4.val[2], b4.val[2]);
            result4.val[3] = vaddq_f32(a4.val[3], b4.val[3]);
            
            vst4q_f32(&result[i].x, result4);
        } else {
            // 处理剩余向量
            for (size_t j = i; j < count; ++j) {
                result[j] = Vector4::add(a[j], b[j]);
            }
        }
    }
}

void BatchVectorOperations::addVectors3(const Vector3* a, const Vector3* b, Vector3* result, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        result[i] = Vector3::add(a[i], b[i]);
    }
}

void BatchVectorOperations::scaleVectors4(const Vector4* vectors, float scalar, Vector4* result, size_t count) {
    float32x4_t scale_vec = vdupq_n_f32(scalar);
    for (size_t i = 0; i < count; ++i) {
        result[i].neon = vmulq_f32(vectors[i].neon, scale_vec);
    }
}

void BatchVectorOperations::scaleVectors3(const Vector3* vectors, float scalar, Vector3* result, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        result[i] = Vector3::scale(vectors[i], scalar);
    }
}

void BatchVectorOperations::dotProducts4(const Vector4* a, const Vector4* b, float* results, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        results[i] = Vector4::dot(a[i], b[i]);
    }
}

void BatchVectorOperations::dotProducts3(const Vector3* a, const Vector3* b, float* results, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        results[i] = Vector3::dot(a[i], b[i]);
    }
}

void BatchVectorOperations::normalizeVectors4(Vector4* vectors, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        vectors[i] = Vector4::normalize(vectors[i]);
    }
}

void BatchVectorOperations::normalizeVectors3(Vector3* vectors, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        vectors[i] = Vector3::normalize(vectors[i]);
    }
}

void BatchVectorOperations::fastNormalizeVectors4(Vector4* vectors, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        vectors[i] = Vector4::fastNormalize(vectors[i]);
    }
}

void BatchVectorOperations::fastNormalizeVectors3(Vector3* vectors, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        vectors[i] = Vector3::fastNormalize(vectors[i]);
    }
}

} // namespace OptimizedMath
/* END math/VectorMath.cpp */

/* BEGIN math/MatrixMath.cpp */

namespace OptimizedMath {

Matrix4::Matrix4() {
    // 初始化为单位矩阵
    rows[0] = vsetq_lane_f32(1.0f, vdupq_n_f32(0.0f), 0);
    rows[1] = vsetq_lane_f32(1.0f, vdupq_n_f32(0.0f), 1);
    rows[2] = vsetq_lane_f32(1.0f, vdupq_n_f32(0.0f), 2);
    rows[3] = vsetq_lane_f32(1.0f, vdupq_n_f32(0.0f), 3);
}

Matrix4::Matrix4(const Matrix4& other) {
    rows[0] = other.rows[0];
    rows[1] = other.rows[1];
    rows[2] = other.rows[2];
    rows[3] = other.rows[3];
}

Matrix4 Matrix4::identity() {
    return Matrix4();
}

Matrix4 Matrix4::zero() {
    Matrix4 result;
    result.rows[0] = vdupq_n_f32(0.0f);
    result.rows[1] = vdupq_n_f32(0.0f);
    result.rows[2] = vdupq_n_f32(0.0f);
    result.rows[3] = vdupq_n_f32(0.0f);
    return result;
}

Matrix4 Matrix4::translation(float x, float y, float z) {
    Matrix4 result = identity();
    result.m[3][0] = x;
    result.m[3][1] = y;
    result.m[3][2] = z;
    return result;
}

Matrix4 Matrix4::scale(float x, float y, float z) {
    Matrix4 result = identity();
    result.m[0][0] = x;
    result.m[1][1] = y;
    result.m[2][2] = z;
    return result;
}

Matrix4 Matrix4::rotationX(float angle) {
    Matrix4 result = identity();
    float s = FastMath::sin(angle);
    float c = FastMath::cos(angle);
    
    result.m[1][1] = c;
    result.m[1][2] = -s;
    result.m[2][1] = s;
    result.m[2][2] = c;
    
    return result;
}

Matrix4 Matrix4::rotationY(float angle) {
    Matrix4 result = identity();
    float s = FastMath::sin(angle);
    float c = FastMath::cos(angle);
    
    result.m[0][0] = c;
    result.m[0][2] = s;
    result.m[2][0] = -s;
    result.m[2][2] = c;
    
    return result;
}

Matrix4 Matrix4::rotationZ(float angle) {
    Matrix4 result = identity();
    float s = FastMath::sin(angle);
    float c = FastMath::cos(angle);
    
    result.m[0][0] = c;
    result.m[0][1] = -s;
    result.m[1][0] = s;
    result.m[1][1] = c;
    
    return result;
}

Matrix4 Matrix4::perspective(float fov, float aspect, float near, float far) {
    Matrix4 result = zero();
    
    float f = 1.0f / FastMath::tan(fov * 0.5f);
    float rangeInv = 1.0f / (near - far);
    
    result.m[0][0] = f / aspect;
    result.m[1][1] = f;
    result.m[2][2] = (far + near) * rangeInv;
    result.m[2][3] = -1.0f;
    result.m[3][2] = 2.0f * far * near * rangeInv;
    
    return result;
}

Matrix4 Matrix4::orthographic(float left, float right, float bottom, float top, float near, float far) {
    Matrix4 result = identity();
    
    result.m[0][0] = 2.0f / (right - left);
    result.m[1][1] = 2.0f / (top - bottom);
    result.m[2][2] = -2.0f / (far - near);
    
    result.m[3][0] = -(right + left) / (right - left);
    result.m[3][1] = -(top + bottom) / (top - bottom);
    result.m[3][2] = -(far + near) / (far - near);
    
    return result;
}

Matrix4 Matrix4::multiply(const Matrix4& a, const Matrix4& b) {
    Matrix4 result;
    
    // 使用NEON优化的矩阵乘法
    for (int i = 0; i < 4; ++i) {
        float32x4_t row = a.rows[i];
        
        float32x4_t result_row = vmulq_n_f32(b.rows[0], vgetq_lane_f32(row, 0));
        result_row = vmlaq_n_f32(result_row, b.rows[1], vgetq_lane_f32(row, 1));
        result_row = vmlaq_n_f32(result_row, b.rows[2], vgetq_lane_f32(row, 2));
        result_row = vmlaq_n_f32(result_row, b.rows[3], vgetq_lane_f32(row, 3));
        
        result.rows[i] = result_row;
    }
    
    return result;
}

Vector4 Matrix4::multiply(const Matrix4& m, const Vector4& v) {
    Vector4 result;
    
    float32x4_t vec = v.neon;
    float32x4_t result_vec = vmulq_n_f32(m.rows[0], vgetq_lane_f32(vec, 0));
    result_vec = vmlaq_n_f32(result_vec, m.rows[1], vgetq_lane_f32(vec, 1));
    result_vec = vmlaq_n_f32(result_vec, m.rows[2], vgetq_lane_f32(vec, 2));
    result_vec = vmlaq_n_f32(result_vec, m.rows[3], vgetq_lane_f32(vec, 3));
    
    result.neon = result_vec;
    return result;
}

Matrix4 Matrix4::transpose(const Matrix4& m) {
    Matrix4 result;
    
    // 使用NEON转置
    float32x4x2_t a01 = vtrnq_f32(m.rows[0], m.rows[1]);
    float32x4x2_t a23 = vtrnq_f32(m.rows[2], m.rows[3]);
    
    result.rows[0] = vcombine_f32(vget_low_f32(a01.val[0]), vget_low_f32(a23.val[0]));
    result.rows[1] = vcombine_f32(vget_low_f32(a01.val[1]), vget_low_f32(a23.val[1]));
    result.rows[2] = vcombine_f32(vget_high_f32(a01.val[0]), vget_high_f32(a23.val[0]));
    result.rows[3] = vcombine_f32(vget_high_f32(a01.val[1]), vget_high_f32(a23.val[1]));
    
    return result;
}

Matrix4 Matrix4::inverse(const Matrix4& m) {
    // 这里实现完整的4x4矩阵求逆
    // 为简化，这里返回单位矩阵，实际实现需要完整的求逆算法
    return identity();
}

float Matrix4::determinant(const Matrix4& m) {
    // 4x4矩阵行列式计算
    // 为简化，这里返回1，实际实现需要完整的行列式计算
    return 1.0f;
}

Matrix4 Matrix4::fastInverse(const Matrix4& m) {
    // 快速近似逆矩阵，假设是刚体变换矩阵
    Matrix4 result = transpose(m);
    
    // 提取平移部分并取反
    Vector4 translation(m.m[3][0], m.m[3][1], m.m[3][2], 0.0f);
    translation = Vector4::scale(translation, -1.0f);
    
    // 重新计算平移
    result.m[3][0] = Vector4::dot(Vector4(m.m[0][0], m.m[1][0], m.m[2][0], 0.0f), translation);
    result.m[3][1] = Vector4::dot(Vector4(m.m[0][1], m.m[1][1], m.m[2][1], 0.0f), translation);
    result.m[3][2] = Vector4::dot(Vector4(m.m[0][2], m.m[1][2], m.m[2][2], 0.0f), translation);
    
    return result;
}

Matrix4& Matrix4::operator=(const Matrix4& other) {
    if (this != &other) {
        rows[0] = other.rows[0];
        rows[1] = other.rows[1];
        rows[2] = other.rows[2];
        rows[3] = other.rows[3];
    }
    return *this;
}

Matrix4 Matrix4::operator*(const Matrix4& other) const {
    return multiply(*this, other);
}

Vector4 Matrix4::operator*(const Vector4& v) const {
    return multiply(*this, v);
}

// BatchMatrixOperations 实现
void BatchMatrixOperations::multiplyMatrices(const Matrix4* a, const Matrix4* b, Matrix4* result, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        result[i] = Matrix4::multiply(a[i], b[i]);
    }
}

void BatchMatrixOperations::transformVectors4(const Matrix4* matrices, const Vector4* vectors, Vector4* result, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        result[i] = Matrix4::multiply(matrices[i], vectors[i]);
    }
}

void BatchMatrixOperations::transformVectors3(const Matrix4* matrices, const Vector3* vectors, Vector3* result, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        Vector4 vec4(vectors[i].x, vectors[i].y, vectors[i].z, 1.0f);
        Vector4 transformed = Matrix4::multiply(matrices[i], vec4);
        result[i] = Vector3(transformed.x, transformed.y, transformed.z);
    }
}

void BatchMatrixOperations::transposeMatrices(Matrix4* matrices, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        matrices[i] = Matrix4::transpose(matrices[i]);
    }
}

void BatchMatrixOperations::generatePerspectiveMatrices(float fov, float aspect, float near, float far, 
                                                       Matrix4* matrices, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        matrices[i] = Matrix4::perspective(fov, aspect, near, far);
    }
}

} // namespace OptimizedMath
/* END math/MatrixMath.cpp */
