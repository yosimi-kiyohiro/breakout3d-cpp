#include "Stage.hpp"

#include <algorithm>

#include "constants.hpp"

// ②：ステージ設計
// ステージ番号(1始まり)に応じて以下を変化させる：
//   - rows/cols（行数・列数）：ステージが進むほど増える
//   - gapX/gapY（隙間）：ステージが進むほど詰まる（下限BLOCK_MIN_GAPでクランプ）
//   - durability（耐久値）：3ステージごとに+1
//   - zLayers（Z方向のレイヤー数）：BLOCK_DEPTH_STAGE_THRESHOLD以上で奥に2層目を追加
//     （3D配置の導入。パドルのZオフセット反射で奥のブロックを狙う必要が出てくる）
namespace Stage {

namespace {

struct Layout {
    int rows;
    int cols;
    float gapX;
    float gapY;
    int durability;
    int zLayers;
};

Layout layoutForStage(int stageIndex) {
    int idx = std::max(1, stageIndex);

    Layout layout;
    layout.rows = std::min(Constants::BLOCK_BASE_ROWS + (idx - 1), Constants::BLOCK_MAX_ROWS);
    layout.cols = (idx >= Constants::BLOCK_WIDE_STAGE_THRESHOLD) ? Constants::BLOCK_BASE_COLS + 1
                                                                  : Constants::BLOCK_BASE_COLS;

    // ステージが進むほど隙間を詰めて密度を上げる（下限BLOCK_MIN_GAPでクランプ）
    float shrink = 0.02f * static_cast<float>(idx - 1);
    layout.gapX = std::max(Constants::BLOCK_MIN_GAP, Constants::BLOCK_GAP_X - shrink);
    layout.gapY = std::max(Constants::BLOCK_MIN_GAP, Constants::BLOCK_GAP_Y - shrink);

    layout.durability = 1 + (idx - 1) / 3;
    layout.zLayers = (idx >= Constants::BLOCK_DEPTH_STAGE_THRESHOLD) ? 2 : 1;

    return layout;
}

// z位置に1レイヤー分のブロックを配置する。奥レイヤー（z != 0）は行数を間引いて
// 「奥から迫ってくる」印象にする
void addLayer(std::vector<Block>& blocks, const Layout& layout, float z) {
    const float halfW = Constants::BLOCK_HALF_WIDTH;
    const float halfH = Constants::BLOCK_HALF_HEIGHT;
    const float pitchX = halfW * 2.0f + layout.gapX;
    const float pitchY = halfH * 2.0f + layout.gapY;
    const float totalWidth = layout.cols * pitchX - layout.gapX;
    const float startX = -totalWidth / 2.0f + halfW;

    int rows = layout.rows;
    if (z != 0.0f) rows = std::max(2, layout.rows / 2);

    for (int row = 0; row < rows; ++row) {
        float y = Constants::BLOCK_TOP_Y - static_cast<float>(row) * pitchY;
        for (int col = 0; col < layout.cols; ++col) {
            float x = startX + static_cast<float>(col) * pitchX;
            blocks.emplace_back(Vector3{x, y, z}, halfW, halfH, layout.durability);
        }
    }
}

}  // namespace

std::vector<Block> buildBlocks(int stageIndex) {
    Layout layout = layoutForStage(stageIndex);

    std::vector<Block> blocks;
    blocks.reserve(static_cast<size_t>(layout.rows) * static_cast<size_t>(layout.cols) *
                   static_cast<size_t>(layout.zLayers));

    addLayer(blocks, layout, 0.0f);

    if (layout.zLayers >= 2) {
        float zBack = -(Constants::BLOCK_HALF_DEPTH * 2.0f + Constants::BLOCK_GAP_Z);
        addLayer(blocks, layout, zBack);
    }

    return blocks;
}

}  // namespace Stage
