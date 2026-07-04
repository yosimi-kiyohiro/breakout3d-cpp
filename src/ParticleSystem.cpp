#include "ParticleSystem.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "raymath.h"

namespace {

// raylib.h が PI をマクロ定義しているため衝突を避けて別名にする（他ファイルと同じ方針）
constexpr float PARTICLE_PI = 3.14159265358979323846f;

constexpr int PARTICLES_PER_EMIT = 10;
constexpr float GRAVITY = 6.0f;    // units/秒²（ワールド座標系での落下演出）
constexpr float SPEED_MIN = 1.5f;
constexpr float SPEED_MAX = 4.5f;
constexpr float LIFE_MIN = 0.35f;
constexpr float LIFE_MAX = 0.7f;
constexpr float SIZE_BASE = 0.12f;

float randF(float lo, float hi) { return lo + (hi - lo) * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)); }

// 単位球面上で一様乱数な方向ベクトルを作る（破片が全方向に均等に飛び散るようにするため）
Vector3 randomDirection() {
    float theta = randF(0.0f, 2.0f * PARTICLE_PI);
    float z = randF(-1.0f, 1.0f);
    float ringRadius = std::sqrt(std::max(0.0f, 1.0f - z * z));
    return {ringRadius * std::cos(theta), ringRadius * std::sin(theta), z};
}

}  // namespace

void ParticleSystem::emit(Vector3 pos, Color color) {
    for (int i = 0; i < PARTICLES_PER_EMIT; ++i) {
        Vector3 dir = randomDirection();
        float speed = randF(SPEED_MIN, SPEED_MAX);
        float life = randF(LIFE_MIN, LIFE_MAX);

        Particle3D p;
        p.position = pos;
        p.velocity = Vector3Scale(dir, speed);
        p.life = life;
        p.maxLife = life;
        p.color = color;
        p.size = SIZE_BASE + randF(-0.03f, 0.03f);
        particles_.push_back(p);
    }
}

void ParticleSystem::update(float dt) {
    for (auto& p : particles_) {
        p.velocity.y -= GRAVITY * dt;
        p.position.x += p.velocity.x * dt;
        p.position.y += p.velocity.y * dt;
        p.position.z += p.velocity.z * dt;
        p.life -= dt;
    }
    // 寿命が尽きたパーティクルを削除
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
                                     [](const Particle3D& p) { return p.life <= 0.0f; }),
                      particles_.end());
}

void ParticleSystem::draw() const {
    for (const auto& p : particles_) {
        float ratio = p.life / p.maxLife;  // 1.0→0.0
        float sz = p.size * ratio;
        if (sz < 0.01f) continue;

        Color c = p.color;
        c.a = static_cast<unsigned char>(255 * ratio);  // フェードアウト
        DrawCube(p.position, sz, sz, sz, c);
    }
}
