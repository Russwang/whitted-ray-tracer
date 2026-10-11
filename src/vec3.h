// vec3.h
#pragma once

#include <cmath>

struct Vec3 {
    float x;
    float y;
    float z;

    // 构造函数
    Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    Vec3(float xx, float yy, float zz) : x(xx), y(yy), z(zz) {}

    // 下标访问：0->x, 1->y, 2->z
    float& operator[](int i) {
        if (i == 0) return x;
        if (i == 1) return y;
        return z;
    }
    const float& operator[](int i) const {
        if (i == 0) return x;
        if (i == 1) return y;
        return z;
    }

    // 一元负号
    Vec3 operator-() const { return Vec3(-x, -y, -z); }

    // 分量加减乘除
    Vec3& operator+=(const Vec3& v) {
        x += v.x; y += v.y; z += v.z;
        return *this;
    }
    Vec3& operator-=(const Vec3& v) {
        x -= v.x; y -= v.y; z -= v.z;
        return *this;
    }
    Vec3& operator*=(float s) {
        x *= s; y *= s; z *= s;
        return *this;
    }
    Vec3& operator/=(float s) {
        float inv = 1.0f / s;
        x *= inv; y *= inv; z *= inv;
        return *this;
    }
};

// 非成员运算符
inline Vec3 operator+(const Vec3& a, const Vec3& b) {
    return Vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}
inline Vec3 operator-(const Vec3& a, const Vec3& b) {
    return Vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}
inline Vec3 operator*(const Vec3& v, float s) {
    return Vec3(v.x * s, v.y * s, v.z * s);
}
inline Vec3 operator*(float s, const Vec3& v) {
    return Vec3(v.x * s, v.y * s, v.z * s);
}
inline Vec3 operator/(const Vec3& v, float s) {
    float inv = 1.0f / s;
    return Vec3(v.x * inv, v.y * inv, v.z * inv);
}

// 点乘、叉乘、长度、归一化
inline float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

inline float length(const Vec3& v) {
    return std::sqrt(dot(v, v));
}

inline Vec3 normalize(const Vec3& v) {
    float len = length(v);
    if (len == 0.0f) return v;
    return v / len;
}
