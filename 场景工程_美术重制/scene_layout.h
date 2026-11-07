#pragma once
#include <glm/glm.hpp>
#include <array>

// Art/layout owner edits this file; lighting and animation stay independent.
namespace scene_layout {
inline constexpr float mainYaw=-3.f, mainPitch=5.f, mainDistance=6.7f;
inline const glm::vec3 mainTarget(.10f,1.45f,-2.6f);
inline const glm::vec3 caozhiPosition(-4.80f,.62f,-5.f);
inline constexpr float caozhiYaw=65.f, caozhiScale=1.35f;
inline const glm::vec3 luoshenPosition(1.f,.045f,-1.2f);
inline constexpr float luoshenYaw=-25.f, luoshenScale=1.86f;
inline const glm::vec3 blossomPosition(-3.34f,.46f,-2.65f);
inline constexpr float blossomYaw=0.f, blossomScale=1.12f;
inline const glm::vec3 blossomStretch(.79f,1.40f,1.f);
inline const glm::vec3 leftMountainPosition(-12.05f,-.30f,-33.06f);
inline const glm::vec3 leftMountainStretch(2.40f,2.98f,7.04f);
inline const glm::vec3 leftFoothillPosition(-12.75f,-.89f,-27.98f);
inline const glm::vec3 leftFoothillStretch(1.53f,1.58f,7.11f);
inline const glm::vec3 leftFarPosition(-18.11f,-.27f,-52.05f);
inline const glm::vec3 leftFarStretch(2.84f,2.01f,5.45f);
inline const glm::vec3 boatPosition(-2.0f,.025f,-6.9f);
inline constexpr float boatYaw=-6.f, boatScale=1.55f;
struct Rock {glm::vec3 position;float yaw,scale;};
inline const std::array<Rock,4> ledges{{
    {{-4.85f,0.f,-5.f},-8.f,1.f},
    {{-5.15f,-.30f,-3.1f},23.f,1.05f},
    {{-3.65f,-.30f,-.95f},-24.f,1.30f},
    {{-2.80f,-.30f,.55f},28.f,1.42f}
}};
struct BankRock {glm::vec3 position;float yaw;glm::vec3 size;int variant;};
inline const std::array<BankRock,23> bankRocks{{
    // Broad slabs at the bends, with smaller broken edges between them.
    {{-4.15f,.07f,-8.2f},-22.f,{1.95f,.68f,1.65f},0},
    {{-3.50f,-.08f,-9.6f},19.f,{1.38f,.57f,1.80f},1},
    {{-4.76f,.14f,-10.2f},35.f,{1.60f,.65f,1.35f},1},
    {{-3.37f,-.07f,-11.2f},-27.f,{1.42f,.60f,1.80f},2},
    {{-4.21f,.01f,-13.9f},-14.f,{1.75f,.63f,2.15f},0},
    {{-3.74f,-.09f,-15.0f},22.f,{1.40f,.56f,1.85f},1},
    {{-4.80f,.08f,-17.6f},46.f,{1.62f,.55f,2.10f},1},
    {{-4.09f,-.06f,-19.4f},-12.f,{1.66f,.55f,2.00f},2},
    {{-4.02f,-.04f,-23.2f},28.f,{1.35f,.52f,1.80f},1},
    // Stagger the right-bank ledges inland from the visible waterline.
    {{9.55f,-.06f,-10.9f},-28.f,{1.65f,.61f,1.65f},1},
    {{11.35f,.02f,-12.8f},12.f,{2.35f,.48f,2.50f},0},
    {{7.88f,-.13f,-15.3f},-12.f,{1.38f,.64f,2.00f},2},
    {{12.85f,.11f,-14.9f},-24.f,{2.85f,.73f,2.05f},1},
    {{8.00f,-.13f,-17.8f},36.f,{1.64f,.57f,2.20f},1},
    {{13.65f,.11f,-17.7f},-20.f,{2.15f,.65f,2.35f},0},
    {{8.47f,-.11f,-21.4f},14.f,{1.40f,.55f,2.00f},2},
    // Inland groups on the left meadow, behind the shoreline slabs.
    {{-5.75f,0,-8.7f},-22.f,{1.30f,.52f,1.15f},0},
    {{-6.65f,0,-9.3f},29.f,{.82f,.46f,.86f},1},
    {{-5.00f,0,-11.4f},-38.f,{1.16f,.57f,1.34f},2},
    {{-5.38f,0,-13.0f},16.f,{1.52f,.52f,1.45f},0},
    {{-7.55f,0,-14.1f},-18.f,{1.03f,.49f,1.12f},1},
    {{-5.15f,0,-17.5f},38.f,{1.12f,.44f,1.30f},1},
    {{-6.45f,0,-18.2f},-12.f,{.92f,.49f,1.20f},2}
}};
struct Pine {glm::vec3 position;float yaw,scale;};
// Left bank recedes from behind Cao Zhi; right bank stays behind Luo Shen.
// Left Y is replaced with terrainHeight at draw time. Right Y matches the island.
inline const std::array<Pine,11> riverPines{{
    {{-6.10f,0,-13.7f},-7.f,1.11f},
    {{-5.88f,0,-19.1f},28.f,.58f},
    {{-8.55f,0,-17.4f},-28.f,.58f},
    {{-5.72f,0,-24.8f},-23.f,.53f},
    {{-10.05f,0,-16.6f},45.f,.55f},
    {{-4.79f,0,-31.1f},20.f,.41f},
    {{10.45f,.19f,-12.7f},-25.f,.82f},
    {{12.55f,.25f,-15.0f},28.f,1.30f},
    {{8.9f,.07f,-17.1f},5.f,.84f},
    {{14.7f,.07f,-17.9f},-32.f,1.04f},
    {{11.2f,.07f,-19.5f},35.f,.69f}
}};
inline const std::array<Pine,5> leftGrove{{
    {{-7.48f,0,-17.65f},154.f,.53f},
    {{-6.12f,0,-21.7f},-8.f,.49f},
    {{-5.22f,0,-28.35f},160.f,.43f},
    {{-4.62f,0,-34.8f},-24.f,.34f},
    {{-4.27f,0,-39.7f},171.f,.30f}
}};
inline const glm::vec3 distantBankPosition(12.f,-.12f,-16.8f);
}
