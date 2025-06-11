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

    class Frustum2D
    {
    public:
        //left, right, bottom, top
        std::array<Plane2D, 4 > planes;


        bool insersects(AABB aabb) const
        {
            QVector2D mn(aabb.minX, aabb.minY);
            QVector2D mx(aabb.maxX, aabb.maxY);
            return insersects(mn, mx);
        }

        bool insersects(const QVector2D& mn, const QVector2D& mx) const
        {
            for (auto& pl : planes)
            {
                QVector2D p(
                    pl.a >= 0 ? mx.x() : mn.x(),
                    pl.b >= 0 ? mx.y() : mn.y()
                );

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

    AABB getViewFrustum() const;
    Frustum2D getFrustum2D() const;
    QVector3D getPosition() const;

    QMatrix4x4 getProjection() const { return m_projectionMartrix; }
    QMatrix4x4 getView() const { return m_viewMatrix; }

    QVector2D calculatePlaneIntersection(const Camera::Plane2D& plane1, const Camera::Plane2D& plane2) const
    {
        // 两个平面方程：
        // plane1: a1*x + b1*y + c1 = 0
        // plane2: a2*x + b2*y + c2 = 0

        float determinant = plane1.a * plane2.b - plane2.a * plane1.b;

        // 如果行列式为0，平面平行，没有交点
        if (std::abs(determinant) < 1e-6f) {
            return QVector2D(std::numeric_limits<float>::infinity(),
                std::numeric_limits<float>::infinity());
        }

        // 求解线性方程组
        float x = (plane2.c * plane1.b - plane1.c * plane2.b) / determinant;
        float y = (plane1.c * plane2.a - plane2.c * plane1.a) / determinant;

        return QVector2D(x, y);
    }
private:
    QMatrix4x4 m_projectionMartrix;
    QMatrix4x4 m_viewMatrix;
};

