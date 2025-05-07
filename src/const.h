#pragma once
#include <cmath>
#include <qgenericmatrix.h>

#include <qvector2d.h>
#include <qvector3d.h>

using Mat2F = QGenericMatrix<2, 2, float>;

constexpr float IdentityMmatrix2D[4] = { 1.0f, 0.0f, 0.0f, 1.0f };

struct SimplePushConstantData
{
    Mat2F   transform;
    QVector2D offset;
    alignas(16) QVector3D color;
};