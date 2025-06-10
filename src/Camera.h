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

        AABB getBounds() const
        {
            float maxRange = 10000.0f; // 默认搜索范围
            float left = -maxRange, right = maxRange;
            float bottom = -maxRange, top = maxRange;

            // 通过平面求交计算实际边界
            std::vector<QVector2D> intersectionPoints;

            // 计算相邻平面的交点
            for (int i = 0; i < 4; ++i) {
                int next = (i + 1) % 4;
                const auto& p1 = planes[i];
                const auto& p2 = planes[next];

                // 求两平面交线与2D的交点: p1.a*x + p1.b*y + p1.c = 0
                //                        p2.a*x + p2.b*y + p2.c = 0
                float det = p1.a * p2.b - p1.b * p2.a;
                if (std::abs(det) > 0.001f) {
                    float x = (p1.b * p2.c - p1.c * p2.b) / det;
                    float y = (p1.c * p2.a - p1.a * p2.c) / det;

                    // 验证交点是否在所有平面的正确一侧
                    bool valid = true;
                    for (const auto& plane : planes) {
                        if (plane.a * x + plane.b * y + plane.c <= 0) {
                            valid = false;
                            break;
                        }
                    }

                    if (valid) {
                        intersectionPoints.push_back(QVector2D(x, y));
                    }
                }
            }

            // 如果找到了有效交点，计算边界
            if (!intersectionPoints.empty()) {
                left = right = intersectionPoints[0].x();
                bottom = top = intersectionPoints[0].y();

                for (const auto& point : intersectionPoints) {
                    left = qMin(left, point.x());
                    right = qMax(right, point.x());
                    bottom = qMin(bottom, point.y());
                    top = qMax(top, point.y());
                }

                // 添加小边距以避免边界情况
                float margin = 10.0f;
                left -= margin;
                right += margin;
                bottom -= margin;
                top += margin;
            }

            // 确保边界有效且合理
            if (left >= right || bottom >= top ||
                (right - left) > maxRange || (top - bottom) > maxRange) {
                // 回退到保守估计
                left = -maxRange * 0.1f;
                right = maxRange * 0.1f;
                bottom = -maxRange * 0.1f;
                top = maxRange * 0.1f;
            }

            return AABB(left, bottom, right, top);
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
    AABB getFrustumBound2D() const;


    QMatrix4x4 getProjection() const { return m_projectionMartrix; }
    QMatrix4x4 getView() const { return m_viewMatrix; }

private:
    QMatrix4x4 m_projectionMartrix;
    QMatrix4x4 m_viewMatrix;
};

