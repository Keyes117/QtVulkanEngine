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
    //m_projectionMartrix(2, 3) = 1.f;
    //m_projectionMartrix(3, 2) = -(far * near) / (far - near);
}

void Camera::setViewDirection(QVector3D position, QVector3D direction, QVector3D up)
{
    QVector3D w = direction.normalized();
    QVector3D u = QVector3D::crossProduct(w, up).normalized();
    QVector3D v = QVector3D::crossProduct(w, u);

    // 2) 先重置为单位矩阵
    m_viewMatrix.setToIdentity();

    // 3) 把旋转部分写入（列 0 = u，列 1 = v，列 2 = w）
    m_viewMatrix(0, 0) = u.x();  m_viewMatrix(0, 1) = v.x();  m_viewMatrix(0, 2) = -w.x();
    m_viewMatrix(1, 0) = u.y();  m_viewMatrix(1, 1) = v.y();  m_viewMatrix(1, 2) = -w.y();
    m_viewMatrix(2, 0) = u.z();  m_viewMatrix(2, 1) = v.z();  m_viewMatrix(2, 2) = -w.z();

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
        /* x */  c1 * c3 + s1 * s2 * s3,
        /* y */  c2 * s3,
        /* z */  c1 * s2 * s3 - s1 * c3
    );
    QVector3D v(
        /* x */  s1 * c2,
        /* y */ -s2,
        /* z */  c1 * c2
    );
    QVector3D w(
        /* x */  s1 * s3 - c1 * s2 * c3,
        /* y */  c2 * c3,
        /* z */  c1 * c3 + s1 * s2 * s3
    );

    // 3) 把它们填到 View 矩阵里（列主序，(row,col)）
    m_viewMatrix.setToIdentity();
    m_viewMatrix(0, 0) = u.x();  m_viewMatrix(0, 1) = v.x();  m_viewMatrix(0, 2) = w.x();
    m_viewMatrix(1, 0) = u.y();  m_viewMatrix(1, 1) = v.y();  m_viewMatrix(1, 2) = w.y();
    m_viewMatrix(2, 0) = u.z();  m_viewMatrix(2, 1) = v.z();  m_viewMatrix(2, 2) = w.z();

    // 4) 平移部分 = -dot(基向量, position)
    m_viewMatrix(3, 0) = -QVector3D::dotProduct(u, position);
    m_viewMatrix(3, 1) = -QVector3D::dotProduct(v, position);
    m_viewMatrix(3, 2) = -QVector3D::dotProduct(w, position);


    //m_viewMatrix.setToIdentity();

    //// 1) 先按欧拉角的逆方向旋转
    ////    假设 rotation.x = 绕 X 轴的角度（度），
    ////             rotation.y = 绕 Y 轴的角度，
    ////             rotation.z = 绕 Z 轴的角度。
    //m_viewMatrix.rotate(-rotation.x(), 1, 0, 0);
    //m_viewMatrix.rotate(-rotation.y(), 0, 1, 0);
    //m_viewMatrix.rotate(-rotation.z(), 0, 0, 1);

    //// 2) 再平移到原点（逆向平移）
    //m_viewMatrix.translate(-position);
}
