#pragma once

// ブロックくずし3D 定数定義
// プレイフィールドは「奥行きのある箱」：X=左右、Y=上下、Z=奥手前
namespace Constants {

// --- プレイフィールド境界（ワールド座標） ---
constexpr float X_MIN = -8.0f;        // 左壁
constexpr float X_MAX = 8.0f;         // 右壁
constexpr float Z_MIN = -6.0f;        // 奥壁
constexpr float Z_MAX = 6.0f;         // 手前壁
constexpr float Y_MAX = 10.0f;        // 天井
constexpr float MISS_LINE_Y = -1.0f;  // これより下に落ちたらミス（壁ではない）

// --- ボール ---
constexpr float BALL_RADIUS = 0.3f;
constexpr float BALL_SPEED = 10.0f;   // 通常速度（units/sec）

// --- パドル ---
constexpr float PADDLE_SPEED = 12.0f;
constexpr float PADDLE_Y = 0.0f;            // パドル中心のY座標（固定）
constexpr float PADDLE_HALF_WIDTH = 1.5f;   // X方向の半幅
constexpr float PADDLE_HALF_HEIGHT = 0.3f;  // Y方向の半厚み
constexpr float PADDLE_HALF_DEPTH = 0.8f;   // Z方向の半厚み（奥行き反射用）

// --- ブロック ---
constexpr float BLOCK_HALF_WIDTH = 0.7f;
constexpr float BLOCK_HALF_HEIGHT = 0.35f;
// ブロックのZ方向の半厚み。
// 【設計変更・未光レビュー致命傷1対応（2周目）】
// 旧設計ではZ方向の当たり判定がZ_MIN〜Z_MAX全体（厚み12.0）を占有しており、
// 見た目の描画（main.cppのDrawCubeはZ方向1.0固定）と全く一致していなかった。
// これは実質「Z軸を無効化した2D判定」であり、3D衝突判定として成立していないという
// 指摘を受け、見た目の奥行き（1.0）と当たり判定の厚みを完全に一致させる薄い板状に変更する。
// 真子の計画で決まっている「1周目はブロック配置をXY平面に限定し、Z方向は壁・パドルのみに
// 使う簡略化」とも整合する（ブロックのZ座標は基本0固定・厚みだけ見た目と揃える）。
constexpr float BLOCK_HALF_DEPTH = 0.5f;  // 見た目の奥行き1.0（main.cppのDrawCube）と一致させた値

// --- 衝突判定システム ---
constexpr int MAX_SUBSTEPS = 8;
// パドルのオフセット±1.0で反射方向が何ラジアン曲がるか（約46度）
constexpr float PADDLE_MAX_BOUNCE_ANGLE = 0.8f;

// --- ステージ配置（②：Stage::buildBlocks） ---
// ステージ1の基本レイアウト。ステージ番号が進むごとにrows/cols/gapが変化する
// （詳細な生成ロジックはStage.cpp参照）。
constexpr int   BLOCK_BASE_ROWS = 5;   // ステージ1の基本行数
constexpr int   BLOCK_BASE_COLS = 6;   // ステージ1の基本列数
constexpr int   BLOCK_MAX_ROWS = 8;    // 行数の上限（天井Y_MAXに収まる範囲で決定）
constexpr float BLOCK_TOP_Y = 8.5f;    // 最上段ブロック中心のY座標
constexpr float BLOCK_GAP_X = 0.3f;    // ブロック同士のX方向（同じ行の隣）の隙間の初期値
constexpr float BLOCK_GAP_Y = 0.3f;    // ブロック同士のY方向（行間）の隙間の初期値
constexpr float BLOCK_GAP_Z = 0.4f;    // Z方向に2層目を配置する際の層間の隙間
constexpr float BLOCK_MIN_GAP = 0.15f; // ステージが進んでも詰まりすぎないための隙間の下限
// このステージ番号以上で列数を+1して横に広げる
constexpr int BLOCK_WIDE_STAGE_THRESHOLD = 3;
// このステージ番号以上でZ方向奥に2層目のブロック列を追加する（3D配置の導入）
constexpr int BLOCK_DEPTH_STAGE_THRESHOLD = 3;

}  // namespace Constants
