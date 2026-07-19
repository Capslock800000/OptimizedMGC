// MobileGlues - includes.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

#ifndef MOBILEGLUES_INCLUDES_H
#define MOBILEGLUES_INCLUDES_H

#define RENDERERNAME "OptimizedMG"
#ifdef __ANDROID__
#include <android/log.h>
#endif
#include <dlfcn.h>

#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <MG/extensions.h>

#include "egl/egl.h"
#include "egl/loader.h"

#if PROFILING
#include <perfetto.h>
PERFETTO_DEFINE_CATEGORIES(perfetto::Category("glcalls").SetDescription("Calls from OpenGL"),
                           perfetto::Category("internal").SetDescription("Internal calls"));
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    static int g_initialized = 0;

    void proc_init();

#ifdef __cplusplus
}
#endif

#include <FastSTL/UnorderedMap.h>

template <typename Key, typename T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>,
          class Allocator = std::allocator<std::pair<const Key, T>>>
using UnorderedMap = FastSTL::unordered_map<Key, T, Hash, KeyEqual, Allocator>;

#endif // MOBILEGLUES_INCLUDES_H

// OptimizedMG merged optimization headers

/* BEGIN PerformanceMonitor.h */
#include <atomic>
#include <chrono>
#include <cmath>

/**
 * 极致优化的性能监控器（C++实现）
 */
class PerformanceMonitor {
private:
    // 帧时间统计
    static constexpr int FRAME_TIME_HISTORY_SIZE = 120;
    float m_frameTimes[FRAME_TIME_HISTORY_SIZE];
    int m_currentFrameIndex;
    std::atomic<uint64_t> m_totalFrames;
    
    // FPS计算
    std::chrono::high_resolution_clock::time_point m_lastFPSUpdate;
    std::atomic<uint64_t> m_frameCountSinceLastUpdate;
    std::atomic<float> m_currentFPS;
    
    // 统计信息
    std::atomic<float> m_averageFrameTime;
    std::atomic<float> m_minFrameTime;
    std::atomic<float> m_maxFrameTime;
    std::atomic<float> m_frameTimeVariance;
    
public:
    PerformanceMonitor();
    ~PerformanceMonitor() = default;
    
    // 帧时间更新
    void updateFrameTime(float frameTime);
    
    // 统计数据获取
    float getCurrentFPS() const { return m_currentFPS.load(std::memory_order_relaxed); }
    float getAverageFrameTime() const { return m_averageFrameTime.load(std::memory_order_relaxed); }
    float getMinFrameTime() const { return m_minFrameTime.load(std::memory_order_relaxed); }
    float getMaxFrameTime() const { return m_maxFrameTime.load(std::memory_order_relaxed); }
    float getFrameTimeVariance() const { return m_frameTimeVariance.load(std::memory_order_relaxed); }
    
    // 百分位计算
    float getFrameTimePercentile(float percentile) const;
    
private:
    void updateStatistics();
    void updateFPS();
    float calculateVariance() const;
};
/* END PerformanceMonitor.h */

// OptimizedMG merged optimization headers

/* BEGIN math/FastMath.h */
#include <cmath>
#include <arm_neon.h>

namespace OptimizedMath {

class FastMath {
public:
    // 快速平方根 - 使用位操作和牛顿迭代法
    static float fastSqrt(float x);
    
    // 快速反平方根 - 著名的Quake III算法
    static float fastInverseSqrt(float x);
    
    // 快速正弦和余弦 - 使用查表法和多项式近似
    static float fastSin(float x);
    static float fastCos(float x);
    static void fastSinCos(float x, float& sin, float& cos);
    
    // 快速指数函数
    static float fastExp(float x);
    
    // 快速对数函数
    static float fastLog(float x);
    
    // 快速atan2
    static float fastAtan2(float y, float x);
    
    // 标准数学函数的包装（在需要精确性时使用）
    static float sqrt(float x) { return ::sqrtf(x); }
    static float sin(float x) { return ::sinf(x); }
    static float cos(float x) { return ::cosf(x); }
    static float tan(float x) { return ::tanf(x); }
    static float exp(float x) { return ::expf(x); }
    static float log(float x) { return ::logf(x); }
    static float atan2(float y, float x) { return ::atan2f(y, x); }
    
    // 批量运算
    static void fastSqrtArray(const float* input, float* output, size_t count);
    static void fastInverseSqrtArray(const float* input, float* output, size_t count);
    static void fastSinArray(const float* input, float* output, size_t count);
    static void fastCosArray(const float* input, float* output, size_t count);
    
    // 常量
    static constexpr float PI = 3.14159265358979323846f;
    static constexpr float TWO_PI = 2.0f * PI;
    static constexpr float HALF_PI = 0.5f * PI;
    static constexpr float INV_PI = 1.0f / PI;
    static constexpr float INV_TWO_PI = 1.0f / TWO_PI;
    
private:
    // 查表法用的查找表
    static const float SIN_TABLE[1025];
    static const float EXP_TABLE[256];
    
    // 辅助函数
    static float reduceAngle(float x);
    static int32_t floatToInt(float x);
    static float intToFloat(int32_t x);
};

} // namespace OptimizedMath
/* END math/FastMath.h */

// OptimizedMG merged optimization headers

/* BEGIN math/VectorMath.h */
#include <cstddef>
#include <arm_neon.h>

namespace OptimizedMath {

// 对齐内存分配
#ifndef ALIGNED
#define ALIGNED(x) __attribute__((aligned(x)))
#endif

// 4维向量 - 针对NEON优化
struct ALIGNED(16) Vector4 {
    union {
        struct { float x, y, z, w; };
        float data[4];
        float32x4_t neon;
    };
    
    Vector4() : x(0), y(0), z(0), w(0) {}
    Vector4(float x, float y, float z, float w = 1.0f) : x(x), y(y), z(z), w(w) {}
    
    // NEON优化的向量运算
    static Vector4 add(const Vector4& a, const Vector4& b);
    static Vector4 subtract(const Vector4& a, const Vector4& b);
    static Vector4 multiply(const Vector4& a, const Vector4& b);
    static Vector4 scale(const Vector4& a, float scalar);
    static float dot(const Vector4& a, const Vector4& b);
    static Vector4 cross(const Vector4& a, const Vector4& b);
    static Vector4 normalize(const Vector4& a);
    static float length(const Vector4& a);
    static float lengthSquared(const Vector4& a);
    
    // 快速近似运算
    static Vector4 fastNormalize(const Vector4& a);
    static float fastLength(const Vector4& a);
    static float fastInverseLength(const Vector4& a);
};

// 3维向量
struct ALIGNED(16) Vector3 {
    union {
        struct { float x, y, z; };
        float data[3];
        float32x4_t neon; // 仍然使用128位对齐以便NEON操作
    };
    
    Vector3() : x(0), y(0), z(0) {}
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
    
    // 高效向量运算
    static Vector3 add(const Vector3& a, const Vector3& b);
    static Vector3 subtract(const Vector3& a, const Vector3& b);
    static Vector3 multiply(const Vector3& a, const Vector3& b);
    static Vector3 scale(const Vector3& a, float scalar);
    static float dot(const Vector3& a, const Vector3& b);
    static Vector3 cross(const Vector3& a, const Vector3& b);
    static Vector3 normalize(const Vector3& a);
    static float length(const Vector3& a);
    static float lengthSquared(const Vector3& a);
    
    // 快速近似版本
    static Vector3 fastNormalize(const Vector3& a);
    static float fastLength(const Vector3& a);
};

// 2维向量
struct ALIGNED(8) Vector2 {
    float x, y;
    
    Vector2() : x(0), y(0) {}
    Vector2(float x, float y) : x(x), y(y) {}
    
    static Vector2 add(const Vector2& a, const Vector2& b);
    static Vector2 subtract(const Vector2& a, const Vector2& b);
    static Vector2 multiply(const Vector2& a, const Vector2& b);
    static Vector2 scale(const Vector2& a, float scalar);
    static float dot(const Vector2& a, const Vector2& b);
    static float length(const Vector2& a);
    static float lengthSquared(const Vector2& a);
    static Vector2 normalize(const Vector2& a);
};

// 批量向量操作 - 针对缓存优化
class BatchVectorOperations {
public:
    // 批量向量加法
    static void addVectors4(const Vector4* a, const Vector4* b, Vector4* result, size_t count);
    static void addVectors3(const Vector3* a, const Vector3* b, Vector3* result, size_t count);
    
    // 批量向量缩放
    static void scaleVectors4(const Vector4* vectors, float scalar, Vector4* result, size_t count);
    static void scaleVectors3(const Vector3* vectors, float scalar, Vector3* result, size_t count);
    
    // 批量点积计算
    static void dotProducts4(const Vector4* a, const Vector4* b, float* results, size_t count);
    static void dotProducts3(const Vector3* a, const Vector3* b, float* results, size_t count);
    
    // 批量归一化
    static void normalizeVectors4(Vector4* vectors, size_t count);
    static void normalizeVectors3(Vector3* vectors, size_t count);
    
    // 快速近似版本
    static void fastNormalizeVectors4(Vector4* vectors, size_t count);
    static void fastNormalizeVectors3(Vector3* vectors, size_t count);
};

} // namespace OptimizedMath
/* END math/VectorMath.h */

// OptimizedMG merged optimization headers

/* BEGIN math/MatrixMath.h */
#include "VectorMath.h"
#include <arm_neon.h>

namespace OptimizedMath {

// 4x4 矩阵 - 针对NEON和缓存优化
struct ALIGNED(16) Matrix4 {
    union {
        float m[4][4];
        float data[16];
        float32x4_t rows[4]; // NEON友好的行存储
    };
    
    Matrix4();
    Matrix4(const Matrix4& other);
    
    // 静态构造方法
    static Matrix4 identity();
    static Matrix4 zero();
    static Matrix4 translation(float x, float y, float z);
    static Matrix4 scale(float x, float y, float z);
    static Matrix4 rotationX(float angle);
    static Matrix4 rotationY(float angle);
    static Matrix4 rotationZ(float angle);
    static Matrix4 perspective(float fov, float aspect, float near, float far);
    static Matrix4 orthographic(float left, float right, float bottom, float top, float near, float far);
    
    // 矩阵运算
    static Matrix4 multiply(const Matrix4& a, const Matrix4& b);
    static Vector4 multiply(const Matrix4& m, const Vector4& v);
    static Matrix4 transpose(const Matrix4& m);
    static Matrix4 inverse(const Matrix4& m);
    static float determinant(const Matrix4& m);
    
    // 快速近似版本
    static Matrix4 fastInverse(const Matrix4& m);
    
    // 运算符重载
    Matrix4& operator=(const Matrix4& other);
    Matrix4 operator*(const Matrix4& other) const;
    Vector4 operator*(const Vector4& v) const;
};

// 批量矩阵操作
class BatchMatrixOperations {
public:
    // 批量矩阵乘法
    static void multiplyMatrices(const Matrix4* a, const Matrix4* b, Matrix4* result, size_t count);
    
    // 批量矩阵-向量乘法
    static void transformVectors4(const Matrix4* matrices, const Vector4* vectors, Vector4* result, size_t count);
    static void transformVectors3(const Matrix4* matrices, const Vector3* vectors, Vector3* result, size_t count);
    
    // 批量矩阵转置
    static void transposeMatrices(Matrix4* matrices, size_t count);
    
    // 批量透视矩阵生成
    static void generatePerspectiveMatrices(float fov, float aspect, float near, float far, 
                                          Matrix4* matrices, size_t count);
};

} // namespace OptimizedMath
/* END math/MatrixMath.h */
