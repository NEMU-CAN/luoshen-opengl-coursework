#pragma once
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

// ============================================================================
// 角色与叙事动画：洛神沿三次 Bézier 曲线离去、披帛风场、小舟微动。
//
// 约定：
//   * 世界坐标 Y 轴向上，水面 Y = 0；人物以包围盒底部为落脚点。
//   * 所有姿态都是 animationTime（秒）的纯函数，不累加状态。
//     因此暂停时画面冻结，重置后同一时间点必定回到同一位姿。
//   * 动画总长 45 秒：0-4 秒静立回望，4-34 秒沿曲线离去，34-45 秒在终点停留。
// ============================================================================
namespace scene_motion {

inline constexpr double kDuration = 45.0;      // 完整叙事时长
inline constexpr double kStillEnd = 4.0;       // 起步前的停顿
inline constexpr double kTravelEnd = 34.0;     // 到达终点
inline constexpr double kHoldEnd = 45.0;       // 结尾停留到 45 秒

// 三次 Bézier 控制点。起点与 scene_layout::luoshenPosition 完全一致。
// 路线沿主镜头的屏幕右方展开、只略微向纵深后退：这样位移在画面上是
// 可见的横向移动，而不是"原地转身、只剩缩小"的观感。终点仍在开阔水面内，
// 不越过右岸，也不进入中央叙事留白。
inline constexpr glm::vec3 kP0(1.000f, .045f, -1.200f);
inline constexpr glm::vec3 kP1(2.300f, .045f, -1.700f);
inline constexpr glm::vec3 kP2(3.400f, .045f, -2.450f);
inline constexpr glm::vec3 kP3(4.450f, .045f, -2.800f);

// 朝向（度，绕 Y 轴）。模型默认面向 +Z；起点使用静态站位的 -25°，
// 离去时与曲线参数同步转到路线末端的切线方向（约 108°），
// 也就是"转身、朝右前方走去"，中途不会停在原地打转。
inline constexpr float kStartYaw = -25.0f;
inline constexpr float kEndYaw = 108.0f;

// 小舟微动参数：都以 0 时刻为静止基准，保证重置后与布局一致。
inline constexpr float kBoatHeave = .018f;      // 升沉幅度（米）
inline constexpr float kBoatRoll = 1.05f;       // 横摇幅度（度）
inline constexpr float kBoatPitch = .55f;       // 纵摇幅度（度）
inline constexpr float kBoatYaw = .45f;         // 艏摇幅度（度）

struct Farewell {
    glm::vec3 position;   // 落脚点世界坐标
    float yaw;            // 朝向角度
    float speed;          // 瞬时速度（世界单位/秒），供测试与调试
    float curve;          // 当前 Bézier 参数 t ∈ [0,1]
    int phase;            // 0 静立，1 行进，2 终点停留
};

struct BoatMotion {
    float heave;   // 垂直位移
    float yaw;     // 艏摇
    float pitch;   // 纵摇（绕 X）
    float roll;    // 横摇（绕 Z）
};

inline float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

// 五次平滑曲线（C2 连续，起止速度为 0），用于时间到曲线参数的映射。
inline float smootherstep(float t) {
    t = clamp01(t);
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

// 解析 Bézier 位置。
inline glm::vec3 bezierPoint(float t) {
    const float s = 1.0f - t;
    return s * s * s * kP0 + 3.0f * s * s * t * kP1
         + 3.0f * s * t * t * kP2 + t * t * t * kP3;
}

// 解析 Bézier 一阶导数，用于求切线朝向。
inline glm::vec3 bezierTangent(float t) {
    const float s = 1.0f - t;
    return 3.0f * s * s * (kP1 - kP0)
         + 6.0f * s * t * (kP2 - kP1)
         + 3.0f * t * t * (kP3 - kP2);
}

// 时间到曲线参数：起步前停顿，随后用梯形速度（加速—匀速—减速）走完全程。
// 若直接用 smootherstep 作用于位置，曲线两端速度虽然为 0，但中段起步过慢，
// 45 秒演示里前十几秒几乎看不出移动。这里改为对速度积分：
//   速度 v(u)：0..a 加速，a..1-b 匀速，1-b..1 减速，两端速度为 0。
// 再对累计路程归一化，得到时间到曲线参数的映射，起步后约 2 秒即可看到位移。
inline float travelFraction(float u) {
    u = clamp01(u);
    constexpr float rampIn = .12f, rampOut = .18f;
    const float total = 1.0f - .5f * (rampIn + rampOut);
    float distance;
    if (u < rampIn) {
        const float q = u / rampIn;
        distance = rampIn * (q * q * q - .5f * q * q * q * q);
    } else if (u <= 1.0f - rampOut) {
        distance = .5f * rampIn + (u - rampIn);
    } else {
        const float q = (1.0f - u) / rampOut;
        distance = .5f * rampIn + (1.0f - rampIn - rampOut)
                 + rampOut * (.5f - q * q * q + .5f * q * q * q * q);
    }
    return clamp01(distance / total);
}

// 时间到曲线参数：先停顿，再按梯形速度离去，最后停在终点。
inline float curveParam(double seconds) {
    const float u = static_cast<float>((seconds - kStillEnd) / (kTravelEnd - kStillEnd));
    return travelFraction(u);
}

// 洛神离去姿态。秒数超出 [0,45] 时按端点夹紧（重播与结束停留都稳定）。
inline Farewell farewell(double seconds) {
    const double s = std::clamp(seconds, 0.0, kDuration);
    Farewell pose;
    pose.curve = curveParam(s);
    pose.position = bezierPoint(pose.curve);
    pose.position.y = .045f;

    // 转向使用与位移相同的缓动，保证起步、停止时角速度为 0。
    pose.yaw = kStartYaw + (kEndYaw - kStartYaw) * pose.curve;

    // 数值速度，仅用于测试与调试输出。
    const double h = 1.0e-3;
    const glm::vec3 before = bezierPoint(curveParam(s - h));
    const glm::vec3 after = bezierPoint(curveParam(s + h));
    pose.speed = glm::length(after - before) / static_cast<float>(2.0 * h);
    if (s < kStillEnd) pose.speed = 0.0f;

    pose.phase = s >= kTravelEnd ? 2 : (s < kStillEnd ? 0 : 1);
    return pose;
}

// 兼容旧接口（main.cpp 早期版本与 mesh_tests.cpp 使用）。
inline glm::vec3 farewellPosition(double seconds) {
    return farewell(seconds).position;
}

// 小舟微动：升沉、艏摇、纵摇、横摇分别使用互质周期，0 秒时全部归零。
inline BoatMotion boatMotion(double seconds) {
    const float t = static_cast<float>(std::max(0.0, seconds));
    BoatMotion motion;
    motion.heave = kBoatHeave * std::sin(.72f * t) + .006f * std::sin(.33f * t);
    motion.roll = kBoatRoll * std::sin(.55f * t);
    motion.pitch = kBoatPitch * std::sin(.81f * t);
    motion.yaw = kBoatYaw * std::sin(.47f * t);
    return motion;
}

// 披帛风场强度：开场几乎无风，起步后逐渐加强，途中有一阵持续微风，
// 到达终点后回落到安静的程度。返回值约 0..1，供披帛模块使用。
inline float windEnvelope(double seconds) {
    const float t = static_cast<float>(seconds);
    const float ramp = smootherstep(t / 2.2f);
    const float gust = .78f + .22f * std::sin(.36f * t - 1.1f);
    const float settle = 1.0f - .34f * smootherstep((t - 34.0f) / 11.0f);
    return clamp01(ramp * gust * settle);
}

} // namespace scene_motion
