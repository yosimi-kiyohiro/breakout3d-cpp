#pragma once
#include <vector>

#include "Block.hpp"

// ②：ステージごとのブロック配置を組み立てるモジュール。
// ステージ番号（1始まり）に応じて行数・列数・隙間・耐久値・Z方向のレイヤー数を
// 変化させる。生成ロジックの詳細はStage.cpp参照。
namespace Stage {

// stageIndexに応じたブロック配置を新規生成して返す
std::vector<Block> buildBlocks(int stageIndex);

}  // namespace Stage
