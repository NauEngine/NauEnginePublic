// Copyright 2024 N-GINN LLC. All rights reserved.
// Copyright (C) 2024  Gaijin Games KFT.  All rights reserved

#pragma once

#include <nau/math/math.h>

#define INLINE __forceinline

/// @addtogroup math
/// @{

namespace nau::math 
{
// BBox3 - 3D bounding box //
/**
  3D axis-aligned bounding box
  @sa BSphere3 TMatrix TMatrix4 Vector3 Point2 Point4
*/
class BSphere3;
class BBox3;
INLINE float non_empty_boxes_not_intersect(const BBox3 &a, const BBox3 &b);

class BBox3
{
public:
  /// Minimum (0) and maximum (1) limits for the box.
  Vector3 lim[2];

  INLINE BBox3() { setempty(); }
  INLINE BBox3(const Vector3 &min, const Vector3 &max)
  {
    lim[0] = min;
    lim[1] = max;
  }
  INLINE BBox3(Vector3 p, float s) { makecube(p, s); }
  INLINE BBox3(const BSphere3 &s);
  INLINE BBox3 &operator=(const BSphere3 &s);

  INLINE void setempty()
  {
    lim[0] = Vector3(FLT_MAX / 4, FLT_MAX / 4, FLT_MAX / 4);
    lim[1] = Vector3(FLT_MIN / 4, FLT_MIN / 4, FLT_MIN / 4);
  }
  INLINE bool isempty() const { return lim[0].getX() > lim[1].getX() || lim[0].getY() > lim[1].getY() || lim[0].getZ() > lim[1].getZ(); }
  INLINE void makecube(const Vector3 &p, float s)
  {
    Vector3 d(s / 2, s / 2, s / 2);
    lim[0] = p - d;
    lim[1] = p + d;
  }
  INLINE Vector3 center() const { return (lim[0] + lim[1]) * 0.5; }
  INLINE Vector3 width() const { return lim[1] - lim[0]; }

  INLINE const Vector3 &operator[](int i) const { return lim[i]; }
  INLINE Vector3 &operator[](int i) { return lim[i]; }
  INLINE operator const Vector3 *() const { return lim; }
  INLINE operator Vector3 *() { return lim; }

  INLINE const Vector3 &boxMin() const { return lim[0]; }
  INLINE Vector3 &boxMin() { return lim[0]; }
  INLINE const Vector3 &boxMax() const { return lim[1]; }
  INLINE Vector3 &boxMax() { return lim[1]; }

  INLINE bool operator==(const BBox3 &right) const { return lim[0] == right.lim[0] && lim[1] == right.lim[1]; }

  static constexpr unsigned int PointsCount = 8;
  INLINE Point3 point(unsigned int i) const { return Point3(lim[(i & 1)].getX(), lim[(i & 2) >> 1].getY(), lim[(i & 4) >> 2].getZ()); }

  INLINE bool operator!=(const BBox3 &right) const { return lim[0] != right.lim[0] || lim[1] != right.lim[1]; }

  INLINE float float_is_empty() const
  {
    return nau::math::fsel(lim[1].getX() - lim[0].getX(), 0.0f, 1.0f) + nau::math::fsel(lim[1].getY() - lim[0].getY(), 0.0f, 1.0f) + nau::math::fsel(lim[1].getZ() - lim[0].getZ(), 0.0f, 1.0f);
  }
  INLINE BBox3 &operator+=(const Vector3 &p)
  {
    lim[0].setX(nau::math::fsel(lim[0].getX() - p.getX(), p.getX(), lim[0].getX()));
    lim[1].setX(nau::math::fsel(p.getX() - lim[1].getX(), p.getX(), lim[1].getX()));
    lim[0].setY(nau::math::fsel(lim[0].getY() - p.getY(), p.getY(), lim[0].getY()));
    lim[1].setY(nau::math::fsel(p.getY() - lim[1].getY(), p.getY(), lim[1].getY()));
    lim[0].setZ(nau::math::fsel(lim[0].getZ() - p.getZ(), p.getZ(), lim[0].getZ()));
    lim[1].setZ(nau::math::fsel(p.getZ() - lim[1].getZ(), p.getZ(), lim[1].getZ()));
    return *this;
  }
  INLINE BBox3 &operator+=(const BBox3 &b)
  {
    if (b.isempty())
      return *this;
    lim[0].setX(nau::math::fsel(lim[0].getX() - b.lim[0].getX(), b.lim[0].getX(), lim[0].getX()));
    lim[1].setX(nau::math::fsel(b.lim[1].getX() - lim[1].getX(), b.lim[1].getX(), lim[1].getX()));
    lim[0].setY(nau::math::fsel(lim[0].getY() - b.lim[0].getY(), b.lim[0].getY(), lim[0].getY()));
    lim[1].setY(nau::math::fsel(b.lim[1].getY() - lim[1].getY(), b.lim[1].getY(), lim[1].getY()));
    lim[0].setZ(nau::math::fsel(lim[0].getZ() - b.lim[0].getZ(), b.lim[0].getZ(), lim[0].getZ()));
    lim[1].setZ(nau::math::fsel(b.lim[1].getZ() - lim[1].getZ(), b.lim[1].getZ(), lim[1].getZ()));
    return *this;
  }
  INLINE BBox3 &operator+=(const BSphere3 &s);

  /// check intersection with point
  INLINE bool operator&(const Vector3 &p) const
  {
    if (p.getX() < lim[0].getX())
      return 0;
    if (p.getX() > lim[1].getX())
      return 0;
    if (p.getY() < lim[0].getY())
      return 0;
    if (p.getY() > lim[1].getY())
      return 0;
    if (p.getZ() < lim[0].getZ())
      return 0;
    if (p.getZ() > lim[1].getZ())
      return 0;
    return 1;
  }

  /// check intersection with box
  INLINE bool operator&(const BBox3 &b) const
  {
    if (b.lim[0].getX() > b.lim[1].getX() || lim[0].getX() > lim[1].getX())
      return false;
    return non_empty_intersect(b);
  }
  INLINE bool non_empty_intersect(const BBox3 &b) const
  {
    if (b.lim[0].getX() > lim[1].getX())
      return false;
    if (b.lim[1].getX() < lim[0].getX())
      return false;
    if (b.lim[0].getY() > lim[1].getY())
      return false;
    if (b.lim[1].getY() < lim[0].getY())
      return false;
    if (b.lim[0].getZ() > lim[1].getZ())
      return false;
    if (b.lim[1].getZ() < lim[0].getZ())
      return false;
    return true;
  }
  INLINE void scale(float val)
  {
    const Vector3 c = center();
    lim[0] = (lim[0] - c) * val + c;
    lim[1] = (lim[1] - c) * val + c;
  }

  INLINE void inflate(float val)
  {
    lim[0] -= Vector3(val, val, val);
    lim[1] += Vector3(val, val, val);
  }

  INLINE void inflateXZ(float val)
  {
    lim[0] -= Vector3(val, 0, val);
    lim[1] += Vector3(val, 0, val);
  }

  INLINE BBox3 getIntersection(const BBox3 &right) const
  {
    if (!operator&(right))
      return BBox3();

    BBox3 result;
    result.lim[1][0] = eastl::min(lim[1][0], right.lim[1][0]);
    result.lim[0][0] = eastl::max(lim[0][0], right.lim[0][0]);
    result.lim[1][1] = eastl::min(lim[1][1], right.lim[1][1]);
    result.lim[0][1] = eastl::max(lim[0][1], right.lim[0][1]);
    result.lim[1][2] = eastl::min(lim[1][2], right.lim[1][2]);
    result.lim[0][2] = eastl::max(lim[0][2], right.lim[0][2]);
    return result;
  }

  static const BBox3 IDENT;
};

INLINE float non_empty_boxes_not_intersect(const BBox3 &a, const BBox3 &b)
{
  return nau::math::fsel(a.lim[1].getX() - b.lim[0].getX(), 0.0f, 1.0f) + nau::math::fsel(b.lim[1].getX() - a.lim[0].getX(), 0.0f, 1.0f) +
         nau::math::fsel(a.lim[1].getY() - b.lim[0].getY(), 0.0f, 1.0f) + nau::math::fsel(b.lim[1].getY() - a.lim[0].getY(), 0.0f, 1.0f) +
         nau::math::fsel(a.lim[1].getZ() - b.lim[0].getZ(), 0.0f, 1.0f) + nau::math::fsel(b.lim[1].getZ() - a.lim[0].getZ(), 0.0f, 1.0f);
}

INLINE float float_non_empty_boxes_not_inclusive(const BBox3 &inner, const BBox3 &outer)
{
  return nau::math::fsel(inner.lim[0].getX() - outer.lim[0].getX(), 0.0f, 1.0f) + nau::math::fsel(outer.lim[1].getX() - inner.lim[1].getX(), 0.0f, 1.0f) +
         nau::math::fsel(inner.lim[0].getY() - outer.lim[0].getY(), 0.0f, 1.0f) + nau::math::fsel(outer.lim[1].getY() - inner.lim[1].getY(), 0.0f, 1.0f) +
         nau::math::fsel(inner.lim[0].getZ() - outer.lim[0].getZ(), 0.0f, 1.0f) + nau::math::fsel(outer.lim[1].getZ() - inner.lim[1].getZ(), 0.0f, 1.0f);
};

INLINE bool non_empty_boxes_inclusive(const BBox3 &inner, const BBox3 &outer)
{
  return float_non_empty_boxes_not_inclusive(inner, outer) < 1.0f;
};


// BSphere3 - 3D bounding sphere (uses only fast routines) //
/**
  3D bounding sphere (uses only fast routines)
  @sa BBox3 TMatrix TMatrix4 Vector3 Point2 Point4
*/
class BSphere3
{
public:
  Vector3 c;
  float r, r2;
  INLINE BSphere3() { setempty(); }
  INLINE BSphere3(const Vector3 &p, float s) :
    c(p),
    r(s),
    r2(r * r)
  {
  }
  INLINE BSphere3 &operator=(const BBox3 &a)
  {
    if (a.lim[1].getX() < a.lim[0].getX())
    {
      setempty();
      return *this;
    }
    r = length(a.lim[1] - a.lim[0]) * (0.5f * 1.01f);
    c = (a.lim[1] + a.lim[0]) * 0.5f;
    r2 = r * r;
    return *this;
  }

  INLINE void setempty()
  {
    c.zero();
    r = -1;
    r2 = -1;
  }
  INLINE bool isempty() const { return r < 0; }

  INLINE BSphere3 &operator+=(const Vector3 &p)
  {
    Vector3 cd = p - c;
    float rd = length(cd);
    if (r >= rd)
      return *this;
    if (isempty())
      return *this = BSphere3(p, 0.f);
    float rad = (rd + r) / 2.f;
    c = c + cd * ((rad - r) / rd);
    r = rad;
    r2 = rad * rad;
    return *this;
  }
  INLINE BSphere3 &operator+=(const BSphere3 &b)
  {
    Vector3 cd = b.c - c;
    float rd = length(cd);
    if (b.isempty() || r >= rd + b.r)
      return *this;
    if (isempty() || b.r >= rd + r)
      return *this = b;
    float rad = (rd + r + b.r) / 2.f;
    c = c + cd * ((rad - r) / rd);
    r = rad;
    r2 = rad * rad;
    return *this;
  }

  INLINE BSphere3 &operator+=(const BBox3 &b)
  {
    if (b.isempty())
      return *this;
    if (isempty())
    {
      c = b.center();
      r2 = lengthSqr(b.width()) * 0.25;
      r = sqrtf(r2) * 1.01f;
      return *this;
    }
    Vector3 mind = absPerElem(b[0] - c);
    Vector3 maxd = absPerElem(b[1] - c);
    Vector3 p = Vector3(b[mind.getX() < maxd.getX() ? 1 : 0].getX(), b[mind.getY() < maxd.getY() ? 1 : 0].getY(), b[mind.getZ() < maxd.getZ() ? 1 : 0].getZ());
    *this += p;
    return *this;
  }

  INLINE bool operator&(const Vector3 &p) const
  {
    if (r < 0)
      return 0;
    return (lengthSqr(p - c) <= r2);
  }
  INLINE bool operator&(const BSphere3 &b) const
  {
    if (r < 0 || b.r < 0)
      return 0;
    float rd = r + b.r;
    return lengthSqr(c - b.c) < (rd * rd);
  }

  INLINE bool operator&(const BBox3 &b) const
  {

    float dmin = 0;
    for (int i = 0; i < 3; i++)
    {
      if (c[i] < b.lim[0][i])
        dmin += sqrf(c[i] - b.lim[0][i]);
      else if (c[i] > b.lim[1][i])
        dmin += sqrf(c[i] - b.lim[1][i]);
    }
    return dmin <= r2;
  }
};

INLINE BBox3::BBox3(const BSphere3 &s)
{
  if (s.r >= 0)
  {
    makecube(s.c, s.r * 2.0f);
  }
  else
    setempty();
}

INLINE BBox3 &BBox3::operator=(const BSphere3 &s)
{
  if (s.r >= 0)
  {
    makecube(s.c, s.r * 2.0f);
  }
  else
    setempty();
  return *this;
}

INLINE BBox3 &BBox3::operator+=(const BSphere3 &s)
{
  if (!s.isempty())
  {
    BBox3 bb;
    bb.makecube(s.c, s.r * 2.0f);
    *this += bb;
  }
  return *this;
}

}

#undef INLINE

/// @}
