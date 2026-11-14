#pragma once

#include <Windows.h>
#include <DirectXMath.h>
#include <cstdint>
#include "d3d12.h"
#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

namespace nau::PEPEngine::Utils
{
    class MathHelper
    {
    public:
        // Returns random float in [0, 1).
        static float RandF()
        {
            return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        }

        // Returns random float in [a, b).
        static float RandF(const float a, const float b)
        {
            return a + RandF() * (b - a);
        }

        static int Rand(const int a, const int b)
        {
            return a + rand() % ((b - a) + 1);
        }

        template <typename T>
        static T Min(const T& a, const T& b)
        {
            return a < b ? a : b;
        }

        template <typename T>
        static T Max(const T& a, const T& b)
        {
            return a > b ? a : b;
        }

        template <typename T>
        static T Lerp(const T& a, const T& b, float t)
        {
            return a + (b - a) * t;
        }

        template <typename T>
        static T Clamp(const T& x, const T& low, const T& high)
        {
            return x < low ? low : (x > high ? high : x);
        }

        // Returns the polar angle of the point (x,y) in [0, 2*PI).
        static float AngleFromXY(float x, float y);

        static DirectX::XMVECTOR SphericalToCartesian(const float radius, const float theta, const float phi)
        {
            return DirectX::XMVectorSet(
                radius * sinf(phi) * cosf(theta),
                radius * cosf(phi),
                radius * sinf(phi) * sinf(theta),
                1.0f);
        }

        static DirectX::XMMATRIX InverseTranspose(DirectX::CXMMATRIX M)
        {
            // Inverse-transpose is just applied to normals.  So zero out 
            // translation row so that it doesn't get into our inverse-transpose
            // calculation--we don't want the inverse-transpose of the translation.
            DirectX::XMMATRIX A = M;
            A.r[3] = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

            DirectX::XMVECTOR det = XMMatrixDeterminant(A);
            return XMMatrixTranspose(XMMatrixInverse(&det, A));
        }

        static DirectX::XMFLOAT4X4 Identity4x4()
        {
            static DirectX::XMFLOAT4X4 I(
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f);

            return I;
        }

        static DirectX::XMVECTOR RandUnitVec3();
        static DirectX::XMVECTOR RandHemisphereUnitVec3(DirectX::XMVECTOR n);

        static const float Infinity;
        static const float Pi;

        static DirectX::SimpleMath::Vector3 ToEulerAngles(Quaternion q);


        // yaw (Z), pitch (Y), roll (X)
        static DirectX::SimpleMath::Quaternion ToQuaternion(double xRoll, double yPitch, double zYaw);
    };
}
