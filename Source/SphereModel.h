#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include <utility>

// Kuglen: 14 lyde fordelt jævnt på en kugle. Bruges både af lyden og af tegningen,
// så det man ser altid er det man hører.
namespace blip
{
constexpr int kNumSounds = 14;

inline const char* soundNameUtf8 (int i)
{
    static const char* names[kNumSounds] = {
        "VARM PAD", "GLAS KLOKKE", "SAV BAS", "FM BJ\xc3\x86LDE", "VIND ST\xc3\x98J",
        "PLUK", "SUB BAS", "KOR", "ORGEL", "PWM LEAD",
        "GRANUL\xc3\x86R", "CHIP ARP", "M\xc3\x98RK DRONE", "STRENGE"
    };
    return names[i];
}

struct Vec3 { float x, y, z; };

// Fibonacci-fordeling: N punkter jævnt fordelt på en enhedskugle
inline Vec3 fibPoint (int N, int i)
{
    const float y = 1.0f - ((float) i + 0.5f) * 2.0f / (float) N;
    const float r = std::sqrt (std::max (0.0f, 1.0f - y * y));
    const float th = (float) i * 2.39996f;
    return { std::cos (th) * r, y, std::sin (th) * r };
}

inline Vec3 soundPos (int i) { return fibPoint (kNumSounds, i); }

// Drej kuglen: først om lodret akse (yaw), så om vandret akse (pitch).
// z > 0 betyder "vendt mod lytteren".
inline Vec3 rotate (Vec3 p, float yaw, float pitch)
{
    const float x1 = p.x * std::cos (yaw) - p.z * std::sin (yaw);
    const float z1 = p.x * std::sin (yaw) + p.z * std::cos (yaw);
    return { x1,
             p.y * std::cos (pitch) - z1 * std::sin (pitch),
             p.y * std::sin (pitch) + z1 * std::cos (pitch) };
}

// Keglens åbningsvinkel i radianer ud fra knappen (0..1)
inline float coneAngle (float kegle01) { return 0.3f + kegle01 * 1.2f; }

// Hvor meget hver lyd fylder i blandingen. Summen er altid 1.
inline void computeWeights (float yaw, float pitch, float kegle01, std::array<float, kNumSounds>& w)
{
    const float th = std::cos (coneAngle (kegle01));
    float sum = 0.0f, bestZ = -2.0f;
    int best = 0;

    for (int i = 0; i < kNumSounds; ++i)
    {
        const auto r = rotate (soundPos (i), yaw, pitch);
        const float d = std::max (0.0f, r.z - th);
        w[(size_t) i] = d * d;
        sum += w[(size_t) i];
        if (r.z > bestZ) { bestZ = r.z; best = i; }
    }

    if (sum <= 1.0e-9f)
    {
        w.fill (0.0f);
        w[(size_t) best] = 1.0f;
        return;
    }

    for (auto& v : w)
        v /= sum;
}

// Yaw og pitch der vender lyd nr. i direkte mod lytteren
inline std::pair<float, float> facingAngles (int i)
{
    const auto p = soundPos (i);
    const float yaw = std::atan2 (p.x, p.z);
    const float pitch = std::atan2 (p.y, std::sqrt (p.x * p.x + p.z * p.z));
    return { yaw, pitch };
}

// Stjernebilledets streger: hver lyd forbindes med sine to nærmeste naboer
inline const std::vector<std::pair<int, int>>& constellationEdges()
{
    static const std::vector<std::pair<int, int>> edges = []
    {
        std::vector<std::pair<int, int>> out;
        for (int i = 0; i < kNumSounds; ++i)
        {
            std::array<std::pair<float, int>, kNumSounds> d {};
            const auto p = soundPos (i);
            for (int j = 0; j < kNumSounds; ++j)
            {
                const auto q = soundPos (j);
                d[(size_t) j] = { j == i ? 1.0e9f : (p.x - q.x) * (p.x - q.x) + (p.y - q.y) * (p.y - q.y) + (p.z - q.z) * (p.z - q.z), j };
            }
            std::sort (d.begin(), d.end());
            for (int n = 0; n < 2; ++n)
            {
                const int a = std::min (i, d[(size_t) n].second), b = std::max (i, d[(size_t) n].second);
                bool exists = false;
                for (auto& e : out) if (e.first == a && e.second == b) exists = true;
                if (! exists) out.push_back ({ a, b });
            }
        }
        return out;
    }();
    return edges;
}

inline float wrapPi (float a)
{
    constexpr float twoPi = 6.28318530718f;
    a = std::fmod (a + 3.14159265359f, twoPi);
    if (a < 0.0f) a += twoPi;
    return a - 3.14159265359f;
}
} // namespace blip
