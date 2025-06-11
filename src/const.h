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

    // �����������Ƿ���AABB��
    bool contains(const QVector2D& point) const
    {
        return point.x() >= minX && point.x() <= maxX &&
            point.y() >= minY && point.y() <= maxY;
    }

    bool contains(float x, float y) const
    {
        return x >= minX && x <= maxX && y >= minY && y <= maxY;
    }

    // �����������һ��AABB�Ƿ���ȫ�ڴ�AABB��
    bool contains(const AABB& other) const
    {
        return other.minX >= minX && other.maxX <= maxX &&
            other.minY >= minY && other.maxY <= maxY;
    }

    // �����������ཻ����
    AABB intersection(const AABB& other) const
    {
        if (!overlaps(other)) {
            return AABB(); // ���ؿյ�AABB
        }

        return AABB(
            qMax(minX, other.minX),
            qMax(minY, other.minY),
            qMin(maxX, other.maxX),
            qMin(maxY, other.maxY)
        );
    }

    // ���������㲢��
    AABB unionWith(const AABB& other) const
    {
        return AABB(
            qMin(minX, other.minX),
            qMin(minY, other.minY),
            qMax(maxX, other.maxX),
            qMax(maxY, other.maxY)
        );
    }

    // ��������չAABB�԰���һ����
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

    // ��������չAABB�԰�����һ��AABB
    void expand(const AABB& other)
    {
        minX = qMin(minX, other.minX);
        minY = qMin(minY, other.minY);
        maxX = qMax(maxX, other.maxX);
        maxY = qMax(maxY, other.maxY);
    }

    // ��������ָ��ֵ��չ�߽�
    AABB expanded(float margin) const
    {
        return AABB(minX - margin, minY - margin, maxX + margin, maxY + margin);
    }

    // ��������ȡ���ĵ�
    QVector2D center() const
    {
        return QVector2D((minX + maxX) * 0.5f, (minY + maxY) * 0.5f);
    }

    // ��������ȡ�ߴ�
    QVector2D size() const
    {
        return QVector2D(maxX - minX, maxY - minY);
    }

    // ��������ȡ���
    float area() const
    {
        return (maxX - minX) * (maxY - minY);
    }

    // ���������AABB�Ƿ���Ч�����Ƿ���ģ�
    bool isValid() const
    {
        return minX <= maxX && minY <= maxY;
    }

    // ���������AABB�Ƿ�Ϊ��
    bool isEmpty() const
    {
        return minX >= maxX || minY >= maxY;
    }

    // ����������Ϊ��AABB
    void clear()
    {
        minX = minY = maxX = maxY = 0.0f;
    }

    // ���������㵽��ľ��루�������AABB�ڷ���0��
    float distanceToPoint(const QVector2D& point) const
    {
        float dx = qMax(0.0f, qMax(minX - point.x(), point.x() - maxX));
        float dy = qMax(0.0f, qMax(minY - point.y(), point.y() - maxY));
        return std::sqrt(dx * dx + dy * dy);
    }

    // ����������������
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
    alignas(16) float transform[16];  // 4x4����16�ֽڶ���
    alignas(16) float color[3];       // RGB��ɫ
    float padding;                    // ��䵽16�ֽڶ���
};

constexpr int CONTOUR_SEGMENT_THRESHOLD = 200;  // ����200����ͷֶ�
constexpr int CONTOUR_SEGMENT_SIZE = 150;       // ÿ��150����
constexpr int CONTOUR_OVERLAP_POINTS = 2;       // �μ��ص�2���㱣֤������