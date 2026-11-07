#pragma once
#include <glm/glm.hpp>
#include <array>

// Art/layout owner edits this file; lighting and animation stay independent.
namespace scene_layout {
inline constexpr float mainYaw=-3.f, mainPitch=5.f, mainDistance=6.7f;
inline const glm::vec3 mainTarget(.10f,1.45f,-2.6f);
inline const glm::vec3 caozhiPosition(-4.80f,.65f,-5.f);
inline constexpr float caozhiYaw=65.f, caozhiScale=1.35f;
inline constexpr float luoshenYaw=-25.f, luoshenScale=1.86f;
inline const glm::vec3 blossomPosition(-3.9f,.25f,-2.1f);
inline constexpr float blossomYaw=0.f, blossomScale=1.f;
inline const glm::vec3 boatPosition(-2.0f,.025f,-6.9f);
inline constexpr float boatYaw=-70.f, boatScale=.32f;
struct Rock {glm::vec3 position;float yaw,scale;};
inline const std::array<Rock,4> ledges{{
    {{-4.85f,-.20f,-5.f},-8.f,1.f},
    {{-5.3f,-.30f,-3.1f},18.f,1.15f},
    {{-5.0f,-.35f,.35f},-12.f,1.55f},
    {{-6.8f,-.30f,2.6f},30.f,1.7f}
}};
}
