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

    bool overlaps(const AABB& other)
    {
        return !(maxX < other.minX || minX > other.maxX ||
            maxY < other.minY || minY > other.maxY);
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