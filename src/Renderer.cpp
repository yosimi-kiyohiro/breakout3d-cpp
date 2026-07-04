#include "Renderer.hpp"

#include "constants.hpp"

namespace {
constexpr int SCREEN_WIDTH = 1024;
constexpr int SCREEN_HEIGHT = 768;
}  // namespace

Renderer::Renderer() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Breakout3D");
    SetTargetFPS(60);

    // 固定カメラ（CAMERA_CUSTOM運用：値を直接セットし、UpdateCamera()は呼ばない）
    camera_.position = {0.0f, 14.0f, 20.0f};
    camera_.target = {0.0f, 4.0f, 0.0f};
    camera_.up = {0.0f, 1.0f, 0.0f};
    camera_.fovy = 45.0f;
    camera_.projection = CAMERA_PERSPECTIVE;
}

Renderer::~Renderer() { CloseWindow(); }

bool Renderer::shouldClose() const { return WindowShouldClose(); }

void Renderer::beginFrame() {
    BeginDrawing();
    ClearBackground(BLACK);
}

void Renderer::endFrame() { EndDrawing(); }

void Renderer::beginScene3D() { BeginMode3D(camera_); }

void Renderer::endScene3D() { EndMode3D(); }

void Renderer::drawField() {
    Vector3 fieldCenter = {0.0f, Constants::Y_MAX / 2.0f, 0.0f};
    Vector3 fieldSize = {Constants::X_MAX - Constants::X_MIN, Constants::Y_MAX,
                          Constants::Z_MAX - Constants::Z_MIN};
    DrawCubeWires(fieldCenter, fieldSize.x, fieldSize.y, fieldSize.z, GRAY);
}

void Renderer::drawPaddle(const Paddle& paddle) {
    Vector3 size = {paddle.halfWidth * 2.0f, paddle.halfHeight * 2.0f, paddle.halfDepth * 2.0f};
    DrawCube(paddle.position, size.x, size.y, size.z, BLUE);
    DrawCubeWires(paddle.position, size.x, size.y, size.z, DARKBLUE);
}

void Renderer::drawBall(const Ball& ball) {
    DrawSphere(ball.position, ball.radius, RED);
    // タスク7：見た目の仕上げ。黒背景に対してボールの輪郭を強調するワイヤーフレームを重ねる
    // （ライティングシェーダー等の重い演出は避け、既存のDrawXWiresの延長で済む軽量な追加のみ）
    DrawSphereWires(ball.position, ball.radius, 8, 8, MAROON);
}

void Renderer::drawBlocks(const std::vector<Block>& blocks) {
    for (const auto& block : blocks) {
        if (!block.alive) continue;
        // 見た目のサイズは当たり判定（getBoundingBox）と完全一致させる
        Vector3 size = {block.halfWidth * 2.0f, block.halfHeight * 2.0f, block.halfDepth * 2.0f};
        // タスク7：見た目の仕上げ。耐久値3以上（ステージ7以降で出現）の色分けを追加し、
        // ステージが進むにつれて耐久値が上がっていくことをプレイヤーが色で把握できるようにする
        Color color = (block.durability >= 3) ? DARKPURPLE : (block.durability == 2 ? MAROON : ORANGE);
        DrawCube(block.position, size.x, size.y, size.z, color);
        DrawCubeWires(block.position, size.x, size.y, size.z, BLACK);
    }
}

void Renderer::drawHud(int score, int lives, int stageIndex, int highScore) {
    DrawText(TextFormat("SCORE: %d", score), 10, 10, 20, RAYWHITE);
    DrawText(TextFormat("HIGH SCORE: %d", highScore), 10, 35, 20, GOLD);
    DrawText(TextFormat("LIVES: %d", lives), 10, 60, 20, RAYWHITE);
    DrawText(TextFormat("STAGE: %d", stageIndex), 10, 85, 20, RAYWHITE);
}

void Renderer::drawTitleOverlay(int highScore) {
    const char* title = "BREAKOUT 3D";
    int titleSize = 48;
    int titleWidth = MeasureText(title, titleSize);
    DrawText(title, (SCREEN_WIDTH - titleWidth) / 2, SCREEN_HEIGHT / 2 - 80, titleSize, RAYWHITE);

    const char* hint = "Press ENTER to Start";
    int hintSize = 20;
    int hintWidth = MeasureText(hint, hintSize);
    DrawText(hint, (SCREEN_WIDTH - hintWidth) / 2, SCREEN_HEIGHT / 2, hintSize, GRAY);

    // タスク6：保存済みハイスコアをタイトル画面にも表示する
    const char* highScoreText = TextFormat("HIGH SCORE: %d", highScore);
    int highScoreWidth = MeasureText(highScoreText, hintSize);
    DrawText(highScoreText, (SCREEN_WIDTH - highScoreWidth) / 2, SCREEN_HEIGHT / 2 + 30, hintSize, GOLD);
}

void Renderer::drawPauseOverlay() {
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.5f));

    const char* text = "PAUSED";
    int fontSize = 40;
    int width = MeasureText(text, fontSize);
    DrawText(text, (SCREEN_WIDTH - width) / 2, SCREEN_HEIGHT / 2 - 20, fontSize, RAYWHITE);
}

void Renderer::drawGameOverOverlay(int score) {
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.6f));

    const char* text = "GAME OVER";
    int fontSize = 40;
    int width = MeasureText(text, fontSize);
    DrawText(text, (SCREEN_WIDTH - width) / 2, SCREEN_HEIGHT / 2 - 40, fontSize, RED);

    const char* scoreText = TextFormat("SCORE: %d", score);
    int scoreWidth = MeasureText(scoreText, 20);
    DrawText(scoreText, (SCREEN_WIDTH - scoreWidth) / 2, SCREEN_HEIGHT / 2 + 10, 20, RAYWHITE);

    const char* hint = "Press ENTER to Title";
    int hintWidth = MeasureText(hint, 20);
    DrawText(hint, (SCREEN_WIDTH - hintWidth) / 2, SCREEN_HEIGHT / 2 + 40, 20, GRAY);
}

void Renderer::drawClearOverlay(int stageIndex) {
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.5f));

    const char* text = TextFormat("STAGE %d CLEAR!", stageIndex);
    int fontSize = 36;
    int width = MeasureText(text, fontSize);
    DrawText(text, (SCREEN_WIDTH - width) / 2, SCREEN_HEIGHT / 2 - 20, fontSize, GREEN);

    const char* hint = "Press ENTER for Next Stage";
    int hintWidth = MeasureText(hint, 20);
    DrawText(hint, (SCREEN_WIDTH - hintWidth) / 2, SCREEN_HEIGHT / 2 + 30, 20, GRAY);
}
