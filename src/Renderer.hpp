#pragma once
#include <vector>

#include "Ball.hpp"
#include "Block.hpp"
#include "Paddle.hpp"
#include "raylib.h"

// ③-A：raylib描画をラップするクラス。
// カメラはCAMERA_CUSTOM固定（ステージ全体・パドルが見える俯瞰角度に固定し、
// UpdateCamera()は呼ばない。可動カメラは今回のスコープ外）。
class Renderer {
public:
    Renderer();
    ~Renderer();

    bool shouldClose() const;

    void beginFrame();
    void endFrame();
    void beginScene3D();
    void endScene3D();

    void drawField();
    void drawPaddle(const Paddle& paddle);
    void drawBall(const Ball& ball);
    void drawBlocks(const std::vector<Block>& blocks);

    // 2D HUD（常時表示）。タスク6でhighScoreを追加（ScoreFileで永続化した値を表示する）
    void drawHud(int score, int lives, int stageIndex, int highScore);

    // フェーズ別オーバーレイ（最小実装。演出強化は④以降）
    // タスク6：タイトル画面にもハイスコアを表示する
    void drawTitleOverlay(int highScore);
    void drawPauseOverlay();
    void drawGameOverOverlay(int score);
    void drawClearOverlay(int stageIndex);

private:
    Camera3D camera_;
};
