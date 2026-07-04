# Breakout3D C++ — Claude への説明書

## プロジェクト概要

「ブロックくずし」を3D空間（X/Y/Z軸）に拡張したC++/raylib学習プロジェクト。
清広さんのポートフォリオ用ゲーム開発の一環。tetris-cpp・space-invaders-cppに続く3作目。

最大の技術リスクは「3D空間でのボール⇔パドル⇔ブロック⇔壁の衝突判定と反射」が破綻しないことで、
①-Bで技術検証スパイクを実施した。

**進捗の正直な報告（3周目修正後）：** 1周目の自己採点94点に対し、未光の破壊レビューで
「ブロックの見た目（奥行き1.0）と当たり判定（Z_MIN〜Z_MAX全厚み＝12.0）が一致しておらず
実質2D判定になっている」「複合3軸軌道・真の角ヒットが一度も検証されていない」という
致命的な指摘を受け41点まで下がった。指摘を受けてブロックのZ厚みを見た目と一致させる
設計変更（BLOCK_HALF_DEPTH）、resolveBlocksのX/Y/Z 3軸判定への拡張、ブロック衝突の
位置補正の追加、TC7（真の角ヒット）・TC8（隣接ブロックの継ぎ目）・TC9（パドル→ブロック→壁
の複合軌道）の新設、TC2・TC5の複合軸への書き換えを行い、2周目時点でTC1〜TC9の
**9項目全てPASSを実測で確認した（Go判定）**と報告した。

しかし2周目の報告には過大評価があった。**未光の3周目再検証で、「TC1〜TC9は全て
z=0固定・vz=0固定のままで、ブロックのZ面（position.z ± BLOCK_HALF_DEPTHの実面）に
ボールが実際に当たるケースが一度も検証されていない」という新たな致命的な見落としが
発覚した。**正確には、2周目時点で実測でPASS確認できていたのは「X/Y面のブロック衝突・
壁の3軸複合反射・パドルのX/Z複合反射」までであり、ブロックのZ面衝突は**未検証のまま
「Go判定」としてしまっていた**。

3周目でTC10（ブロックのZ面単独ヒット・4パターン）とTC11（X/Y/Zの3軸すべてが同時に
外側判定になる真の3方向角ヒット・4パターン）を新設し、TC9のoffsetZを非ゼロに修正して
「複合軌道」という名前と実態の乖離（名前負け）も解消した。**TC1〜TC11の11項目全てPASSを
実測で確認済み**（詳細は下記進捗表・test_results.log参照）。

ただし、パドル反射のテスト（TC3・TC5b）は本番コードと同じ式を再実装した比較が残っており
（完全な独立検証ではない）、TC7（X/Y面の角ヒット）は反転軸が「ちょうど1軸だけ選ばれ
クラッシュしない」という健全性のみを検証しており「その軸の選択が幾何学的に正しいか」
までは検証していない、という2点は既知の限界として残っている（詳細は下記「既知の限界」参照）。

**②（ステージ設計）・③-A（GameState/Renderer本実装）・③-B（Web最小疎通スパイク）完了報告：**
②でTC8の既知の限界（隣接ブロックの継ぎ目は1サブステップ1ブロックのみ処理）が
実際のステージ密度で問題化するかをTC12として実測したところ、**現在の想定密度
（隙間の下限BLOCK_MIN_GAP=0.15）ではボール直径（0.6）の方が隙間より大きく、
隣接ブロックの継ぎ目にボールが同時に重なる状況が幾何学的に避けられないことが判明した**
（最初の実測：568試行中112件がボール半径を超えるオーバーラップで違反、致命的）。
このため急遽`resolveBlocks()`を「同一サブステップで重なっている生存ブロックを全て処理する」
方式に拡張した（詳細は下記「②修正：resolveBlocksの複数ブロック同時処理への拡張」参照）。
拡張後はTC12（568試行）で違反0件を確認し、TC8も新しい期待値（継ぎ目の隣接ブロックは
両方同時に割れる）に書き換えてPASSしている。**TC1〜TC12の12項目全てPASSを実測で
確認済み**（-O2最適化でも同じ結果を確認済み。詳細は下記表参照）。

③-AではGameState（GamePhase: Title/Playing/Paused/GameOver/Clear）とRenderer
（raylib描画ラップ・CAMERA_CUSTOM固定カメラ）を実装し、main.cppをtetris-cpp方式
（静的グローバル＋`UpdateDrawFrame()`＋`PLATFORM_WEB`分岐）に全面書き換えた。
ネイティブビルドの起動を実測確認済み（詳細は下記）。

③-Bでは`build_web.sh`（v0草案）・`web/shell.html`を新規作成し、emscriptenでのビルドに
成功、`emrun`のローカルサーバー経由でindex.html・index.js・index.wasmがHTTP 200で
正しく配信されることを確認した（ブラウザでの目視によるレンダリング確認は本環境では
実施できていない。詳細は下記「③-B Webビルド」参照）。

③-Cでは梨緒のコードレビュー指摘4件（HIGH2・LOW1・軽微1）を修正し、TC13（2x2隣接
4ブロック同時重なり）を新設した。**TC1〜TC13の13項目全てPASSを実測で確認済み**
（詳細は下記「③-C：梨緒レビュー対応」参照）。

## ビルド環境（重要）

- **コンパイラ**：w64devkit（MinGW-GCC）— `C:\w64devkit\w64devkit\bin\g++.exe`
- **ライブラリ**：raylib 6.0 MinGW版 — `C:\raylib\raylib-6.0_win64_mingw-w64\`
- **CMake・vcpkg は使用しない**（tetris-cpp・space-invaders-cppと同じ方針）

⚠️ **raylib.h は `PI` `DEG2RAD` `RAD2DEG` をマクロ定義している。** 自前でこれらの名前の変数・定数を
宣言すると展開されてコンパイルエラーになるため、別名（`MATH_PI`など）を使うこと。

ビルドコマンド：
```bash
cd /Users/nimam/Projects/breakout3d-cpp
sh build.sh          # ネイティブ確認（breakout3d.exe・raylibウィンドウ起動）
sh build_test.sh      # 衝突判定ヘッドレステスト（test_collision.exe・ウィンドウなし）
sh build_web.sh       # Webビルド（emscripten・web/index.html等を生成。v0草案）
```

新しい .cpp を追加したら、build.sh・build_test.sh・build_web.sh 全てのビルドコマンドに追記すること。

Webビルドについて：`emsdk`と`Web用raylib静的lib`（libraylib.web.a）はtetris-cppプロジェクトと
共用しており、`build_web.sh`は自スクリプトからの相対パス（`../tetris-cpp/emsdk`・
`../raylib-src/src`）で解決している。breakout3d-cpp・tetris-cpp・raylib-srcが
`C:\Users\nimam\Projects\`直下の兄弟ディレクトリであることが前提。

## 現在の進捗

| ステップ | 内容 | 状態 |
|---|---|---|
| ①-A | 環境構築（プロジェクト作成・build.sh・空raylibウィンドウ） | ✅ 完了 |
| ①-B（1周目） | 3D衝突判定スパイク（CollisionSystem・TC1〜TC6ヘッドレステスト） | ✅ 完了（6/6 PASS）→ 未光の破壊レビューで41点。ブロックZ厚み不一致・複合3軸未検証が発覚 |
| ①-B（2周目・修正） | ブロックZ厚み設計修正・resolveBlocks 3軸化・位置補正追加・TC2/TC5書き換え・TC7〜TC9新設 | ⚠️ 完了報告（TC1〜TC9 9/9 PASS・Go判定）→ ブロックのZ面衝突が未検証のまま報告していたことが3周目で判明 |
| ①-B（3周目・修正） | TC10（ブロックZ面単独ヒット）・TC11（X/Y/Z3軸同時角ヒット）新設・TC9のoffsetZ修正・AGENTS.md記述の是正 | ✅ 完了（TC1〜TC11 11/11 PASS・test_results.log記録済み・Go判定） |
| ②（ステージ設計＋残課題対応） | Stage::buildBlocks新設・-O2実測・TC12新設（複数ブロック同時ヒットのすり抜け実測）・resolveBlocks拡張 | ✅ 完了（TC1〜TC12 12/12 PASS・-O2でも12/12 PASS） |
| ③-A（GameState/Renderer本実装） | GamePhase状態遷移・Renderer描画ラップ・main.cpp全面書き換え（tetris-cpp方式） | ✅ 完了（ネイティブビルド成功・起動実測確認済み） |
| ③-B（Web最小疎通スパイク） | build_web.sh（v0草案）・web/shell.html新規作成・emscriptenビルド | ✅ 完了（ビルド成功・emrunでHTTP 200配信を確認。ブラウザ目視は未実施） |
| ③-C（梨緒レビュー対応） | GameState::update()のクリア/ミス優先順位バグ修正・.gitignore漏れ修正・TC13新設（2x2隣接4ブロック同時重なり）・CollisionSystemのミス確定後break追加・GameState/Renderer既存実装の要件充足確認 | ✅ 完了（TC1〜TC13 13/13 PASS・詳細は下記「③-C：梨緒レビュー対応」参照） |
| ③-D（修正1〜3：スコアバグ修正・副産物ログ整理・TC13の4方向拡張） | StepResultにblockHitCount追加・GameState::update()のスコア加算を破壊数比例に修正・.gitignoreに副産物ログ3件追加・TC13を4方向（右上/左上/右下/左下）に拡張 | ✅ 完了（TC1〜TC13 13/13 PASS・詳細は下記「③-D：修正1〜3」参照） |
| ④（⑥⑦⑧：パーティクル・音声・ハイスコア） | ParticleSystem（3D版）・AudioManager（raylib版コード生成音声）・ScoreFile（ハイスコア永続化）新規実装 | ✅ 完了（ネイティブビルド成功・起動実測確認済み・詳細は下記「④：⑥⑦⑧の実装」参照） |
| ④（⑨：見た目の仕上げ） | ブロック耐久値3段階の色分け・ボールのワイヤーフレーム輪郭・HUD/タイトル画面へのハイスコア表示追加 | ✅ 完了（カメラアングル自体は変更なし。詳細は下記「④：⑨見た目の仕上げ」参照） |
| ⑩ | Web版本番ビルド | ⬜（今回のスコープ外。build_web.shにparticles/audio/scorefileは追記済みだが実ビルド・ブラウザ目視は未実施） |
| ⑪ | CI（GitHub Actions） | ⬜（今回のスコープ外） |
| ⑫ | README最終化 | ⬜（今回のスコープ外） |

## 採用した衝突判定設計（変更不可・変更時は要相談）

3案（A:AABBめり込み比較 / B:CCDレイキャスト / C:box-sphere+サブステップのハイブリッド）を比較し、
**C案を採用**。理由：raylibの`CheckCollisionBoxSphere`を使うため実装がシンプルで、
`numSubSteps = clamp(ceil(|v|*dt/radius), 1, 8)` によるサブステップ分割で高速球のすり抜けも防げるため。
ブロックは前フレーム位置基準でX面/Y面/Z面を判定する方式（ちょうど1軸だけが外側判定の場合は
その軸を採用、2軸以上が同時に外側＝真の角ヒット・稜線ヒットの場合のみめり込み量最小の軸を
優先するフォールバック）。壁・パドルと同様、ヒット後はヒット面の外側へ位置を補正する。

**【2周目修正：ブロックのZ厚み設計変更（未光致命傷1対応）】**
旧設計はブロックのZ方向当たり判定がZ_MIN〜Z_MAX全体（厚み12.0）を占有しており、
見た目の描画（DrawCubeのZサイズ1.0固定）と全く一致していなかった。これは実質
「Z軸を無効化した2D判定」であり3D衝突判定として成立していないという指摘を受け、
`Constants::BLOCK_HALF_DEPTH`（0.5＝見た目の奥行き1.0と一致）を新設し、
`Block::getBoundingBox()`のZ範囲を`position.z ± halfDepth`に変更した。
これにより、resolveBlocksはX面/Y面だけで良い設計から**X/Y/Z 3軸を見る本来の3D判定**に
拡張する必要が生じ、実際に3軸化した（TC7で角ヒットのフォールバック分岐を実測検証済み）。
真子の計画で決まっている「1周目はブロック配置をXY平面に限定し、Z方向は壁・パドルのみに
使う簡略化」方針とも矛盾しない（ブロックのZ座標は基本0固定・厚みだけ見た目と揃えた）。

**【3周目修正：ブロックのZ面衝突が未検証だった問題への対応】**
2周目はZ厚みの「設計変更」自体は行ったが、そのZ面に実際にボールを当てるテストを
一度も書いておらず、「設計は直したがテストしていない」状態のままGo判定を出していた。
TC10でZ面単独ヒット（vzの反転・snapOutsideAxisによるZ軸位置補正）を、TC11でX/Y/Zの
3軸が同時に外側判定になる真の立体角ヒットを実測し、ようやく3軸判定の全体が
実際に動作することを確認できた。

**【②修正：resolveBlocksの複数ブロック同時処理への拡張】**
②でステージのブロック密度（`Stage::buildBlocks`の隙間・行列数）を決めた際、
TC8で記録していた「1サブステップ1ブロックのみ処理」という既知の限界が、
実際の密度で視認可能な貫通を引き起こすかをTC12として実測した。ステージの
隙間の下限`BLOCK_MIN_GAP`（0.15）はボール直径（`BALL_RADIUS`×2＝0.6）より
狭く、隣接ブロックの継ぎ目ではボールが両方に同時に重なる状況が幾何学的に
避けられない。この状態で1ブロックしか処理しないと、割れなかった方の
ブロックにボールが深くめり込んだまま（実測：オーバーラップ最大0.45、
ボール半径0.3を超過）次のサブステップまで持ち越され、見た目上貫通して見える。
このため`resolveBlocks()`を「同一サブステップ内で現在ボールに重なっている
生存中の全ブロック」を処理する方式に拡張した。実装のポイントは以下の2点：
1. 速度反転は軸（X/Y/Z）ごとに最大1回のみ（`flippedX/Y/Z`で管理）。同じ軸を
   複数ブロックが要求しても二重反転で相殺しないようにする。
2. ブロックの検出・軸判定・フォールバックの重なり量計算は、このサブステップ
   開始時点の位置（`entryPos`）に固定して行う。最初に処理したブロックの
   位置スナップで`ball.position`が動いた後の値を基準に次のブロックを判定すると、
   境界の浮動小数点誤差で「衝突なし」と誤判定されることがあった
   （TC8書き換え時に実際に発生し、`entryPos`固定で解消を確認）。
TC8は新しい期待挙動（継ぎ目の隣接ブロックは両方同時に割れる・Y軸反転は1回のみ）
を検証するテストに書き換え、TC12は568試行で違反0件（PASS）を確認した。

## ②：ステージ設計（Stage::buildBlocks）

`src/Stage.hpp/.cpp`が`Stage::buildBlocks(int stageIndex)`でステージごとの
ブロック配置を生成する。ステージ番号（1始まり）に応じて変化する要素：

| 要素 | 変化ルール |
|---|---|
| 行数（rows） | `BLOCK_BASE_ROWS`(5) + (stageIndex-1)。`BLOCK_MAX_ROWS`(8)で頭打ち |
| 列数（cols） | `BLOCK_WIDE_STAGE_THRESHOLD`(3)以上で`BLOCK_BASE_COLS`(6)+1＝7列に拡張 |
| 隙間（gapX/gapY） | ステージが進むごとに0.02ずつ詰まる。`BLOCK_MIN_GAP`(0.15)で下限クランプ |
| 耐久値（durability） | `1 + (stageIndex-1)/3`（3ステージごとに+1） |
| Zレイヤー数 | `BLOCK_DEPTH_STAGE_THRESHOLD`(3)以上で奥（-Z方向）に2層目を追加（3D配置の導入） |

奥レイヤーは`Constants::BLOCK_HALF_DEPTH*2 + Constants::BLOCK_GAP_Z`だけ手前レイヤー
より奥（-Z方向）に配置され、行数は手前レイヤーの半分（最低2行）に間引かれる
（「奥から迫ってくる」印象を出す狙い）。パドルのZオフセット反射（`resolvePaddle`の
`offsetZ`）で奥のブロックを狙う操作性が生まれる設計。

**-O2最適化の実測結果：** `test_collision.cpp`（TC1〜TC12）を`-O2`付きで実際に
ビルド・実行し、最適化なし（デフォルト）と同じ12/12 PASSを確認した。
主要な等価判定（`ball.velocity.x == -velocity.x`等）はIEEE 754の符号反転
（`-x`）のみに依存しており、`-ffast-math`等を使わない標準的な`-O2`では
丸め方が変わらないため、崩れなかったと考えられる。

## ③-A：GameState / Renderer / main.cpp

- `src/GameState.hpp/.cpp`：`enum class GamePhase { Title, Playing, Paused, GameOver, Clear }`。
  スコア・ライフ（初期3）・ステージ番号・Ball/Paddle/`std::vector<Block>`を保持し、
  `update(dt)`・`handleInput(dt)`・`resetStage(stageIndex)`・`loseLife()`・
  `advanceStage()`・`checkStageClear()`・`checkGameOver()`を実装。ボールはミス後・
  ステージ開始後にパドル上の定位置へ自動配置され、速さ`BALL_SPEED`固定のやや斜め上
  方向へ自動発射される（パドル上で待機してから発射する演出は④以降の検討事項）。
- `src/Renderer.hpp/.cpp`：`shouldClose()`・`beginFrame()/endFrame()`・
  `beginScene3D()/endScene3D()`（`BeginMode3D`/`EndMode3D`のラップ）・
  `drawField()`・`drawPaddle()`・`drawBall()`・`drawBlocks()`・`drawHud()`と
  フェーズ別オーバーレイ（`drawTitleOverlay()`等）を実装。カメラは①-Bスパイクと
  同じ俯瞰角度（position={0,14,20}・target={0,4,0}）のCAMERA_CUSTOM固定。
- `src/main.cpp`：tetris-cpp方式（静的グローバル`g_renderer`/`g_game`＋
  `UpdateDrawFrame()`関数＋`#if defined(PLATFORM_WEB)`分岐）に全面書き換え。
- ネイティブビルド（`sh build.sh`相当のコマンド）に成功し、起動して数秒間
  クラッシュなく動作すること（raylib初期化ログ・ウィンドウ生成まで）を実測確認した。
  タイトル→プレイ→クリア/ゲームオーバーの実際のプレイ操作による目視確認は
  今回は実施していない（自動テストで状態遷移ロジックの妥当性は保証できないため、
  ④以降でのプレイテストを推奨）。

## ③-B：Webビルド（emscripten）v0草案

- `build_web.sh`：現時点のソース一式（main.cpp・GameState・Renderer・Ball・Paddle・
  Block・CollisionSystem・Stage）を`em++`でリンクし`web/index.html`等を生成する。
  `EMSDK_DIR`・`RAYLIB_SRC_DIR`は自スクリプトからの相対パス
  （`$(dirname "$0")/../tetris-cpp/emsdk`・`$(dirname "$0")/../raylib-src/src`）で
  解決しており、breakout3d-cpp・tetris-cpp・raylib-srcが`Projects/`直下の
  兄弟ディレクトリであることに依存する（他プロジェクトへの絶対パスハードコードは
  避けた）。
- `web/shell.html`：tetris-cppの`scripts/shell.html`を土台に、タイトルを
  「Breakout 3D」・キャンバスサイズを1024×768に変更したもの。
- ビルドは成功し、`web/index.html`・`index.js`・`index.wasm`が生成されることを
  確認した。`emrun --no_browser`でローカルサーバーを起動し、`curl`で
  `index.html`・`index.js`・`index.wasm`のいずれもHTTP 200で正しく配信される
  ことを確認した。**ブラウザでの実際のレンダリング（画面が映るか・操作できるか）
  の目視確認は本環境（ヘッドレスのCLI環境）では実施できていない**ため、
  次回作業時に清広さんの環境で`emrun web/index.html`を実行しての目視確認を推奨する。
- `.gitignore`を`web/` → `web/* ＋ !web/shell.html`に変更し、ビルド成果物
  （index.html/js/wasm/data）は除外しつつ手書きソースの`shell.html`はコミット
  対象に含めるようにした。

## ③-C：梨緒レビュー対応（HIGH2件・LOW1件・軽微1件の修正＋TC13新設）

③-A・③-Bの完了後、梨緒のコードレビューで指摘された4件を修正し、新規テスト
TC13を1件追加した。

**【HIGH】GameState::update()のステージクリア/ミス優先順位バグ修正：**
`CollisionSystem::update()`は1フレーム内で最大8サブステップ回るため、「最後の
ブロックを割った直後、同じフレーム内でミスラインを越える」ケースが起こりえた。
旧実装は`r.missed`を`checkStageClear()`より先に評価しており、この場合
本来ステージクリアのはずが`loseLife()`が呼ばれてreturnしてしまい、クリア判定に
到達しないバグがあった（クリアのはずがライフが減る・最悪その場でGameOverになる）。
`checkStageClear()`を`r.missed`より先に評価するよう順序を入れ替え、「クリアと
ミスが同時に起きた場合はクリアを優先する」という仕様にした（`src/GameState.cpp`
`GameState::update(float dt)`）。

**【HIGH】.gitignoreのビルドログ漏れ修正：**
`build_native.log`・`emrun.log`が`.gitignore`に含まれておらず、動作確認時に
生成したログファイルが誤ってコミット対象になりうる状態だった。既存の
`test_results.log`除外の直後に追記した。

**【LOW】TC13新設：2x2隣接4ブロック同時重なりの検証：**
現在のブロック密度（`BLOCK_MIN_GAP`=0.15・`BALL_RADIUS`=0.3）では、2x2に隣接
配置した4ブロックの隙間の交差点（十字の中心）付近でボールが4ブロック全てに
同時に重なる状況が幾何学的に起こりうる（TC12は2ブロック隣接・1方向までしか
検証していなかった）。`src/test_collision.cpp`にTC13を新設し、4ブロックを
`Constants::BLOCK_HALF_WIDTH`・`BLOCK_HALF_HEIGHT`・`BLOCK_MIN_GAP`から算出した
実際のステージ密度と同じ間隔で2x2配置し、十字の中心（4ブロック全てに重なる点、
実測距離約0.106<半径0.3）へボールを1サブステップ（steps==1）で到達させて実測した。

**実測で判明したテスト設計上の問題と修正：**
1. 当初ブロック群をY=0付近（十字の中心が原点になるよう）に配置したところ、
   パドル（`PADDLE_Y`=0・`PADDLE_HALF_HEIGHT`=0.3）の当たり判定と重なってしまい、
   `resolveBlocks`より先に`resolvePaddle`が割り込んでボールを弾き飛ばし、本来
   検証したい4ブロック同時ヒットが正しく実測できていなかった（TL/TRのみ検出、
   BL/BRが未検出という誤った結果になった）。ブロック群を他のTC（TC6・TC7・
   TC10・TC11）と同じY=7付近に移動して解消した。
2. 反転軸の検証ロジックに`(ball.velocity.z == -velocity.z)`という式をそのまま
   使ったところ、本テストは元々velocity.zが常に0のため、IEEE754上
   `0.0f == -0.0f`が真になり「Z軸が反転した」という誤検出が発生した。TC6と
   同じガード（`(velocity.z != 0.0f) && (...)`）を追加して解消した。

**実測結果（修正後）：**
- ターゲット位置(十字の中心)で4ブロック全てにボールが重なることを事前確認：4/4 OK
- サブステップ=1（想定通り）、4ブロック全てが`hit()`され`alive=false`になった
- 速度反転はちょうど1軸のみ（Y軸）。二重反転・無反転は発生しなかった
- 生存ブロックへのめり込み：0件（4ブロックとも同時に破壊されるため対象なし）
- 最終`ball.position`=(-0.0000, 7.2250, 0.0000)で、2x2クラスタ+余裕の範囲内に収まり異常値なし

**興味深い実測上の発見（設計変更は不要と判断）：**
理論上はX面・Y面どちらの重なり量も同じ値（`r - gap/2` = 0.3-0.075 = 0.225）に
なるはずだが、`cx - halfWidth`と`cy - halfHeight`の計算過程での浮動小数点丸め
誤差の違いにより、実測ではごくわずかな差でY軸側が選ばれた（理論上のタイが
浮動小数点演算で崩れた形）。4ブロックとも同じ軸（Y）で異なるスナップ値を
要求し合うため、`snapOutsideAxis`が後から処理したブロックの値で前のブロックの
スナップ結果を上書きする（最終位置は`blocks`ベクタの処理順で決まる）ことを
実測で確認した。ただし本ケース（4ブロックとも耐久値1で同時破壊）では上書きが
起きても生存ブロックへのめり込みという実害には至らないことも確認できた。
**resolveBlocksの改修は不要と判断した**（詳細な理由・残る限界は下記
「既知の限界」参照）。

**【軽微】CollisionSystem::update()のミス確定後の無駄なサブステップ継続を修正：**
`result.missed`が立ってもforループがbreakせず残りのサブステップを回し続けて
いた。実害はほぼないが（GameState側でそのフレームの結果は`loseLife()`後に
破棄される）、無駄な計算を避けるため`if (result.missed) break;`を追加した
（`src/CollisionSystem.cpp`）。

**タスク5・6（HUD/オーバーレイ・フェーズ遷移）の確認結果：**
依頼文では`drawHUD(const GameState&)`・`drawTitleScreen()`等の名前が指定されて
いたが、既存実装は`drawHud`・`drawTitleOverlay`等の名前・シグネチャで**既に
要件を満たして実装済み**だったため、見た目を合わせるためだけの書き換えは
行わなかった。具体的に確認した内容：
- `Renderer::drawHud()`は`main.cpp`で`endScene3D()`の後（`BeginMode3D()`の外側）
  に呼ばれており、Playing中常時表示されている
- `drawTitleOverlay()`・`drawPauseOverlay()`・`drawGameOverOverlay()`・
  `drawClearOverlay()`は`main.cpp`の`switch (g_game->phase())`でフェーズごとに
  正しく呼び分けられている
- `GameState::handleInput()`は既に、Title→ENTER/SPACEでscore/lives/stage初期化
  しPlaying遷移、GameOver→ENTER/SPACEでTitleへ、Clear→ENTER/SPACEで
  `advanceStage()`後Playingへ、をすべて実装済み。追加実装は不要だった。

## ③-D：修正1〜3（スコア加算バグ・副産物ログ整理・TC13の4方向拡張）

**【修正1】複数ブロック同時破壊時のスコア加算バグ：**
`CollisionSystem::resolveBlocks()`は同一サブステップで複数の生存ブロックに
同時に重なっていれば全て処理する（②で対応済み）が、`StepResult`は
`hitBlock`（bool）しか持っておらず、`GameState::update()`側は
`if (r.hitBlock) score_ += 10;`という固定加算だった。そのため2〜4ブロックが
同時に破壊されても常に10点しか入らず、TC8（2ブロック同時）・TC13（4ブロック
同時）のようなケースでスコアが過小評価されるバグがあった。`StepResult`に
`int blockHitCount`を追加し、`resolveBlocks()`内でこのサブステップで
`block.hit()`を呼んだ回数を数えて`result.blockHitCount += hitsThisSubstep;`と
加算するようにした（サブステップをまたいで`+=`で積算するため、1フレーム内で
複数サブステップにわたって別々のブロックにヒットしても正しく合計される）。
`GameState::update()`側は`score_ += 10 * r.blockHitCount;`に修正した。
回帰防止のため、TC8（期待値2）・TC13（期待値4、4パターン全て）に
`blockHitCount`の実測チェックを追加した。

**【修正2】副産物ログファイルの.gitignore対応：**
`build_test_err.log`・`build_test_output.log`・`run_native.log`が
`.gitignore`に含まれておらず、`git status`で未追跡ファイルとして表示されて
いた。`build_native.log`・`emrun.log`と同じ「実行するたびに再生成される
ビルド・動作確認ログ」の扱いとして`.gitignore`に追記した。

**【修正3】TC13を4方向に拡張：**
③-C時点のTC13は「十字の中心よりわずかにTR（右上）側へ寄った位置から中心へ
向かう」1方向のみの検証だった。TC7/TC10/TC11と同じ考え方で、右上・左上・
右下・左下の4方向から十字の中心（完全対称点）へ接近するトライアルに拡張し、
どの方向から接近しても「4ブロック全て破壊・速度反転はちょうど1軸のみ・
生存ブロックへのめり込みが残らない・座標が異常値にならない」ことを実測した
（4/4 PASS。詳細は下記「衝突判定テストケース一覧」・実測ログ参照）。

## ④：⑥⑦⑧の実装（パーティクル・音声・ハイスコア）

**⑥ ParticleSystem（`src/ParticleSystem.hpp/.cpp`新規）：**
tetris-cppの`ParticleSystem`（2Dピクセル座標・`emit(int col, int row, Color)`）
を、本プロジェクトのワールド座標（`Vector3`）に適合させた3D版として新規作成した。
`emit(Vector3 pos, Color color)`で単位球面上の一様乱数方向に破片（`DrawCube`の
極小立方体）を10個飛び散らせ、`update(float dt)`で重力落下・寿命減衰・
自動削除を行い、`draw()`で寿命に応じたフェードアウト（アルファ値を線形減衰）
付きで描画する。`GameState::update()`が衝突判定の前後でブロックの
`alive`フラグの変化を検出し（`aliveBefore`・`durabilityBefore`を記録して
比較）、破壊されたブロックの位置・色（耐久値>1ならMAROON系、それ以外は
ORANGE系。`Renderer::drawBlocks`の配色ルールと合わせた）を
`consumeParticleEmits()`で外部へ渡す設計にした。`GameState`自体は
`ParticleSystem`クラスに依存させず（疎結合を保つ）、`main.cpp`が両方を
知っていて橋渡しする。

**⑦ AudioManager（`src/AudioManager.hpp/.cpp`新規）：**
space-invaders-cppの`AudioManager`（SDL3のpush型`AudioStream`でサイン波・
矩形波減衰PCMを生成する方式）の考え方を、raylibの`Wave`/`Sound` API向けに
書き換えた。`std::vector<int16_t>`でPCMサンプルを生成し、`Wave`構造体を
組み立てて`LoadSoundFromWave()`でraylibの`Sound`にする。`playHit()`
（660Hzサイン波・50ms）・`playBreak()`（300→80Hzの矩形波減衰・180ms）・
`playMiss()`（180→60Hzの矩形波減衰・350ms）・`playBgmLoop()`/`stopBgm()`
（220Hzサイン波・1秒を`IsSoundPlaying()`で鳴り終わりを検知して`update(dt)`
から再トリガーする手動ループ方式。raylibの`Sound`にはネイティブのループ
機構が無いため、シームレスではないが簡易ループとして許容する）を実装した。
Webの自動再生ポリシー対策として`initAfterUnlock()`を用意し、
`#if !defined(PLATFORM_WEB)`でネイティブは起動直後に初期化、Web版は
`main.cpp`がタイトル画面での最初のENTER/SPACEキー入力を検知して呼び出す
（`GameState::handleInput()`でフェーズがPlayingへ遷移する前に判定する
必要があるため、`handleInput()`より先にチェックする実装にした）。
`GameState`はここでも`AudioManager`クラス自体には依存せず、`consumeAudioEvents()`
で`hit`/`blockBreak`/`miss`の3種のフラグを`main.cpp`へ渡す設計にした。

**⑧ ScoreFile（`src/ScoreFile.hpp/.cpp`新規）：**
tetris-cpp・space-invaders-cppの`ScoreFile::load()`/`save()`という
静的メソッドのみのシンプルなパターンを踏襲した。`highscore.txt`
（`.gitignore`に既存の「セーブデータ」エントリとして登録済み）へ
整数1個を読み書きするだけの最小実装。**Web版でも無条件でファイルI/Oを
呼ぶ方式を採用した**（tetris-cpp・space-invaders-cppともWeb版を無効化する
分岐コードを書いておらず、emscriptenの仮想ファイルシステム(MEMFS)上に
そのまま読み書きしている。本プロジェクトもこの方式を踏襲する）。
この結果、**ネイティブ版はhighscore.txtがディスクに残り永続化されるが、
Web版はページをリロードするとMEMFSが初期化され直すためハイスコアが
リセットされる、という挙動差が生まれる**。これは既知の仕様上の割り切り
として受け入れる（tetris-cpp・space-invaders-cppと同じ割り切り）。
`GameState`はコンストラクタで`ScoreFile::load()`を呼んで`highScore_`を
復元し、`updateHighScoreIfNeeded()`を`loseLife()`のGameOver確定時と
`update()`のステージクリア確定時という「プレイの区切り」のタイミングで
呼んでいる（毎フレーム呼ぶとファイルI/Oが頻発するための判断）。
**既知の限界：** ウィンドウを閉じる等で不正終了した場合、その回の
最高得点が区切りタイミングに到達する前だと保存されない（未検証・未対応）。

**梨緒（rio）のコードレビューで見つかったHIGH2件・LOW2件の修正：**
実装後に梨緒へレビューを依頼したところ、HIGH2件が見つかり、その場で修正した。
- **【HIGH】blockHitCountが「破壊数」ではなく「当たった回数」になっていた**：
  `resolveBlocks()`は`block.hit()`を呼んだ回数をそのまま加算していたため、
  耐久値2以上のブロックを1回叩いただけ（まだ生存）でも加点されてしまう
  仕様になっていた。修正1の目的（同時破壊時の過小評価是正）そのものは
  正しく動いていたが、コメントで謳っていた「破壊数に応じて加算」とは
  ズレた実装だった。`resolveBlocks()`側を「`block.hit()`後に`alive`が
  falseになった回数」だけを数えるよう修正し、耐久ブロックを壊し切った
  瞬間にだけ加点される仕様に揃えた。
- **【HIGH】パーティクル色が耐久値の高いブロックでも常にORANGE固定になっていた**：
  `GameState::update()`は「壊れる直前のdurability」で色を選んでいたが、
  耐久値2以上のブロックが壊れるのは必ずdurability 1→0のタイミングのため、
  この値は破壊時には常に1になり、`Renderer::drawBlocks()`の3段階配色
  （耐久値1/2/3以上）と対応しないバグだった。`Block`に`initialDurability`
  （生成時の耐久値。`hit()`で変化しない）を追加し、GameStateはこちらを
  参照するよう修正した。
- **【LOW】ParticleSystemの乱数が起動のたびに同じ列になっていた**：
  `srand()`が一度も呼ばれていなかったため修正。`main.cpp`の`main()`冒頭で
  `srand(static_cast<unsigned int>(time(nullptr)))`を1回呼ぶようにした。
- **【LOW】ScoreFile::save()の書き込み失敗が無言で握りつぶされていた**：
  `ofstream`が開けなかった場合に`stderr`へ警告を1行出すよう修正した。

## ④：⑨見た目の仕上げ

⑤〜⑧のプレイテスト（ヘッドレステスト・ネイティブ起動確認）を踏まえ、
以下の軽量な調整のみを行った（`DrawCube`/`DrawSphere`中心の描画に色・
質感を加える程度に留め、シェーダー等の重い演出は追加していない）：
- `Renderer::drawBlocks()`：耐久値3段階の色分け（1=ORANGE、2=MAROON、
  3以上=DARKPURPLE）。ステージが進むにつれて耐久値が上がっていくことを
  プレイヤーが色で把握できるようにした（旧実装は1と2以上の2段階のみ）。
- `Renderer::drawBall()`：黒背景に対してボールの視認性を上げるため、
  `DrawSphereWires()`で輪郭線を追加（既存の`DrawCubeWires`と同じ発想の
  軽量な追加。ポリゴン数の増加はごく僅か）。
- `Renderer::drawHud()`・`drawTitleOverlay()`：⑧で永続化したハイスコアを
  画面に表示（せっかく保存しても表示されないと意味が無いため）。

**固定カメラアングル（③-Aで決定済み・position={0,14,20}・target={0,4,0}）
自体は変更していない。** ヘッドレス環境での数秒間の起動確認では見た目の
問題を判断できず、AGENTS.mdの設計の決定事項にも「変更時は要相談」と
明記されているため、実プレイでの目視確認（清広さんの環境）を経てから
判断すべきと考え、今回は据え置いた。

## ファイル構成

```
breakout3d-cpp/
├── src/
│   ├── constants.hpp        プレイフィールド境界・速度・パドル/ブロック/ステージ配置の定数
│   ├── Ball.hpp/.cpp        ボール（位置・速度・半径）
│   ├── Paddle.hpp/.cpp      パドル（X軸移動のみ・Z方向に厚みあり）
│   ├── Block.hpp/.cpp       ブロック（XY平面配置＋②で奥レイヤー追加・Z方向は見た目と一致する薄い板＝BLOCK_HALF_DEPTH）
│   ├── CollisionSystem.hpp/.cpp  衝突判定・反射ロジック（採用案Cを実装・ブロックはX/Y/Z 3軸判定・②で同一サブステップ複数ブロック処理に拡張）
│   ├── Stage.hpp/.cpp       ②新設：ステージ番号に応じたブロック配置の生成（Stage::buildBlocks）
│   ├── GameState.hpp/.cpp   ③-A新設：GamePhase状態管理・スコア/ライフ/ステージ進行（④でパーティクル/音声イベントの発行・ハイスコア更新を追加）
│   ├── Renderer.hpp/.cpp    ③-A新設：raylib描画ラップ（CAMERA_CUSTOM固定カメラ。④でハイスコア表示・ブロック3段階配色・ボール輪郭を追加）
│   ├── ParticleSystem.hpp/.cpp ④新設：ブロック破壊時の3Dパーティクル演出（emit(Vector3, Color)）
│   ├── AudioManager.hpp/.cpp   ④新設：raylibのWave/SoundでPCM生成する効果音・BGM（initAfterUnlock()でWeb自動再生対策）
│   ├── ScoreFile.hpp/.cpp      ④新設：ハイスコアのファイル永続化（highscore.txt）
│   ├── main.cpp             ③-Aで全面書き換え：tetris-cpp方式（静的グローバル＋UpdateDrawFrame＋PLATFORM_WEB分岐）。④でParticleSystem/AudioManagerの生成・イベント消費を追加
│   └── test_collision.cpp   衝突判定ヘッドレステスト（TC1〜TC13・ウィンドウを開かない・test_results.logへも出力）
├── web/
│   └── shell.html            ③-B新設：Webビルド用シェル（ビルド成果物のindex.html等は.gitignore対象）
├── build.sh                 ネイティブビルド（breakout3d.exe）
├── build_test.sh             衝突判定テストビルド（test_collision.exe）
├── build_web.sh              ③-B新設：Webビルド（emscripten・v0草案）
├── AGENTS.md                 このファイル
└── .gitignore
```

## 設計の決定事項（変更不可）

| 項目 | 決定内容 |
|---|---|
| プレイフィールド | X_MIN/X_MAX・Z_MIN/Z_MAX・Y_MAX（天井）で囲まれた箱。床の壁はなく、MISS_LINE_Yを下回ったらミス |
| パドル反射 | ヒット位置のX/Zオフセット（±1.0にクランプ）に応じて反射方向を決定。速さは常にBALL_SPEEDに再正規化 |
| ブロック当たり判定 | Z方向は見た目の奥行き（1.0）と一致する薄い板（BLOCK_HALF_DEPTH=0.5）。X/Y/Z 3面すべてを見る判定（2周目修正・旧設計のZ_MIN〜Z_MAX全厚みは廃止） |
| ブロック衝突後の位置補正 | 壁・パドルと同様、ヒット面の外側へball.positionをスナップする（2周目修正で追加） |
| ブロックの同時ヒット処理 | 同一サブステップ内で現在ボールに重なっている生存ブロックは全て処理する（②修正・旧仕様は1ブロックのみ処理でTC12の実測で貫通が判明したため拡張） |
| サブステップ数 | `min(8, max(1, ceil(|v|*dt/radius)))`（constants.hppのMAX_SUBSTEPS=8） |
| カメラ | CAMERA_CUSTOM固定。可動カメラは今回のスパイクでは扱わない |
| ステージ配置 | `Stage::buildBlocks(stageIndex)`で行数・列数・隙間・耐久値・Zレイヤー数がステージ番号に応じて変化（②新設。詳細は上記「②：ステージ設計」参照） |

## 衝突判定テストケース一覧（TC1〜TC13・test_collision.cpp）

| # | 内容 | 実測結果 |
|---|---|---|
| TC1 | 垂直落下（中央発射・50往復） | PASS |
| TC2 | 真の3軸複合斜め反射＋部屋の角への同時衝突（速度保存検証） | PASS |
| TC3 | パドル端ヒット（オフセット±0.9・20回） | PASS |
| TC4 | 高速球（通常の5倍・すり抜け検証） | PASS |
| TC5 | (a)Z壁単軸反転（正直な名称） + (b)パドルX+Z複合オフセット反射 | PASS |
| TC6 | ブロック四隅ギリギリ（前フレーム位置基準のX/Y面判定） | PASS |
| TC7（新設） | 真の角ヒット（X面・Y面が両方「外側」判定の対角アプローチ・健全性のみ検証） | PASS |
| TC8（②修正で書き換え） | 隣接ブロックの継ぎ目への同時ヒット（両方同時に割れる・Y軸反転は1回のみであることを検証） | PASS |
| TC9（3周目修正） | パドル→ブロック→壁を跨ぐ複合軌道（offsetZ導入・vz非ゼロを実測・速度保存・すり抜け検証） | PASS |
| TC10（新設・3周目） | ブロックのZ面単独ヒット（X/Y面は外側判定されない軌道・4パターン・vz反転＋位置補正の実測） | PASS |
| TC11（新設・3周目） | X・Y・Zの3軸すべてが同時に外側判定になる真の3方向角ヒット（4パターン・フォールバック優先順位の実測） | PASS |
| TC12（新設・②） | ②の最密ステージ密度で隣接ブロック全142組×代表速度(通常/高速)×代表軌道(垂直/斜め)＝568試行を実測し、ボール半径を超えるオーバーラップ（視認可能な貫通）が起きないことを検証 | PASS（568試行・違反0件。resolveBlocks拡張前は112〜396件の違反あり） |
| TC13（新設・③-C・梨緒レビュー→修正3で4方向に拡張） | 2x2に隣接配置した4ブロックの隙間の交差点（十字の中心）へ右上・左上・右下・左下の4方向からボールを1サブステップで到達させ、4ブロック全ての同時ヒット・破壊、速度反転がちょうど1軸のみであること、`blockHitCount==4`（修正1回帰チェック）、生存ブロックへのめり込みがないこと、最終位置が異常値でないことを4パターンで検証 | PASS（4/4パターンで4ブロック同時破壊・反転軸はいずれもY軸1回のみ・blockHitCount=4・生存ブロックへのめり込み0件） |

**-O2最適化での再実測：** 上記TC1〜TC12を`g++ ... -O2`でビルドし直しても同じ12/12 PASSを確認済み（TC13新設後・修正1〜3後の-O2再実測は未実施・次回の既知の限界として扱う）。

## 既知の限界（正直な報告・3周目修正後も残っているもの）

- **パドル反射の期待値検証（TC3・TC5b）は本番コード（resolvePaddle）と同じ式を再実装した比較が
  中心**。パドル反射は「入射方向によらずヒット位置だけで反射方向を決める」設計であり、
  古典的な「入射角=反射角」の物理的反射則が存在しないため、設計意図そのものと比較する以外の
  完全に独立した幾何学的検証手段がない。速さの再正規化・オフセット符号との一致という
  独立不変量は追加したが、`PADDLE_MAX_BOUNCE_ANGLE`自体の値が間違っていた場合はこれらの
  独立チェックでは検出できない。
- **TC7（真の角ヒット）は「ちょうど1軸だけが反転しクラッシュ・二重反転が起きない」という
  フォールバック分岐の健全性のみを検証している。「選ばれた軸が幾何学的に正しいか」までは
  検証していない**（未光3周目指摘）。TC11でめり込み量に意図的な差（ΔX<ΔY<ΔZ）を付けて
  優先順位ロジックの一意性は検証したが、これも「めり込み最小の軸を選ぶ」という設計方針
  そのものが正しいかどうかまでは検証していない。
- **resolveBlocksの複数ブロック同時処理（②修正）は、TC12で2ブロック隣接
  （X方向・Y方向それぞれ）、TC13（③-C・梨緒レビュー対応）で2x2の4ブロック
  同時重なりまで実測を拡張した。** TC13の実測で、同じ軸で複数ブロックが異なる
  スナップ値を要求し合うケースが実際に発生し、`snapOutsideAxis`が後から処理した
  ブロックの値で前のブロックのスナップ結果を上書きすること（＝最終位置は
  `blocks`ベクタの処理順で決まる）を確認した。**ただし、TC13は4ブロックとも
  耐久値1で同時に破壊される構成のみを検証しており、「上書きされても生存
  ブロックへのめり込みという実害には至らない」ことまでしか確認できていない。
  一部のブロックだけ耐久値が2以上で生き残る構成（例：奥レイヤー導入後に
  X/Y/Zすべての方向で隣接し、かつ耐久値がまちまちなケース）では、上書きされた
  スナップ位置が生存ブロックの内部に残ってしまう可能性があり、この構成は
  未検証のまま残っている**（現状は理論上ありうる不具合として認識しているのみで、
  実測・修正のいずれも行っていない）。
- **TC12は「最も密度が高いステージ」として`denseStageIndex=10`固定で1パターンのみ検証**。
  ステージごとに隙間・行列数・Zレイヤー数が変わるため、他のステージ（特にZレイヤーが
  2層になるステージ3以降）で奥レイヤーのブロック同士・手前レイヤーとの間で同様の
  同時ヒット問題が起きないかまでは検証していない（幾何学的には同じ`resolveBlocks`ロジックを
  通るため同様に解決されるはずだが、実測はステージ10の単層部分のみ）。
- **③-Aのゲームループは最小実装**：ボールはパドル上で待機せず自動発射される（本来の
  ブロックくずしにある「発射待ち」の演出は未実装）。スコア加算はブロック1個あたり
  固定10点（耐久値・ステージ番号による加点調整はしていない）。パドル操作・状態遷移の
  実プレイによる目視確認は実施できていない（自動テストのみ）。
- **③-BのWebビルドはブラウザでの実際の目視確認ができていない**。ビルド成功・
  `emrun`経由でのHTTP配信（index.html/js/wasm全てHTTP 200）までは確認したが、
  実際にcanvasに3Dシーンが描画されるか・キーボード操作が効くかは未確認。次回、
  実際のブラウザ環境での確認を推奨する。
- **パドル反射の期待値検証（TC3・TC5b）は本番コード（resolvePaddle）と同じ式を再実装した
  比較が中心**（①-Bから継続する既知の限界。詳細は上記進捗報告の段落を参照）。
- **TC7（真の角ヒット）は「ちょうど1軸だけが反転しクラッシュ・二重反転が起きない」という
  フォールバック分岐の健全性のみを検証している**（①-Bから継続する既知の限界）。
- **TC13の4方向拡張（修正3）は、いずれの方向から接近しても`snapOutsideAxis`のフォール
  バックがY軸を選ぶ（4/4パターンとも同一の軸選択）ことしか実測できていない**。
  4ブロックの配置・耐久値・接近方向を変えても常にY軸が選ばれるのは、cx/cyの
  浮動小数点丸め誤差の出方が4方向とも同じ大小関係になったためで、「他の軸が
  選ばれるケース」自体は依然として④以降（他の耐久値構成・他のステージの
  ブロック密度）でのみ発生しうる可能性があり未検証（③-C時点の既知の限界と同じ）。
- **④のAudioManager・BGMループはraylibの`Sound`に対する手動再トリガー方式**であり、
  厳密には「鳴り終わりを検知して次を再生する」ギャップ有りループである（シームレス
  ループではない）。1秒程度の短い単音BGMのため実用上大きな違和感は想定していないが、
  実際に耳で聴いての確認（清広さんの環境でのプレイテスト）は未実施。
- **④のWeb版音声アンロック（`initAfterUnlock()`）はロジック上は実装したが、実際に
  ブラウザで「タイトル画面の最初のキー入力後に音が鳴るか」の目視・実聴確認は
  本環境（ヘッドレスCLI環境）では実施できていない**。③-Bと同様、次回ブラウザ環境
  での確認を推奨する。
- **④のScoreFileは「GameOver確定時」「ステージクリア確定時」のみ保存する設計**のため、
  ウィンドウを閉じる等で不正終了した場合、その回の最高得点がこれらの区切りに
  到達する前だと保存されない（意図的な割り切り。詳細は上記「④：⑥⑦⑧の実装」参照）。
- **④のパーティクル演出・見た目の仕上げ（耐久値3段階の色分け・ボール輪郭）は実プレイに
  よる目視確認ができていない**（ヘッドレステスト・数秒間のネイティブ起動確認のみ）。
  過度な演出・パフォーマンス影響がないかは次回の実プレイで確認を推奨する。

## 注意事項

- `breakout3d.exe` `test_collision.exe` `test_results.log` は `.gitignore` で除外済み（コミットしない）
- `web/`配下はビルド成果物（index.html/js/wasm/data）を`.gitignore`で除外しているが、
  手書きソースの`web/shell.html`だけは例外的にコミット対象（`web/* + !web/shell.html`）
- `build_test_err.log`・`build_test_output.log`・`run_native.log`・`build_native.log`・
  `emrun.log`はいずれも動作確認のたびに再生成されるログのため`.gitignore`で除外済み（修正2）
- `highscore.txt`（④で追加したScoreFileの保存先）も`.gitignore`の「セーブデータ」
  エントリで除外済み（個人のプレイ記録なのでコミット対象にしない）
- GitHub Actions CIは今回のスコープ外（後続ステップ⑪で対応）
- 衝突判定のテストケース詳細・実測結果は上記表・`test_results.log`・麻耶からの報告（daily note等）を参照
