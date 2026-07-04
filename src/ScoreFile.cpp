#include "ScoreFile.hpp"

#include <cstdio>
#include <fstream>

SaveData ScoreFile::load() {
    SaveData data;
    std::ifstream f(FILE_PATH);
    if (f.is_open()) {
        f >> data.highScore;
        // 読み込みに失敗した（空ファイル・壊れた内容等）場合はistreamがfail状態になり
        // data.highScoreへの書き込みが行われないことがあるため、念のため0未満を弾く
        if (!f || data.highScore < 0) data.highScore = 0;
    }
    return data;
}

void ScoreFile::save(int highScore) {
    std::ofstream f(FILE_PATH);
    if (f.is_open()) {
        f << highScore;
    } else {
        // 梨緒レビュー指摘対応：書き込み禁止ディレクトリ等で失敗した場合に無言で
        // 握りつぶさず、調査しやすいよう1行だけ警告を残す
        std::fprintf(stderr, "警告: ScoreFile::save - %s を開けませんでした。ハイスコアは保存されません。\n",
                     FILE_PATH);
    }
}
