#pragma once

#include <cmath>

// Minimal column-major 4x4 float matrix, layout matches OpenGL's glLoadMatrixf.
struct Mat4 {
    float m[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
};

struct Vec3f {
    float x, y, z;
};

inline Vec3f sub(Vec3f a, Vec3f b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3f cross(Vec3f a, Vec3f b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float dot(Vec3f a, Vec3f b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3f normalize(Vec3f v) {
    float len = std::sqrt(dot(v, v));
    if (len < 1e-6f) return {0, 0, 0};
    return {v.x / len, v.y / len, v.z / len};
}

// Right-handed look-at, camera space follows OpenGL convention (looking down -Z).
inline Mat4 lookAt(Vec3f eye, Vec3f center, Vec3f up) {
    Vec3f f = normalize(sub(center, eye));
    Vec3f s = normalize(cross(f, up));
    Vec3f u = cross(s, f);

    Mat4 r;
    r.m[0] = s.x;  r.m[4] = s.y;  r.m[8]  = s.z;  r.m[12] = -dot(s, eye);
    r.m[1] = u.x;  r.m[5] = u.y;  r.m[9]  = u.z;  r.m[13] = -dot(u, eye);
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z; r.m[14] = dot(f, eye);
    r.m[3] = 0;    r.m[7] = 0;    r.m[11] = 0;    r.m[15] = 1;
    return r;
}

// Column-major multiply: result transforms a vertex as a * (b * v).
inline Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            r.m[col * 4 + row] = sum;
        }
    }
    return r;
}

inline Mat4 perspective(float fovYDeg, float aspect, float zNear, float zFar) {
    float f = 1.0f / std::tan(fovYDeg * 3.14159265f / 180.0f / 2.0f);
    Mat4 r;
    for (int i = 0; i < 16; ++i) r.m[i] = 0;
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (zFar + zNear) / (zNear - zFar);
    r.m[11] = -1.0f;
    r.m[14] = (2 * zFar * zNear) / (zNear - zFar);
    return r;
}
