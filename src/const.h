#pragma once

#include <cmath>
#include <qgenericmatrix.h>

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