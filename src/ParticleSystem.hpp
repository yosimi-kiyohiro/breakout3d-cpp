#pragma once
#include <vector>

#include "raylib.h"

// タスク4（⑥）：ブロック破壊時の3Dパーティクル演出。
// tetris-cpp（src/ParticleSystem.hpp）はピクセル座標（col/row）を受け取る2D版だが、
// 本プロジェクトはワールド座標（Vector3）で動くため、位置をVector3のまま受け取り
// 3次元的に全方向へ飛び散らせる版に適合させた。
struct Particle3D {
    Vector3 position;
    Vector3 velocity;  // units/秒
    float life;        // 残り寿命（秒）
    float maxLife;     // 初期寿命
    Color color;
    float size;  // 初期サイズ（ワールド単位。DrawCubeの一辺）
};

class ParticleSystem {
public:
    // posを中心に破片を全方向へ撒き散らす（ブロック破壊イベント1回につき1回呼ぶ）
    void emit(Vector3 pos, Color color);
    void update(float dt);
    void draw() const;

private:
    std::vector<Particle3D> particles_;
};
