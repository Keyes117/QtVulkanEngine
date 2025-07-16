#pragma once

#include <cmath>

#include <qmatrix4x4.h>
#include <qvector2d.h>
#include <qvector3d.h>


using Mat2F = QGenericMatrix<2, 2, float>;

constexpr float IdentityMmatrix2D[4] = { 1.0f, 0.0f, 0.0f, 1.0f };

struct SimplePushConstantData
{
    QMatrix4x4   modelMatrix;
    alignas(16) QVector3D color;
};

struct AABB
{
    float minX, minY;
    float maxX, maxY;

    AABB() :minX(0), minY(0), maxX(0), maxY(0) {}
    AABB(float minx, float miny, float maxx, float maxy)
        :minX(minx), minY(miny), maxX(maxx), maxY(maxy) {}

    bool overlaps(const AABB& other) const
    {
        return !(maxX < other.minX || minX > other.maxX ||
            maxY < other.minY || minY > other.maxY);
    }

    // 新增：检查点是否在AABB内
    bool contains(const QVector2D& point) const
    {
        return point.x() >= minX && point.x() <= maxX &&
            point.y() >= minY && point.y() <= maxY;
    }

    bool contains(float x, float y) const
    {
        return x >= minX && x <= maxX && y >= minY && y <= maxY;
    }

    // 新增：检查另一个AABB是否完全在此AABB内
    bool contains(const AABB& other) const
    {
        return other.minX >= minX && other.maxX <= maxX &&
            other.minY >= minY && other.maxY <= maxY;
    }

    // 新增：计算相交区域
    AABB intersection(const AABB& other) const
    {
        if (!overlaps(other)) {
            return AABB(); // 返回空的AABB
        }

        return AABB(
            qMax(minX, other.minX),
            qMax(minY, other.minY),
            qMin(maxX, other.maxX),
            qMin(maxY, other.maxY)
        );
    }

    // 新增：计算并集
    AABB unionWith(const AABB& other) const
    {
        return AABB(
            qMin(minX, other.minX),
            qMin(minY, other.minY),
            qMax(maxX, other.maxX),
            qMax(maxY, other.maxY)
        );
    }

    // 新增：扩展AABB以包含一个点
    void expand(const QVector2D& point)
    {
        minX = qMin(minX, point.x());
        minY = qMin(minY, point.y());
        maxX = qMax(maxX, point.x());
        maxY = qMax(maxY, point.y());
    }

    void expand(float x, float y)
    {
        minX = qMin(minX, x);
        minY = qMin(minY, y);
        maxX = qMax(maxX, x);
        maxY = qMax(maxY, y);
    }

    // 新增：扩展AABB以包含另一个AABB
    void expand(const AABB& other)
    {
        minX = qMin(minX, other.minX);
        minY = qMin(minY, other.minY);
        maxX = qMax(maxX, other.maxX);
        maxY = qMax(maxY, other.maxY);
    }

    // 新增：按指定值扩展边界
    AABB expanded(float margin) const
    {
        return AABB(minX - margin, minY - margin, maxX + margin, maxY + margin);
    }

    // 新增：获取中心点
    QVector2D center() const
    {
        return QVector2D((minX + maxX) * 0.5f, (minY + maxY) * 0.5f);
    }

    // 新增：获取尺寸
    QVector2D size() const
    {
        return QVector2D(maxX - minX, maxY - minY);
    }

    // 新增：获取面积
    float area() const
    {
        return (maxX - minX) * (maxY - minY);
    }

    // 新增：检查AABB是否有效（不是反向的）
    bool isValid() const
    {
        return minX <= maxX && minY <= maxY;
    }

    // 新增：检查AABB是否为空
    bool isEmpty() const
    {
        return minX >= maxX || minY >= maxY;
    }

    // 新增：重置为空AABB
    void clear()
    {
        minX = minY = maxX = maxY = 0.0f;
    }

    // 新增：计算到点的距离（如果点在AABB内返回0）
    float distanceToPoint(const QVector2D& point) const
    {
        float dx = qMax(0.0f, qMax(minX - point.x(), point.x() - maxX));
        float dy = qMax(0.0f, qMax(minY - point.y(), point.y() - maxY));
        return std::sqrt(dx * dx + dy * dy);
    }

    // 新增：操作符重载
    bool operator==(const AABB& other) const
    {
        return minX == other.minX && minY == other.minY &&
            maxX == other.maxX && maxY == other.maxY;
    }

    bool operator!=(const AABB& other) const
    {
        return !(*this == other);
    }
};

struct InstanceData
{
    alignas(16) float transform[16];  // 4x4矩阵，16字节对齐
    alignas(16) float color[3];       // RGB颜色
    float padding;                    // 填充到16字节对齐
};

constexpr int CONTOUR_SEGMENT_THRESHOLD = 200;  // 超过200个点就分段
constexpr int CONTOUR_SEGMENT_SIZE = 150;       // 每段150个点
constexpr int CONTOUR_OVERLAP_POINTS = 2;       // 段间重叠2个点保证连续性

/* ===========================GPU-CPU通信结构============================ */

// 常量定义
constexpr uint32_t MAX_GPU_REQUESTS = 256;
constexpr uint32_t MAX_CPU_RESPONSES = 256;

// 枚举定义
enum class RequestType : uint32_t
{
    REQUEST_LOAD_GEOMETRY = 0,
    REQUEST_UNLOAD_GEOMETRY = 1,
    REQUEST_CHANGE_LOD = 2,
    REQUEST_UPDATE_PRIORITY = 3
};

enum class RequestPriority : uint32_t
{
    URGENT = 0,        // 紧急：视野中心
    HIGH = 1,          // 高：视野边缘
    NORMAL = 2,        // 普通：预加载区域
    BACKGROUND = 3     // 后台：远距离区域
};

enum class ResponseStatus : uint32_t
{
    PENDING = 0,       // 处理中
    SUCCESS = 1,       // 成功
    FAILED = 2,        // 失败
    TIMEOUT = 3        // 超时
};

// GPU请求结构（32字节对齐）
struct GPURequest
{
    uint32_t type;           // RequestType枚举值
    uint32_t objectID;       // 对象ID
    uint32_t regionHash;     // 区域哈希
    uint32_t priority;       // RequestPriority枚举值
    uint32_t frameID;        // 帧ID
    uint32_t checkSum;       // 校验和
    uint32_t timestamp;      // 时间戳
    uint32_t padding;        // 对齐到32字节

    void calculateCheckSum()
    {
        checkSum = type + objectID + frameID;
    }

    bool isValid() const
    {
        return checkSum == (type + objectID + frameID);
    }
};

// CPU响应结构（32字节对齐）
struct CPUResponse
{
    uint32_t requestID;      // 请求ID
    uint32_t status;         // ResponseStatus枚举值
    uint32_t dataOffset;     // 数据偏移
    uint32_t dataSize;       // 数据大小
    uint32_t timestamp;      // 时间戳
    uint32_t frameCompleted; // 完成帧号
    uint32_t padding[2];     // 对齐到32字节
};

// 统一计数器结构（16字节对齐）
struct RequestCounters
{
    uint32_t requestCount;   // GPU写入的请求数量
    uint32_t responseCount;  // CPU写入的响应数量
    uint32_t frameNumber;    // 当前帧号
    uint32_t padding;        // 对齐
};

// 缓冲区大小常量
constexpr uint32_t REQUEST_BUFFER_SIZE = sizeof(GPURequest) * MAX_GPU_REQUESTS;
constexpr uint32_t RESPONSE_BUFFER_SIZE = sizeof(CPUResponse) * MAX_CPU_RESPONSES;
constexpr uint32_t COUNTER_BUFFER_SIZE = sizeof(RequestCounters);