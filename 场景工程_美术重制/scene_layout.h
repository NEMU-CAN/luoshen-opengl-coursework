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
struct Pine {glm::vec3 position;float yaw,scale;};
// Left bank recedes from behind Cao Zhi; right bank stays behind Luo Shen.
// Left Y is replaced with terrainHeight at draw time. Right Y matches the island.
inline const std::array<Pine,11> riverPines{{
    {{-7.4f,0,-9.6f},-15.f,1.03f},
    {{-5.6f,0,-12.0f},32.f,.82f},
    {{-9.0f,0,-16.0f},-28.f,1.20f},
    {{-6.5f,0,-20.0f},18.f,.65f},
    {{-11.7f,0,-17.4f},45.f,.95f},
    {{-4.8f,0,-21.8f},-20.f,.62f},
    {{9.15f,.07f,-12.7f},-25.f,1.02f},
    {{11.4f,.07f,-15.0f},28.f,1.23f},
    {{8.9f,.07f,-17.1f},5.f,.84f},
    {{14.7f,.07f,-17.9f},-32.f,1.04f},
    {{11.2f,.07f,-19.5f},35.f,.69f}
}};
inline const glm::vec3 distantBankPosition(12.f,-.34f,-16.8f);
}
