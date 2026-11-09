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
inline const glm::vec3 blossomPosition(-3.39f,.55f,-2.82f);
inline constexpr float blossomYaw=-32.f, blossomScale=1.12f;
inline const glm::vec3 blossomStretch(1.00f,1.28f,1.f);
inline const glm::vec3 leftMountainPosition(-13.15f,-.30f,-33.06f);
inline const glm::vec3 leftMountainStretch(2.58f,2.57f,6.35f);
inline const glm::vec3 leftFoothillPosition(-12.05f,-.43f,-25.3f);
inline const glm::vec3 leftFoothillStretch(1.72f,1.50f,4.4f);
inline const glm::vec3 leftFarPosition(-18.11f,-.27f,-52.05f);
inline const glm::vec3 leftFarStretch(2.84f,2.01f,5.45f);
inline const glm::vec3 boatPosition(-2.97f,.025f,-8.12f);
inline constexpr float boatYaw=-23.f, boatScale=1.23f;
struct Rock {glm::vec3 position;float yaw,scale;};
inline const std::array<Rock,4> ledges{{
    {{-4.85f,0.f,-5.f},-8.f,1.f},
    {{-5.15f,-.30f,-3.1f},23.f,1.05f},
    {{-3.65f,-.30f,-.95f},-24.f,1.30f},
    {{-2.80f,-.30f,.55f},28.f,1.42f}
}};
struct BankRock {glm::vec3 position;float yaw;glm::vec3 size;int variant;};
// Slots 1-9 are kept for old layout IDs. Their shoreline is part of the continuous mesh.
inline constexpr int firstStandaloneBankRock=9;
inline const std::array<BankRock,23> bankRocks{{
    // Broad slabs at the bends, with smaller broken edges between them.
    {{-4.94f,-.08f,-8.2f},-22.f,{1.50f,.48f,1.65f},0},
    {{-4.12f,-.12f,-9.6f},19.f,{1.38f,.57f,1.80f},1},
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
    {{-6.78f,0,-15.1f},-10.f,1.21f},
    {{-5.54f,0,-20.7f},18.f,.45f},
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
    {{-4.03f,0,-32.0f},-24.f,.38f},
    {{-4.27f,0,-39.7f},171.f,.30f}
}};
struct LeftBankDetail {
    int round;
    bool pine;
    int variant;
    float x, z, yaw;
    glm::vec3 size;
};
// Build the bank in successive clusters, leaving open water beyond the shore.
inline const LeftBankDetail leftBankDetails[] = {
    // Round 1
    {1, false, 0, -5.0f, -10.5f, 17.0f, {1.25f, 0.49f, 1.28f}},
    {1, false, 1, -6.2f, -10.1f, -25.0f, {1.38f, 0.55f, 1.02f}},
    {1, false, 0, -6.95f, -11.7f, 35.0f, {1.36f, 0.55f, 1.28f}},
    {1, false, 2, -5.65f, -12.4f, -6.0f, {0.99f, 0.53f, 1.36f}},
    {1, false, 1, -7.85f, -12.9f, 20.0f, {1.25f, 0.51f, 1.2f}},
    {1, false, 2, -4.51f, -11.7f, 40.0f, {0.75f, 0.38f, 0.81f}},
    // Round 2
    {2, true, 0, -7.75f, -11.8f, -34.0f, {0.64f, 0.64f, 0.64f}},
    {2, true, 2, -6.65f, -12.1f, 120.0f, {0.5f, 0.5f, 0.5f}},
    {2, true, 1, -8.46f, -13.6f, 38.0f, {0.57f, 0.57f, 0.57f}},
    {2, true, 2, -8.96f, -15.0f, 145.0f, {0.52f, 0.52f, 0.52f}},
    {2, true, 2, -7.53f, -14.2f, 61.0f, {0.49f, 0.49f, 0.49f}},
    // Round 3
    {3, false, 0, -7.42f, -15.5f, 18.0f, {1.58f, 0.57f, 1.39f}},
    {3, false, 1, -8.5f, -15.9f, 34.0f, {1.43f, 0.52f, 1.19f}},
    {3, false, 1, -6.15f, -15.9f, -45.0f, {1.32f, 0.52f, 1.12f}},
    {3, false, 0, -5.05f, -16.5f, 24.0f, {1.02f, 0.42f, 0.96f}},
    {3, false, 2, -7.12f, -16.7f, -35.0f, {1.35f, 0.47f, 1.27f}},
    {3, false, 0, -8.96f, -17.7f, 41.0f, {1.16f, 0.45f, 1.32f}},
    // Round 4
    {4, true, 0, -5.6f, -16.0f, 31.0f, {0.46f, 0.46f, 0.46f}},
    {4, true, 2, -6.37f, -16.7f, 143.0f, {0.48f, 0.48f, 0.48f}},
    {4, true, 1, -7.2f, -18.35f, -59.0f, {0.45f, 0.45f, 0.45f}},
    {4, true, 2, -5.1f, -18.6f, 56.0f, {0.37f, 0.37f, 0.37f}},
    {4, true, 0, -6.42f, -19.45f, 119.0f, {0.44f, 0.44f, 0.44f}},
    {4, true, 2, -8.09f, -19.2f, 35.0f, {0.47f, 0.47f, 0.47f}},
    // Round 5
    {5, false, 0, -4.63f, -18.3f, 11.0f, {1.06f, 0.4f, 1.33f}},
    {5, false, 1, -5.12f, -19.7f, -17.0f, {1.31f, 0.37f, 1.31f}},
    {5, false, 2, -4.2f, -20.8f, 41.0f, {1.15f, 0.4f, 1.49f}},
    {5, false, 0, -5.44f, -21.4f, -32.0f, {1.31f, 0.46f, 1.06f}},
    {5, false, 1, -4.17f, -22.6f, 39.0f, {0.83f, 0.43f, 1.26f}},
    {5, false, 2, -5.05f, -23.6f, 6.0f, {0.91f, 0.41f, 1.29f}},
    // Round 6
    {6, true, 2, -5.6f, -23.0f, 118.0f, {0.36f, 0.36f, 0.36f}},
    {6, true, 1, -4.98f, -24.25f, -12.0f, {0.32f, 0.32f, 0.32f}},
    {6, true, 0, -6.05f, -25.5f, 57.0f, {0.36f, 0.36f, 0.36f}},
    {6, true, 2, -4.85f, -26.6f, 140.0f, {0.32f, 0.32f, 0.32f}},
    {6, true, 1, -5.67f, -27.4f, -30.0f, {0.3f, 0.3f, 0.3f}},
    {6, true, 0, -4.55f, -29.5f, 29.0f, {0.29f, 0.29f, 0.29f}},
    // Round 7
    {7, false, 0, -4.34f, -25.6f, -28.0f, {0.87f, 0.34f, 1.25f}},
    {7, false, 1, -4.55f, -27.1f, 24.0f, {1.06f, 0.32f, 1.18f}},
    {7, false, 2, -3.78f, -28.5f, 17.0f, {0.87f, 0.29f, 1.06f}},
    {7, false, 0, -4.48f, -30.6f, -20.0f, {1.03f, 0.29f, 1.2f}},
    {7, false, 1, -3.82f, -32.7f, 32.0f, {0.84f, 0.29f, 1.0f}},
    {7, false, 2, -3.68f, -34.3f, -19.0f, {0.76f, 0.29f, 0.93f}},
    // Round 8
    {8, true, 2, -4.05f, -26.8f, 141.0f, {0.33f, 0.33f, 0.33f}},
    {8, true, 0, -4.25f, -28.7f, -20.0f, {0.30f, 0.30f, 0.30f}},
    {8, true, 2, -3.83f, -35.25f, 26.0f, {0.24f, 0.24f, 0.24f}},
    {8, true, 1, -4.65f, -37.0f, 127.0f, {0.22f, 0.22f, 0.22f}},
    {8, true, 2, -3.88f, -38.15f, -45.0f, {0.22f, 0.22f, 0.22f}},
    {8, true, 0, -4.22f, -41.1f, 38.0f, {0.19f, 0.19f, 0.19f}},
    // Round 9
    {9, false, 0, -7.4f, -9.3f, 12.0f, {1.21f, 0.48f, 1.32f}},
    {9, false, 1, -8.1f, -10.35f, 55.0f, {1.19f, 0.47f, 1.26f}},
    {9, false, 2, -7.17f, -13.75f, -33.0f, {1.15f, 0.48f, 1.05f}},
    {9, false, 1, -6.01f, -14.45f, 38.0f, {1.24f, 0.43f, 1.11f}},
    {9, false, 2, -6.73f, -15.4f, -13.0f, {0.92f, 0.4f, 1.05f}},
    // Round 10
    {10, true, 2, -9.25f, -10.7f, 23.0f, {0.55f, 0.55f, 0.55f}},
    {10, true, 0, -10.15f, -12.8f, 121.0f, {0.57f, 0.57f, 0.57f}},
    {10, true, 0, -8.54f, -14.65f, -23.0f, {0.41f, 0.41f, 0.41f}},
    {10, true, 2, -7.64f, -20.25f, 81.0f, {0.37f, 0.37f, 0.37f}},
    {10, true, 2, -5.75f, -29.6f, 19.0f, {0.26f, 0.26f, 0.26f}},
    // Layer the exposed rock from the near bluff to the shoreline.
    {17, false, 0, -7.1f, -7.1f, 25.0f, {1.28f, 1.02f, 1.42f}},
    {17, false, 1, -7.92f, -8.05f, -33.0f, {1.18f, 0.91f, 1.3f}},
    {18, false, 0, -8.72f, -9.5f, -31.0f, {1.56f, 1.18f, 1.48f}},
    {18, false, 1, -9.57f, -10.55f, 21.0f, {1.42f, 1.01f, 1.49f}},
    {19, false, 0, -9.82f, -11.8f, 44.0f, {1.83f, 1.18f, 1.38f}},
    {19, false, 1, -10.47f, -12.82f, -16.0f, {1.36f, 0.94f, 1.64f}},
    {20, false, 1, -7.33f, -8.62f, 12.0f, {1.36f, 0.7f, 1.34f}},
    {20, false, 0, -6.45f, -7.71f, -16.0f, {1.12f, 0.75f, 1.27f}},
    {21, false, 0, -7.97f, -8.87f, 38.0f, {1.35f, 1.40f, 1.27f}},
    {21, false, 2, -7.81f, -11.45f, -22.0f, {1.01f, 0.7f, 1.35f}},
    {22, false, 0, -10.47f, -14.2f, 18.0f, {1.6f, 1.02f, 1.5f}},
    {22, false, 1, -11.0f, -15.18f, -19.0f, {1.31f, 0.89f, 1.39f}},
    {23, false, 0, -11.8f, -15.9f, -35.0f, {1.45f, 0.86f, 1.38f}},
    {23, false, 1, -11.05f, -16.8f, 14.0f, {1.21f, 0.75f, 1.2f}},
    {24, false, 1, -9.68f, -13.5f, -42.0f, {1.52f, 0.84f, 1.25f}},
    {24, false, 2, -8.83f, -13.96f, 16.0f, {1.17f, 0.62f, 1.4f}},
    {25, false, 0, -9.1f, -14.9f, 24.0f, {1.4f, 0.74f, 1.31f}},
    {25, false, 2, -8.63f, -15.5f, -38.0f, {1.07f, 0.64f, 1.08f}},
    {26, false, 0, -7.83f, -13.1f, -28.0f, {1.28f, 0.68f, 1.19f}},
    {26, false, 2, -6.95f, -12.58f, 37.0f, {1.08f, 0.5f, 1.26f}},
    {27, false, 0, -6.07f, -10.37f, 39.0f, {1.17f, 0.68f, 1.19f}},
    {27, false, 1, -5.8f, -11.85f, -17.0f, {1.02f, 0.52f, 1.08f}},
    {28, false, 1, -6.87f, -14.69f, 24.0f, {1.3f, 0.62f, 1.31f}},
    {28, false, 2, -5.91f, -14.78f, -14.0f, {0.94f, 0.48f, 1.02f}},
    {29, false, 0, -7.65f, -16.3f, 28.0f, {1.24f, 0.55f, 1.22f}},
    {29, false, 1, -7.25f, -17.38f, -24.0f, {0.99f, 0.51f, 1.27f}},
    {30, false, 1, -8.95f, -17.75f, -37.0f, {1.39f, 0.62f, 1.3f}},
    {30, false, 2, -8.15f, -18.45f, 24.0f, {1.14f, 0.52f, 1.21f}},
    {31, false, 0, -7.49f, -19.52f, 9.0f, {1.08f, 0.58f, 1.19f}},
    {31, false, 1, -6.81f, -19.85f, -28.0f, {1.1f, 0.49f, 1.1f}},
    {32, false, 0, -5.59f, -17.83f, -27.0f, {0.94f, 0.55f, 1.25f}},
    {32, false, 2, -5.27f, -18.63f, 22.0f, {1.02f, 0.53f, 1.26f}},
    {33, false, 0, -6.09f, -20.97f, 42.0f, {1.21f, 0.52f, 1.18f}},
    {33, false, 2, -5.59f, -21.63f, -27.0f, {1.11f, 0.46f, 1.24f}},
    {34, false, 0, -5.03f, -22.3f, 12.0f, {0.91f, 0.5f, 1.12f}},
    {34, false, 1, -4.74f, -23.02f, -18.0f, {0.86f, 0.44f, 1.16f}},
    {35, false, 0, -5.43f, -20.15f, 32.0f, {1.05f, 0.75f, 1.18f}},
    {35, false, 2, -5.44f, -24.3f, 48.0f, {0.89f, 0.46f, 1.24f}},
    {36, false, 0, -4.58f, -21.85f, -12.0f, {0.98f, 0.70f, 1.1f}},
    {36, false, 2, -4.36f, -24.62f, 23.0f, {0.84f, 0.4f, 1.19f}},
    {37, false, 0, -4.72f, -25.2f, -18.0f, {0.82f, 0.42f, 1.08f}},
    {37, false, 1, -4.12f, -25.85f, 26.0f, {0.73f, 0.36f, 1.09f}},
    {38, false, 0, -4.18f, -27.25f, 19.0f, {0.75f, 0.37f, 1.08f}},
    {38, false, 2, -3.93f, -28.03f, -19.0f, {0.7f, 0.36f, 0.93f}},
    {39, false, 0, -6.54f, -6.9f, -48.0f, {0.90f, 0.80f, 0.90f}},
    {39, false, 0, -7.87f, -10.7f, 24.0f, {1.08f, 0.6f, 1.08f}},
    {40, false, 0, -8.38f, -10.45f, 57.0f, {1.30f, 1.34f, 1.49f}},
    {40, false, 2, -9.7f, -12.12f, -19.0f, {1.17f, 0.7f, 1.21f}},
};
struct LeftBankRidge {
    int round, variant;
    glm::vec3 position;
    float yaw;
    glm::vec3 size;
};
inline const LeftBankRidge leftBankRidges[] = {
    {11, 2, {-19.8f, -0.12f, -23.0f}, -12.0f, {1.18f, 1.75f, 2.3f}},
    {12, 2, {-13.9f, -0.18f, -17.0f}, 9.0f, {0.73f, 1.08f, 1.7f}},
    {13, 1, {-24.0f, -0.24f, -34.0f}, 13.0f, {1.13f, 2.0f, 3.0f}},
    {14, 2, {-15.0f, -0.14f, -21.0f}, -18.0f, {0.94f, 1.3f, 1.85f}},
    {15, 2, {-16.7f, 0.40f, -24.0f}, 17.0f, {0.62f, 1.39f, 1.55f}},
    {16, 2, {-12.6f, 0.20f, -17.0f}, 21.0f, {0.48f, 1.10f, 1.5f}},
};
inline constexpr int leftBankRounds=42;
inline const glm::vec3 distantBankPosition(12.f,-.12f,-16.8f);
}
