#include "nau/mgpu/Utils/pch.h"
#include "nau/mgpu/Utils/MathHelper.h"
#include <float.h>
#include <cmath>

namespace nau::PEPEngine::Utils
{
    using namespace DirectX;

    const float MathHelper::Infinity = FLT_MAX;
    const float MathHelper::Pi = 3.1415926535f;

    float MathHelper::AngleFromXY(const float x, const float y)
    {
        float theta = 0.0f;

        // Quadrant I or IV
        if (x >= 0.0f)
        {
            // If x = 0, then atanf(y/x) = +pi/2 if y > 0
            //                atanf(y/x) = -pi/2 if y < 0
            theta = atanf(y / x); // in [-pi/2, +pi/2]

            if (theta < 0.0f)
                theta += 2.0f * Pi; // in [0, 2*pi).
        }

        // Quadrant II or III
        else
            theta = atanf(y / x) + Pi; // in [0, 2*pi).

        return theta;
    }

    XMVECTOR MathHelper::RandUnitVec3()
    {
        XMVECTOR One = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
        XMVECTOR Zero = XMVectorZero();

        // Keep trying until we get a point on/in the hemisphere.
        while (true)
        {
            // Generate random point in the cube [-1,1]^3.
            XMVECTOR v = XMVectorSet(RandF(-1.0f, 1.0f), RandF(-1.0f, 1.0f), RandF(-1.0f, 1.0f), 0.0f);

            // Ignore points outside the unit sphere in order to get an even distribution 
            // over the unit sphere.  Otherwise points will clump more on the sphere near 
            // the corners of the cube.

            if (XMVector3Greater(XMVector3LengthSq(v), One))
                continue;

            return XMVector3Normalize(v);
        }
    }

    XMVECTOR MathHelper::RandHemisphereUnitVec3(const XMVECTOR n)
    {
        XMVECTOR One = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
        XMVECTOR Zero = XMVectorZero();

        // Keep trying until we get a point on/in the hemisphere.
        while (true)
        {
            // Generate random point in the cube [-1,1]^3.
            XMVECTOR v = XMVectorSet(RandF(-1.0f, 1.0f), RandF(-1.0f, 1.0f), RandF(-1.0f, 1.0f), 0.0f);

            // Ignore points outside the unit sphere in order to get an even distribution 
            // over the unit sphere.  Otherwise points will clump more on the sphere near 
            // the corners of the cube.

            if (XMVector3Greater(XMVector3LengthSq(v), One))
                continue;

            // Ignore points in the bottom hemisphere.
            if (XMVector3Less(XMVector3Dot(n, v), Zero))
                continue;

            return XMVector3Normalize(v);
        }
    }

    SimpleMath::Vector3 MathHelper::ToEulerAngles(const SimpleMath::Quaternion q)
    {
        SimpleMath::Vector3 angles;

        // roll (x-axis rotation)
        double sinr_cosp = 2 * (q.w * q.x + q.y * q.z);
        double cosr_cosp = 1 - 2 * (q.x * q.x + q.y * q.y);
        angles.x = static_cast<float>(std::atan2(sinr_cosp, cosr_cosp));

        // pitch (y-axis rotation)
        double sinp = 2 * (q.w * q.y - q.z * q.x);
        if (std::abs(sinp) >= 1)
            angles.y = static_cast<float>(std::copysign(XM_PI / 2, sinp)); // use 90 degrees if out of range
        else
            angles.y = static_cast<float>(std::asin(sinp));

        // yaw (z-axis rotation)
        const double siny_cosp = 2 * (q.w * q.z + q.x * q.y);
        const double cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
        angles.z = static_cast<float>(std::atan2(siny_cosp, cosy_cosp));

        return angles;
    }

    SimpleMath::Quaternion MathHelper::ToQuaternion(const double xRoll, const double yPitch,
                                                     const double zYaw)
    {
        return SimpleMath::Quaternion::CreateFromYawPitchRoll(static_cast<float>(zYaw),
                                                               static_cast<float>(yPitch),
                                                               static_cast<float>(xRoll));
    }
}
