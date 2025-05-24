#pragma once

#include "const.h"

class Camera
{
public:

    struct Plane2D
    {
        float a{}, b{}, c{};
        bool contains(const QVector2D& p) const
        {
            return a * p.x() + b * p.y() + c > 0.f;
        }
    };

    struct Plane3D
    {
        float a{}, b{}, c{}, d{};
        bool contains(const QVector3D& p) const
        {
            return a * p.x() + b * p.y() + c * p.z() + d > 0.f;
        }
    };

    class Frustum3D
    {
    public:
        // 6个平面：near, far, left, right, top, bottom
        std::array<Plane3D, 6> planes;

        bool intersects(const QVector3D& mn, const QVector3D& mx) const
        {
            for (const auto& pl : planes)
            {
                QVector3D p(
                    pl.a >= 0 ? mx.x() : mn.x(),
                    pl.b >= 0 ? mx.y() : mn.y(),
                    pl.c >= 0 ? mx.z() : mn.z()
                );

                if (!pl.contains(p))
                    return false;
            }
            return true;
        }
    };

    class Frustum2D
    {
    public:
        //left, right, bottom, top
        std::array<Plane2D, 4> planes;

        bool insersects(const QVector2D& mn, const QVector2D& mx) const
        {
            // 使用分离轴定理进行快速AABB测试
            for (const auto& pl : planes)
            {
                // 获取AABB的支持点（最远点）
                QVector2D p(
                    pl.a >= 0 ? mx.x() : mn.x(),
                    pl.b >= 0 ? mx.y() : mn.y()
                );

                // 如果支持点在平面外部，则AABB完全在平面外
                if (!pl.contains(p))
                    return false;
            }
            return true;
        }
    };


    void setOrthographciProjection(
        float left, float rigth, float top, float bottom, float near, float far
    );


    void setPrespectiveProjection(
        float fovy, float aspect, float near, float far
    );

    void setViewDirection(QVector3D position, QVector3D direction, QVector3D up = QVector3D(0.f, -1.f, 0.f));
    void setViewTarget(QVector3D position, QVector3D target, QVector3D up = QVector3D(0.f, -1.f, 0.f));
    void setViewLocation(QVector3D position, QVector3D rotation);

    Frustum2D getFrustum2D() const;
    Frustum3D getFrustum3D() const;

    QMatrix4x4 getProjection() const { return m_projectionMartrix; }
    QMatrix4x4 getView() const { return m_viewMatrix; }

private:
    QMatrix4x4 m_projectionMartrix;
    QMatrix4x4 m_viewMatrix;
};

