#pragma once
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

// ============================================================================
// 独立的披帛变形模块
//
// 只对披帛/飘带顶点做局部变形，不改模型、材质和蒙皮：
//   * kind 8  ：Wind_RedRibbon / Wind_RedEdge（自肩头向右后方延伸的红色飘带）
//   * kind 9  ：Wind_GoldPibo（沿肩颈一圈的金色披帛，两处肩点固定）
//   * kind 26-29：Drape_*Silk（披帛主体的四层玉色/象牙色垂纱）
//
// 变形场是顶点静止坐标与时间的解析函数：
//     offset = amp * w(s) * [ D*sin(φ) + L*sin(0.61φ+1.2) + U*sin(φ+0.55) ]
//     φ      = ω t - λ(s)              （相位沿根部到末端逐渐滞后）
//     s      = 根部到末端的归一化弧长参数
//
// w(0)=0：肩部接缝完全固定；w 随 s 增大，末端摆幅最大；λ 随 s 增大，
// 形成由根到梢传播的行波，而不是整片披帛同相位平移。
// 所有量都是纯函数，暂停/重置不需要额外状态。
// ============================================================================
namespace pibo_motion {

inline bool ownsKind(int kind) {
    return kind == 8 || kind == 9 || (kind >= 26 && kind <= 29);
}

inline float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

// 根部到末端的归一化参数 s ∈ [0,1]。
// kind 8 沿肩头(-0.033)到飘带末端(-1.133)的 X 方向；
// kind 9 沿肩线(1.403)到披帛最低点(0.985)的 Y 方向；
// kind 26-29 使用 Blender 导出的 V 坐标（0.88 为肩部接缝，0.13 为下端）。
inline float along(int kind, const glm::vec3& local, const glm::vec2& uv) {
    if (kind == 8) return clamp01((-.033f - local.x) / 1.10f);
    if (kind == 9) return clamp01((1.403f - local.y) / .418f);
    return clamp01((.88f - uv.y) / .75f);
}

// 摆幅权重：根部恒为 0，中段轻摆，末端最大。
inline float weight(int kind, float s) {
    s = clamp01(s);
    if (kind == 8) return std::pow(s, 1.55f);
    if (kind == 9) {
        const float e = s * s * (3.0f - 2.0f * s);
        return std::pow(e, 1.15f);
    }
    // 四层垂纱：上段被衣缘压住，下段逐渐放开，末端额外增加 0.9 倍摆幅。
    const float t = s * s;
    const float free = clamp01((s - .20f) / .75f);
    return t * (.55f + .9f * (free * free * (3.0f - 2.0f * free)));
}

// 相位滞后 λ(s)：越靠末端相位越晚，形成根部带动末端的传播感。
inline float phaseLag(int kind, float s) {
    s = clamp01(s);
    if (kind == 8) return 6.20f * s;
    if (kind == 9) return 4.00f * s;
    return 5.40f * s + .32f * float(kind - 26);
}

// 各材质的基础摆幅（模型局部坐标，米）。
inline float amplitude(int kind) {
    switch (kind) {
        case 8:  return .052f;
        case 9:  return .024f;
        case 26: return .060f;
        case 27: return .052f;
        case 28: return .046f;
        default: return .050f;
    }
}

// 主变形场。strength 为主程序传入的布料风强（0..1.6），
// seconds 为统一的 animationTime。
inline glm::vec3 displacement(int kind, const glm::vec3& local, const glm::vec2& uv,
                              double seconds, float strength) {
    if (!ownsKind(kind) || strength <= 0.0f) return glm::vec3(0);
    const float s = along(kind, local, uv);
    const float w = weight(kind, s);
    if (w <= 1.0e-6f) return glm::vec3(0);

    const float phase = 1.08f * static_cast<float>(seconds) - phaseLag(kind, s);
    // 相对风：人物前进方向为 (+X,-Z)，衣料被推向她的左后方与镜头一侧。
    const glm::vec3 downstream = glm::normalize(glm::vec3(-.35f, 0.0f, 1.0f));
    const glm::vec3 lateral = glm::normalize(glm::vec3(1.0f, 0.0f, .35f));
    const glm::vec3 lift(0.0f, 1.0f, 0.0f);
    const glm::vec3 shape = downstream * std::sin(phase)
                          + lateral * (.35f * std::sin(.61f * phase + 1.2f))
                          + lift * (.30f * std::sin(phase + .55f));
    return shape * (amplitude(kind) * w * strength);
}

} // namespace pibo_motion
