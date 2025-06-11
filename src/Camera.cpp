#include "Camera.h"

#include <cassert>
#include <limits>

void Camera::setOrthographciProjection(
    float left, float right, float top, float bottom, float near, float far)
{
    m_projectionMartrix.setToIdentity();

    m_projectionMartrix(0, 0) = 2.0f / (right - left);
    m_projectionMartrix(1, 1) = 2.0f / (bottom - top);
    m_projectionMartrix(2, 2) = 1.0f / (far - near);
    m_projectionMartrix(3, 0) = -(right + left) / (right - left);
    m_projectionMartrix(3, 1) = -(bottom + top) / (bottom - top);
    m_projectionMartrix(3, 2) = -near / (far - near);

}

void Camera::setPrespectiveProjection(float fovy, float aspect, float near, float far)
{
    assert(std::abs(aspect - std::numeric_limits<float>::epsilon()) > 0.0f);
    const float tanHalfFovy = tan(fovy / 2.f);
    m_projectionMartrix.fill(0.f);

    m_projectionMartrix(0, 0) = 1.f / (aspect * tanHalfFovy);
    m_projectionMartrix(1, 1) = 1.f / tanHalfFovy;
    m_projectionMartrix(2, 2) = far / (far - near);
    m_projectionMartrix(2, 3) = -(far * near) / (far - near);
    m_projectionMartrix(3, 2) = 1.f;

}

void Camera::setViewDirection(QVector3D position, QVector3D direction, QVector3D up)
{
    QVector3D w = direction.normalized();
    QVector3D u = QVector3D::crossProduct(w, up).normalized();
    QVector3D v = QVector3D::crossProduct(w, u);

    // 2) 先重置为单位矩阵
    m_viewMatrix.setToIdentity();

    // 3) 把旋转部分写入（列 0 = u，列 1 = v，列 2 = w）
    m_viewMatrix(0, 0) = u.x();  m_viewMatrix(0, 1) = u.y();  m_viewMatrix(0, 2) = u.z();
    m_viewMatrix(1, 0) = v.x();  m_viewMatrix(1, 1) = v.y();  m_viewMatrix(1, 2) = v.z();
    m_viewMatrix(2, 0) = w.x();  m_viewMatrix(2, 1) = w.y();  m_viewMatrix(2, 2) = w.z();

    // 4) 再把平移部分写入（-dot(u,position), -dot(v,position), -dot(w,position)）
    m_viewMatrix(0, 3) = -QVector3D::dotProduct(u, position);
    m_viewMatrix(1, 3) = -QVector3D::dotProduct(v, position);
    m_viewMatrix(2, 3) = -QVector3D::dotProduct(w, position);
}

void Camera::setViewTarget(QVector3D position, QVector3D target, QVector3D up)
{
    setViewDirection(position, target - position, up);
    //m_viewMatrix.setToIdentity();
    //m_viewMatrix.lookAt(
    //    position,   // eye
    //    target - position,     // center
    //    up
    //);
}

void Camera::setViewLocation(QVector3D position, QVector3D rotation)
{

    float c3 = std::cos(rotation.z());
    float s3 = std::sin(rotation.z());
    float c2 = std::cos(rotation.x());
    float s2 = std::sin(rotation.x());
    float c1 = std::cos(rotation.y());
    float s1 = std::sin(rotation.y());

    // 2) 按照 Y1->X2->Z3 顺序，计算 camera 空间的三根正交基向量 u=right, v=up, w=forward
    QVector3D u(
        (c1 * c3 + s1 * s2 * s3),
        (c2 * s3),
        (c1 * s2 * s3 - c3 * s1)
    );
    QVector3D v(
        (c3 * s1 * s2 - c1 * s3),
        (c2 * c3),
        (c1 * c3 * s2 + s1 * s3)
    );
    QVector3D w(
        (c2 * s1),
        (-s2),
        (c1 * c2)
    );

    // 3) 把它们填到 View 矩阵里（列主序，(row,col)）
    m_viewMatrix.setToIdentity();
    m_viewMatrix(0, 0) = u.x();  m_viewMatrix(0, 1) = u.y();  m_viewMatrix(0, 2) = u.z();
    m_viewMatrix(1, 0) = v.x();  m_viewMatrix(1, 1) = v.y();  m_viewMatrix(1, 2) = v.z();
    m_viewMatrix(2, 0) = w.x();  m_viewMatrix(2, 1) = w.y();  m_viewMatrix(2, 2) = w.z();

    // 4) 平移部分 = -dot(基向量, position)
    m_viewMatrix(0, 3) = -QVector3D::dotProduct(u, position);
    m_viewMatrix(1, 3) = -QVector3D::dotProduct(v, position);
    m_viewMatrix(2, 3) = -QVector3D::dotProduct(w, position);

}

AABB Camera::getViewFrustum() const
{
    Camera::Frustum2D frustum = getFrustum2D();

    // 视锥体平面：left, right, bottom, top
    const auto& planes = frustum.planes;

    // 计算视锥体边界的交点来确定可见区域
    // 平面方程：ax + by + c = 0

    // 找到四个角点的交点
    std::vector<QVector2D> intersectionPoints;

    // 计算相邻平面的交点
    for (int i = 0; i < 4; ++i) {
        int next = (i + 1) % 4;
        QVector2D intersection = calculatePlaneIntersection(planes[i], planes[next]);

        // 检查交点是否有效（不是无穷远）
        if (std::isfinite(intersection.x()) && std::isfinite(intersection.y())) {
            intersectionPoints.push_back(intersection);
        }
    }

    // 如果没有找到有效交点，使用一个大的默认区域
    if (intersectionPoints.empty()) {
        return AABB{ -10000.0f, -10000.0f, 10000.0f, 10000.0f };
    }

    // 计算包围所有交点的AABB
    float minX = intersectionPoints[0].x();
    float maxX = intersectionPoints[0].x();
    float minY = intersectionPoints[0].y();
    float maxY = intersectionPoints[0].y();

    for (const auto& point : intersectionPoints) {
        minX = std::min(minX, point.x());
        maxX = std::max(maxX, point.x());
        minY = std::min(minY, point.y());
        maxY = std::max(maxY, point.y());
    }

    // 扩展边界以确保覆盖整个视锥体
    float padding = 100.0f;  // 可以根据需要调整
    return AABB{
        minX - padding,
        minY - padding,
        maxX + padding,
        maxY + padding
    };
}

Camera::Frustum2D Camera::getFrustum2D() const
{
    QMatrix4x4 M = m_projectionMartrix * m_viewMatrix;
    Frustum2D f;
    auto extractPlane = [&](int rowA, int rowB, bool plus, Plane2D& pl)
        {
            // plane = M[3] ± M[row]
            float a = M(rowA, 0) * (plus ? 1 : -1) + M(3, 0);
            float b = M(rowA, 1) * (plus ? 1 : -1) + M(3, 1);
            float c = M(rowA, 3) * (plus ? 1 : -1) + M(3, 3);
            float invLen = 1.0f / qSqrt(a * a + b * b);
            pl.a = a * invLen;
            pl.b = b * invLen;
            pl.c = c * invLen;
        };

    // 左平面:  M[3] + M[0]
    extractPlane(0, 3, true, f.planes[0]);
    // 右平面:  M[3] - M[0]
    extractPlane(0, 3, false, f.planes[1]);
    // 底平面:  M[3] + M[1]
    extractPlane(1, 3, true, f.planes[2]);
    // 顶平面:  M[3] - M[1]
    extractPlane(1, 3, false, f.planes[3]);

    return f;
}

QVector3D Camera::getPosition() const
{
    QMatrix4x4 invView = m_viewMatrix.inverted();
    return QVector3D(invView(0, 3), invView(1, 3), invView(2, 3));
}
