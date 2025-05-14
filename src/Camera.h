#pragma once

#include "const.h"

class Camera
{
public:

    void setOrthographciProjection(
        float left, float rigth, float top, float bottom, float near, float far
    );


    void setPrespectiveProjection(
        float fovy, float aspect, float near, float far
    );

    void setViewDirection(QVector3D position, QVector3D direction, QVector3D up = QVector3D(0.f, -1.f, 0.f));
    void setViewTarget(QVector3D position, QVector3D target, QVector3D up = QVector3D(0.f, -1.f, 0.f));
    void setViewLocation(QVector3D position, QVector3D rotation);

    QMatrix4x4 getProjection() const { return m_projectionMartrix; }
    QMatrix4x4 getView() const { return m_viewMatrix; }

private:
    QMatrix4x4 m_projectionMartrix;
    QMatrix4x4 m_viewMatrix;
};

