# RD ENGINE 1.0

DirectX 12 で動く自作のゲームエンジンです。Unity と同じ考え方の Actor + Component で作り、ImGui のエディタが付いています。
`old/` の旧エンジンを、設計を整理しながら作り直しました。

## ビルドと実行

1. `RdEngine.sln` を Visual Studio 2026（v145 ツールセット）で開く
2. 構成を選んでビルドする
3. スタートアッププロジェクト `Sandbox` を実行する（F5 で `RdEditor.exe -game Sandbox` が起動します。作業ディレクトリは自動でこのフォルダになります）

| 構成 | 内容 |
| --- | --- |
| `Debug` | エディタ付き・最適化なし。`RD_DEBUG=1` |
| `Release` | エディタ付き・最適化あり |
| `Game` | エディタと ImGui を外したゲーム用（`RD_EDITOR=0`、Release と同じ最適化）。起動するといつもプレイ中 |

エディタ付きの構成（Debug / Release）と Game 構成では、できるものが違います（Unreal と同じ形）。

| | Debug / Release | Game |
| --- | --- | --- |
| エンジン | `RdEngine.dll` | 静的ライブラリ（exe に入る） |
| ゲーム（Sandbox など） | `Sandbox.dll` | `Sandbox.exe` |
| 起動する exe | `RdEditor.exe -game Sandbox`（どのゲームでも共通） | `Sandbox.exe` |

ゲームを DLL にしているのは、エディタを開いたままゲームのコードを読み直せるようにするためです（後述の「ゲームのコードの読み直し」）。
出力は `Bin\<構成>\`、中間ファイルは `Intermediate\` に置きます。
初回起動時に `Assets/Sample/Scenes/Sample.scene` が自動で作られます。

### テスト
`Tests` プロジェクトをビルドして `Bin\Debug\Tests.exe` を実行します（失敗した数が終了コードになります）。
```
Bin\Debug\Tests.exe            すべて
Bin\Debug\Tests.exe Scene      名前に "Scene" を含むものだけ
```
数学・幾何、JSON とリフレクション、シーンの更新順とコンポーネントのリスト、参照（ActorRef）、Undo/Redo、物理の形状、JobSystem、音声の読み込み、クラッシュハンドラ（自分自身をわざと落として調べる）、.rdmodel の読み書き、シェーダーとマテリアルのアセット、エディタの言語と状態の保存、NavMesh のギズモと焼き（床・壁・段差・天井）、行動ツリーのアセット（保存と読み込み・組み立て・子の並び）、レンダーグラフ（飛ばすパス・テクスチャの使い回し・状態）、UDP の通信（自分宛てに送って受け取る・中身の詰め方）、ホットリロード（コンポーネントの登録の入れ替えと Missing Component）、AI アシスタント（ツールと LLM ごとの変換）を確かめます。GPU を使うもの（レンダーグラフの実行など）は、`-capture` で撮った画面で確かめます。

### コマンドライン

| 引数 | 内容 |
| --- | --- |
| `-game <名前>` | RdEditor.exe で開くゲーム（`<名前>.dll` を読む）。RdEditor.exe では必ず指定する |
| `-noeditor` | エディタなしでゲームとして起動 |
| `-play` | エディタでも起動直後にプレイ開始 |
| `-scene <path>` | 開くシーン |
| `-capture <frame> [path]` | 指定フレームの画面を PNG に保存して終了（自動テスト用） |
| `-profile <frames> [path]` | 起動から指定フレーム分の計測を保存（既定 `Logs/Profile.json`） |
| `-loglevel <level>` | trace / debug / info / warn / error |
| `-logfile <name>` | ログのファイル名を変える（同じ PC で 2 つ動かして通信を試すときなど） |
| `-fullscreen` / `-windowed` | 全画面 / ウィンドウ（実行中は F11 か Alt+Enter で切り替え） |
| `-nocrashdialog` | 落ちたときのダイアログを出さない（レポートとダンプは残す） |
| `-renderpath forward\|deferred` | 描き方をフォワード / ディファードで始める（既定はフォワード） |
| `-novsync` | 垂直同期オフ |

## フォルダ構成

```
claude/
├─ RdEngine.sln
├─ Build/
│  ├─ Common.props          全プロジェクト共通の設定（出力先・C++20・構成ごとの定義・RD_API）
│  ├─ EngineLink.props      エンジンが使うライブラリのリンクと DXC のコピー
│  └─ GameModule.props      ゲームのプロジェクト共通の設定（DLL / exe の切り替え・F5 の起動・PDB の名前）
├─ External/                 ImGui(docking) / ImGuizmo / imgui-node-editor / DirectXTex / assimp 6.0 / nlohmann-json / magic_enum / stb_truetype
│  └─ ThirdParty.vcxproj     ImGui と DirectXTex をまとめた静的ライブラリ
├─ Engine/                   エンジン本体（Debug / Release では RdEngine.dll、Game では静的ライブラリ）
│  └─ Source/
│     ├─ Core/        ログ・クラッシュハンドラ・プロファイラ・アサート・JobSystem・リフレクション・JSON・時間・文字列とファイル
│     ├─ Math/        ベクトル・行列・クォータニオン・色・乱数・幾何（AABB / Ray / Plane / Frustum / 交差判定）
│     ├─ Platform/    ウィンドウ・入力（キーボード / マウス / XInput・カーソル固定）・HTTPS 通信（WinHTTP）・UDP 通信
│     ├─ RHI/         D3D12 デバイス・デスクリプタ・テクスチャ・パイプライン・シェーダーコンパイラ
│     ├─ Asset/       非同期アセット管理・テクスチャ・モデル（assimp と .rdmodel）・アニメーション・音声のデコード
│     ├─ Render/      レンダラー・レンダーグラフ・描画の機能（Features）・描画リスト・カメラ・環境設定・マテリアル（.rdmat）と自作シェーダー（.hlsl）・デバッグ線
│     ├─ Audio/       XAudio2
│     ├─ Scene/       Actor・Component・Transform・Scene・参照（ActorRef / ComponentRef）・SceneSystem・保存と読み込み（SceneSerializer）
│     ├─ AI/          NavMesh と経路探索（NavigationSystem）・行動ツリー（BehaviorTree）
│     ├─ Components/  描画 / アニメーション / ライト / エフェクト / 音 / ナビゲーション / UI のコンポーネント
│     ├─ Physics/     コライダー・剛体・CharacterController・物理ワールド（SceneSystem として動く）
│     ├─ Runtime/     シーン切替・プレイ状態・フレームの組み立て・画面の保存・組み込みコンポーネント登録・ゲームの DLL（GameModule）・差し込む層（EngineLayer）
│     ├─ Editor/      ImGui エディタ（Editor が組み立て、EditorMainUI がメニュー・ツールバー・ステータスバー・ショートカット）
│     │  ├─ Panels/     Scene・Game・Hierarchy・Properties・Preview・Project・Log・Lighting・Stats・Profiler・Behavior Tree・Render Graph
│     │  ├─ Commands/   Undo/Redo（EditorUndo）とコマンド
│     │  ├─ Style/      色・アイコン・共通の UI 部品（見た目をそろえる）
│     │  ├─ Inspector/  プロパティの表示（ImGuiInspector）・コンポーネントごとの追加の UI（ComponentEditors）
│     │  ├─ Preview/    モデル・プレハブ・マテリアル・テクスチャのプレビューとサムネイル
│     │  ├─ Core/       エディタの部品（選択・アクターの操作・シーンのファイル・プレイ・配置）・ImGui の初期化・エディタカメラ・ファイルダイアログ・取り込み・言語・状態の保存・ホットリロード
│     │  └─ Assistant/  AI アシスタント（LLM ごとの変換・ツール）
│     ├─ Engine.h/.cpp サブシステムの所有とメインループ（エディタは知らない。バージョンは Engine::kVersion）
│     └─ GameLauncher.h/.cpp  ゲームの起動（RD_GAME_MAIN・RdEditor.exe の入口・エディタを差し込む）
├─ Launcher/                 RdEditor.exe（エディタの起動用。Debug / Release だけ）
├─ Tools/
│  ├─ CheckDependencies.ps1  モジュール間の依存が層の決まりを守っているか調べる
│  └─ BuildAssimp.ps1        同梱の assimp をビルドし直す（assimp-md.lib / assimp-mdd.lib）
├─ Tests/                    自動テスト（自前の小さなテストフレームワーク）
├─ Sandbox/                  サンプルゲーム（エンジンの使い方の例）
├─ Assets/
│  ├─ Engine/   シェーダー・フォント・アイコン・訳の表（Localization/ja.json）
│  └─ Sample/   サンプル用のモデル・テクスチャ・音・シェーダー・マテリアル・シーン
├─ Logs/                     ログ・クラッシュレポート・パネルの配置（Git に入れない）
└─ UserSettings/             エディタの状態と AI アシスタントの設定（Git に入れない）
```

## 設計のポイント

### 全体の形
```
Engine（Engine.cpp）……… サブシステムを持ち、メインループを回す。エディタのことは知らない
 ├─ Window / Input                    Platform
 ├─ GraphicsDevice                    RHI
 ├─ AssetManager                      Asset（読み込みは JobSystem で裏で行う）
 ├─ Renderer                          Render（描画の機能を順に呼び、レンダーグラフで描く）
 ├─ AudioSystem                       Audio
 ├─ SceneManager / PlayController     Runtime（シーンの差し替え・ゲーム時間と一時停止）
 │    └─ Scene ── Actor ── Component  Scene（PhysicsWorld・NavigationSystem は SceneSystem としてシーンが持つ）
 ├─ FrameComposer / FrameCapture      Runtime（1 フレームの描画の組み立てと画面の保存）
 └─ EngineLayer（差し込まれた層）     エディタのある構成では Editor。Game 構成と -noeditor ではなし
```
1 フレームの順番（`Engine::Frame`）: ジョブの後始末とシーンの差し替え → 時間を進める → 層（エディタ）の更新 → `Scene::Update`（Start → Update → FixedUpdate と物理を決まった間隔で必要な回数 → LateUpdate。各段でコンポーネントの後に SceneSystem）→ 音 → 描画（`OnRender` で描くものを集め、レンダーグラフで描く）→ 画面に出す。

サブシステムへの届き方は 2 種類にそろえている。
- **Engine が持つもの**（`GraphicsDevice`・`AssetManager`・`Renderer`・`AudioSystem`）: `Engine` が作って壊す。どこからでも `Renderer::Get()` のように使える
- **状態をほとんど持たない道具**（`Logger`・`Input`・`Time`・`JobSystem`・`Profiler`・`ComponentRegistry`・`SceneSerializer`）: クラスの static 関数で使う
- エディタは `Engine::Get()` を使わず、作るときに渡された `Engine&` を使う

### エンジンとエディタの分け方（EngineLayer）
エンジンはエディタを知らない。エディタは `Runtime/EngineLayer` を実装したもので、起動のとき（`RunGame`）に `EngineConfig::createLayer` で差し込まれる（Unreal で `UEditorEngine` が `UEngine` に足す部分にあたる）。
- エンジンは決まったときに層の関数を呼ぶだけ: `OnConfigure`（窓を作る前。前回の窓の位置と開いていたシーン）→ `OnInitialize` → 毎フレーム `OnBeginFrame`・`OnUpdate`・描画への割り込み（`FrameOverlay`）→ `OnShutdown`（状態の保存）
- 層がなければ、起動からずっとプレイ中として動く。層があれば、プレイを始めるのは層（エディタの Play）
- プレイ前のシーンを取っておいて Stop で戻すこと、ゲームのコードの読み直し、エディタの状態の保存、窓のタイトルは、すべてエディタ側の仕事

### エディタの部品
`Editor` はパネルと部品を持って組み立て、フレームの流れを回すだけ。パネルは `editor.GetSelection()` のように部品を通して操作する。
| 部品 | 仕事 |
|---|---|
| `EditorSelection` | 選んでいるアクター（ID で覚える） |
| `EditorUndo` | Undo / Redo。選んでいるアクターとシーンの設定の「変更前」を覚え、確定したときにコマンドにする |
| `ActorActions` | 作る・消す・複製・コピーと貼り付け・プレハブ・モデルを置く・マテリアルを入れる |
| `SceneDocument` | 新規・開く・保存・閉じる（保存していない変更があれば先に聞く） |
| `PlayMode` | Play / Stop / Pause / Step（Play でシーンを JSON に写し、Stop で戻す） |
| `HotReload` | ゲームのコードのビルドと読み直し |
| `EditorLayout` | パネルのドッキングの配置 |
| `EditorMainUI` | メニュー・ツールバー・ステータスバー・全体のショートカット |
### ゲームのコードの読み直し（ホットリロード）
エディタを開いたまま、ゲームのコードを書き換えて読み直せます（Unreal のホットリロードと同じ考え方）。関数の中身だけでなく、変数の追加、新しいクラスやファイルの追加にも対応します。

1. エディタを開いたまま、Visual Studio でゲームのコード（Sandbox など）を書き換える
2. エディタのツールバー右上の **Compile**（または **Ctrl+Alt+F11**、ファイル → ゲームのコードをコンパイル）を押す。
   Visual Studio でビルドしてもよい（できた DLL が新しくなったことにエディタが気づく）
3. エディタが新しいコードを読み直し、シーンを作り直す。シーン・プロパティの値・選んでいるもの・Undo の履歴・保存していない変更はそのまま残る

- ビルドの出力とエラーは Log パネルに出る。失敗したら今までのコードのまま動き続ける
- **プレイ中は読み直さず**、止めたときに読み直す
- Visual Studio でデバッグ中（F5）は、Visual Studio がビルドさせてくれないので、エディタの Compile を使う。ブレークポイントは新しいコードでも効く
- 足した変数には初期値が入り、消した変数の値は捨てる。**クラスを消したり名前を変えたりしたコンポーネントは `MissingComponent` になり、値を持ったまま残る**（Unity の Missing Script と同じ）。保存しても値は消えず、クラスが戻れば元に戻る
- ゲームの DLL の `static` 変数・グローバル変数は、読み直すと初期値に戻る。残したい値はコンポーネントに持たせる
- エンジン（`Engine/`）のコードを変えたときは、エディタを閉じてからビルドする

作り（`Runtime/GameModule`・`Editor/Core/HotReload`）:
- ビルドの出力（`Bin\Debug\Sandbox.dll`）は読み込まずに、`Bin\Debug\HotReload\` へコピーしたものを読む。読み込み中の DLL は上書きできないため
- 読み直すときは、ジョブを待ってからシーンを JSON にし、ゲームのコンポーネントの登録を外してから新しい DLL を読み（中の `RD_REGISTER_COMPONENT` が登録し直す）、JSON からシーンを作り直す
- 古い DLL は外さずに残す（古いコードを指した文字列や関数がどこかに残っていても落ちないように）
- デバッガが PDB を開いたままだとリンクできないので、そのときだけ PDB を `Sandbox-<時刻>.pdb` という別の名前で作る（`Build/GameModule.props`）

### モジュールの層
下の層は上の層を `#include` しない。`Tools/CheckDependencies.ps1` で確認できる（違反があると終了コード 1）。

```
Math / Core → Platform / RHI → Asset → Render / Audio → Scene → AI → Components → Physics → Runtime → Editor
```
- `Scene` はコンポーネントや物理の具体的な型を知らない。物理ワールドや NavMesh は `SceneSystem` として `scene.GetSystem<PhysicsWorld>()` のように使う。
- `Asset` は `Render` の型を知らない。マテリアルやシェーダーは呼ぶ側で `AssetManager::LoadNow<MaterialAsset>()` のように型を指定して読む。
- `AI`（NavMesh・行動ツリー）は `Scene` の上、`Components` の下。`NavMeshAgent` などのコンポーネントが `AI` を使い、`AI` はコンポーネントの型を知らない。
- `Runtime` と `Engine` はエディタを知らない。エディタは `EngineLayer`（`FrameOverlay` を含む）を実装して、更新・カメラ・ギズモ・UI を差し込む。
- **ImGui を使えるのは `Editor` だけ**。コンポーネントは自分の画面を持たず、値は `Reflect` に書く。ボタンなど `Reflect` で表せない UI は、エディタの `ComponentEditors` に型ごとに登録する（Unity の CustomEditor にあたる）。
  ```cpp
  ComponentEditors::Register<AudioSource>([](AudioSource& s) { if (ImGui::Button("Preview")) { s.Play(); } return false; });
  ```
  組み込みの分は `Editor/Inspector/BuiltinComponentEditors.cpp`。シーンビューの線（`OnDrawGizmos`）は `DebugDraw`（Render）だけで描けるので、コンポーネントに書いてよい。
- 1 つのファイルには 1 つのクラス（コンポーネントも 1 つずつ）。小さな補助の構造体はクラスの中に入れる。
- 検査（`Tools/CheckDependencies.ps1`）は、層の表にないフォルダを include したときと、`Editor` の外で ImGui を include したときも違反にする。
- エディタは作るときに `Engine&` を受け取り、`Engine::Get()` に頼らない。
- エンジンの外（ゲームとテスト）から使うクラスと関数には `RD_API` を付ける（Unreal の `ENGINE_API` にあたる）。Debug / Release では DLL から出す印になり、Game 構成では何もしない。付け忘れるとゲームのリンクで「未解決の外部シンボル」になる。
  `Vector3` などの数学の型は、定数をヘッダの中で定義しているので、クラスではなく .cpp に本体がある関数にだけ付ける。

### コンポーネントの更新
アクターを全部回して仮想関数を呼ぶのではなく、シーンが**型ごとのリスト**と**コールバックごとのリスト**を持つ。
- `RD_COMPONENT` が、そのコンポーネントが `Update` / `FixedUpdate` / `LateUpdate` / `OnRender` などを上書きしているかを自動で調べる。上書きしていないもの（ライトの `Update` など）は呼ばない。
- `scene.FindComponents<T>()` は型ごとのリストから集めるので、全アクターを調べない。順番は追加した順で毎回同じ。

### リフレクション
コンポーネントは `Reflect(PropertyVisitor&)` に一度プロパティを書くだけで、JSON の保存と読み込み・プロパティ画面の表示・Undo・AI アシスタントのツールに対応する。保存は型名の文字列で行い、`ComponentRegistry` で作る。シーンとアクターのファイルの形を知っているのは `SceneSerializer` だけで、`Scene` は動いているアクターの入れ物（ゲームから使う `Instantiate`・`Duplicate` は `Scene` にある）。

### Undo / Redo（コマンドパターン）
操作ごとに `EditorCommand` を作って `CommandHistory` に積む。変更した部分だけを保存する。
| コマンド | 内容 |
|---|---|
| `ActorStateCommand` | アクター 1 つの変更前後（名前・Transform・コンポーネントの値・追加・削除・並び替え） |
| `ActorLifetimeCommand` | アクターの作成 / 削除（子ごと。ID はそのままなので参照も復活する） |
| `ReparentCommand` | 親子の付け替え（兄弟の順番も戻す） |
| `SceneSettingsCommand` | 環境・重力の変更 |
| `MaterialAssetCommand` | マテリアルのファイル（.rdmat）の値の変更 |
| `CompositeCommand` | 複数のコマンドを 1 回の操作にまとめる |

短い間隔で同じ操作が続いたら 1 つにまとめる。プレイを停止しても履歴は残る。保存した位置を覚えていて、タイトルの `*` に反映する。

### ログ
```cpp
Logger::Info(LogCat::Render, "Loaded {}", path);   // カテゴリを省略すると General
```
- レベル: Trace / Debug / Info / Warning / Error / Fatal。`-loglevel debug` で実行時に変更。Game ビルドでは Debug 以下をコンパイル時に除く。
- 出力先（`LogSink`）: Visual Studio の出力ウィンドウ（Warning 以上はダブルクリックで該当行へ）、ファイル、エディタの Log パネル。書き込みはログ専用スレッドで行い、Error 以上は書き出しまで待つ。
- ファイル: `Logs/RdEngine.log`。前回分は `Logs/History/` に日時付きで 10 個まで残す。各行に時刻・経過秒・レベル・カテゴリ・スレッド・発生場所が付く。

### クラッシュハンドラ（`Core/Debug/CrashHandler`）
落ちたときに `Logs/` へ 2 つのファイルを残し、ダイアログで知らせる（デバッガ接続中はデバッガで止まる）。
- `Crash_日時.txt`: 原因（アクセス違反なら読み書きしたアドレスも）、落ちた場所、**関数名とファイル:行付きの呼び出し履歴**、OS・メモリ・GPU・開いていたシーン・プレイ中か、直前 40 件のログ
- `Crash_日時.dmp`: ミニダンプ。Visual Studio で開いて「ネイティブのみでデバッグ」を押すと、落ちた行で止まった状態を見られる
- 対象: アクセス違反・ゼロ除算・スタックオーバーフロー、捕まえていない C++ 例外（`what()` の文字列も載る）、`abort()` / `RD_VERIFY` / `Logger::Fatal`、純粋仮想関数の呼び出し、CRT の不正な引数
- レポートに載せる情報は `CrashHandler::SetContext("名前", []{ return ...; })` で足せる

### プロファイラ（`Core/Profiling`、エディタの Window → Profiler / F9）
```cpp
void Enemy::Update(float dt)
{
    RD_PROFILE_FUNCTION();           // 関数全体
    { RD_PROFILE_SCOPE("Path"); ... } // 一部分
}
RD_GPU_SCOPE("Shadow");              // CPU と GPU（タイムスタンプ）の両方
```
- スレッドごと・フレームごとに記録し、直近 300 フレームを残す。ジョブには名前が付く（`JobSystem::Schedule(func, prio, "名前")`）
- パネル: フレーム時間のグラフ（CPU の棒と GPU の線、クリックでそのフレームを止めて見る）、スレッドごとのタイムライン（Ctrl+ホイールで拡大）、区間ごとの平均 / 最大 / 回数
- 「Save Trace」か `-profile 300` で Chrome のトレース形式に保存し、[ui.perfetto.dev](https://ui.perfetto.dev) で開ける
- GPU の区間は PIX / RenderDoc のイベント名にもなる。`RD_PROFILER=0` で計測コードを丸ごと外せる

### レンダーグラフとフォワード / ディファード
- 1 回の描画で行うパスを、`rg::RenderGraph` に「何を読んで何に書くか」と一緒に登録してから実行する
  - パスを登録するのは描画の機能（`Render/Features` の `RenderFeature`。Unity の URP の Renderer Feature にあたる）。`Renderer` はフレームの値を作り、描くものを集め、機能を次の順に呼ぶだけ:
    影（`ShadowFeature`）→ 不透明（`ForwardFeature` か `DeferredFeature`）→ 縁取り → ブルーム → トーンマップ → 重ねるもの（デバッグ線・スプライト）→ スクリーンショット
  - 機能は自分のパイプラインを持ち、テクスチャは `FrameContext::textures`（output・depth・hdr・shadow・bloom）で受け渡す。メッシュの描き方（`MeshDrawer`）と空・半透明（`TransparentDrawer`）は機能どうしで共有する
  - リソースの状態の切り替え（バリア）はグラフが入れる。パスの中では描くだけ
  - 出力に届かないパスは飛ばす。途中でだけ使うテクスチャ（HDR・深度・ブルーム・G バッファー・縁取りの印）はグラフが持ち、使う期間が重ならず大きさと形式が同じなら 1 枚を使い回す（次の描画でも使い回す）
- 描き方は Lighting パネルの Renderer > Render Path（または Render Graph パネルのボタン、起動時は `-renderpath deferred`）で切り替える
  - **Forward**: 物を描くときにその場で光を当てる
  - **Deferred**: 不透明な物を G バッファー（色 + 金属感・法線 + 粗さ・発光 + 種類。3 枚とも RGBA16F）へ書き、画面全体で 1 回だけ光を当てる（`DeferredLighting.hlsl`）。そのあとで空・半透明・パーティクルをフォワードで重ねる。自作のサーフェスシェーダーもそのまま使える（`GBUFFER` を定義してコンパイルし直す）
  - ディファードでは物ごとのライトの数の制限（Lights Per Object）は効かず、すべての点光源・スポットライトを当てる
- Window > Render Graph パネル: メインの画面を最後に描いたときのパスとテクスチャのつながり、飛ばしたパス、使い回している実体の番号を見せる

### 自作シェーダーとマテリアル
- **シェーダー（.hlsl）**: `Surface.hlsli` を include して `Surface` 関数（必要なら `SurfaceVertex` も）を書く。ファイルの先頭の注釈に Unity と同じ書き方で `Properties { ... }` を並べると、エンジンが変数を宣言し、マテリアルの画面で値を変えられる（Color / Float / Range / Int / Vector / 2D、`[HDR]`・`[Toggle]`）。見本は `Assets/Sample/Shaders/Stripes.hlsl`
- **マテリアル（.rdmat）**: JSON で保存するマテリアル。いくつもの MeshRenderer が同じものを指せて、値を変えると全部の見た目が変わる。標準の陰影（PBR）のほか、Flat（面ごとの平らな陰影・影の色・リム）と Triplanar（ワールド座標で 3 方向から貼る）を選べる
- どちらも Project パネルの「Create」で作れる。シェーダーを書き換えたら Ctrl+R で読み直す

### モデル（.rdmodel）
fbx / glTF / obj などを Project パネルへドロップすると、描画とアニメーションに使うものだけを圧縮したエンジン独自の `.rdmodel` に変換して取り込む（ワーカースレッドで変換するのでエディタは止まらない）。解析がいらないので読み込みが速い。法線は八面体の符号化、ウェイトや関節の番号は 16 ビットにして小さくしている。元のファイルは assimp でそのまま読むこともできる。

### AI とナビゲーション
- `NavMeshBuilder`: シーンのメッシュからナビメッシュを焼く（Recast と同じく、ボクセルに塗る → 段差と天井で絞る → エージェントの半径で縁を削る → 平面に乗る範囲を四角形にまとめる。256 マス四方のタイルに分けて並べて焼く）
- `NavMesh`: 焼いた（または Unity から書き出した）三角形を読み、辺を共有する三角形とタイルの境目で一部が重なる辺をつなぐ。A* と funnel で曲がり角を求め、リンク（跳んで渡る道）にも対応する
- `NavMeshSurface` / `NavMeshLink` / `NavMeshAgent`: Unity と同じ使い方のコンポーネント。Surface のインスペクタの「焼く」で、エージェントの設定どおりに焼いてシーンの隣の `<シーン名>/NavMesh-<アクター名>.json` に保存する。エージェントはリンクの入口で止まり、`CompleteOffMeshLink` で続きを歩く
- `bt::`（行動ツリー）: Sequence / Selector / Guard / Branch / Switch / Repeat / Inverter と、Check・Wait・Log などの組み込みのノード
  - ツリーの形はアセット `.rdbt`（JSON）に置き、エディタの「行動ツリー」パネル（thedmd/imgui-node-editor）でノードをつないで組む。プロジェクトパネルの 作成 > 行動ツリー で作り、ダブルクリックで開く
  - ゲームの行動・条件・Switch の値は `bt::Registry::RegisterAction / RegisterCondition / RegisterSelector` で登録する（ゲームのコードを読み直すと登録し直される）。登録した行動はパネルの作成メニューに出る
  - `BehaviorTreeAgent` コンポーネントの Tree にアセットを入れると動く。プレイ中は、選んでいるアクターのツリーの動いているノードがパネルで黄色の枠になる（成功は緑、失敗は赤が少しの間）

### 組み込みのコンポーネント
エディタの「Add Component」に分類ごとに出る（ゲームのものは `RD_REGISTER_COMPONENT` で足す）。

| 分類 | コンポーネント |
| --- | --- |
| Rendering | MeshRenderer・SpriteRenderer・ParticleSystem・CameraComponent（有効なものの中で Priority が一番高いものがゲームの画面になる） |
| Light | DirectionalLight・PointLight・SpotLight |
| Animation | Animator・AnimatorController |
| Physics | Rigidbody・CharacterController・SphereCollider・BoxCollider・CapsuleCollider・MeshCollider |
| Audio | AudioSource |
| Navigation | NavMeshSurface・NavMeshLink・NavMeshAgent |
| AI | BehaviorTreeAgent |
| UI | Canvas・CanvasGroup・RectTransform・UIImage・UIText |

組み込みのモデル `Engine/Cube`・`Engine/Sphere`・`Engine/Plane`・`Engine/Capsule` は、ファイルなしで MeshRenderer に使える。

### 入力（`Platform/Input`）
- キーボードとマウスは Win32 のメッセージ、マウスの移動量は Raw Input、ゲームパッドは XInput から読む
- `Input::SetCursorLocked` でカーソルを隠してウィンドウの中に閉じ込める（FPS・TPS のカメラ用）。ウィンドウが前面でないときは外れ、戻ると自動でかけ直す
- エディタの UI を触っている間は、ゲーム側の入力を止める

### 通信（`Platform/Network`）
- `UdpSocket`: UDP のソケット（待たずに使う。`Receive` は届いていなければすぐ 0 を返す）。`Open(port, broadcast)` で LAN 全体へ送れる
- `NetAddress`: 相手の住所。`Loopback(port)`（同じ PC）・`Broadcast(port)`（同じ LAN の全員）・`Parse("192.168.0.3:7777")`
- `NetWriter` / `NetReader`（`NetPacket.h`）: 送る中身を詰める / 順に取り出す。足りないところを読もうとしたら、以後は失敗のままになる（`IsOk`）
- 届く順番も届くかどうかも保証されないので、使う側で何度も送り直すなどして扱う
- `HttpClient`: HTTPS の通信（WinHTTP）。AI アシスタントが使う

### UI
画面に重ねる UI（Unity の Canvas の Screen Space - Overlay にあたる）。
- `Canvas`（Reference Resolution と Match で拡大・縮小）・`CanvasGroup`（まとめて透明に）・`RectTransform`
- `UIImage`: Simple / Sliced（9 分割）/ Filled（横・縦）
- `UIText`: 折り返し・Auto Size・縁取り、リッチテキスト `<color=#RRGGBB>` `<size=70%>` `<sprite name="x">`。フォントは stb_truetype でアトラスに焼く
- UI はゲームの画面（Game ビューと `-noeditor`）にだけ描く

### エディタの表示の言語と状態の保存
- Window → Language で英語 / 日本語を切り替える（既定は英語。選んだ言語は次の起動でも使う）。コードの文字列は英語で書き、`Tr("Save")` を通すと訳になる。訳の表は `Assets/Engine/Localization/ja.json`（エンジン）と `Assets/<ゲーム>/Localization/ja.json`（ゲームのコンポーネントの項目名など）。表にない言葉は英語のまま出る
- 終了時に、ウィンドウの位置・開いていたシーン・選んでいたアクター・シーンビューのカメラと表示を `UserSettings/EditorSession_<exe 名>.json` に保存し、次の起動で戻す
- パネルの配置は `Logs/editor_layout.ini`。崩れたときは Window → Reset Layout で初期配置に戻る

### AI アシスタント（エディタの Window → AI Assistant / F8）
LLM に頼んでシーンを編集する。「赤い箱を 5 個、半径 3m の円に並べて」「選んでいるライトを暗くして」など。
**既定はローカル LLM** なので、API キーも利用料もいらず誰でも使える。

使い方（Ollama の場合）:
1. [Ollama](https://ollama.com)（無料）をインストールする
2. コマンドプロンプトで `ollama pull qwen3:8b` を実行する（ツール呼び出しに対応したモデルが必要。qwen3 / qwen2.5 / llama3.1 など）
3. エディタで F8 を押す。モデルの一覧が自動で読まれ、最初のモデルが選ばれる

| 接続先（Settings で選ぶ） | 内容 |
| --- | --- |
| Local (Ollama) | 既定。`http://localhost:11434` の `/api/chat`。文脈の長さ（Context = num_ctx）を指定できる |
| Local (OpenAI compatible) | LM Studio（`http://localhost:1234/v1`）・llama.cpp の `llama-server --jinja` など `/chat/completions` を持つサーバー |
| Claude API | Anthropic の Claude（Opus 5.5 / Sonnet 5.5 / Haiku 5.5）。環境変数 `ANTHROPIC_API_KEY` が必要で、利用料がかかる |

- LLM はツールでシーンを読み書きする: `get_scene` / `get_actor` / `list_component_types` / `list_assets` / `create_actor` / `modify_actor` / `delete_actor` / `select_actor`
- 変更は通常の編集と同じコマンドとして積むので **Ctrl+Z で戻せる**（履歴に「AI: ...」と出る）。プレイ中は変更しない
- 設定は `UserSettings/AIAssistant.json` に保存する（API キーは保存しない）
- 小さなローカルモデルは、ツールの使い方を間違えたり、依頼を取り違えたりすることがある。うまくいかないときは大きめのモデルを試す
- 作り: 会話はどの LLM でも同じ形（`AssistantTurn`）で持ち、`Editor/Assistant/ClaudeProvider`・`LocalProvider` が LLM ごとのリクエストと返事に変換する。通信は `Platform/Network/HttpClient`（WinHTTP）。ツールは `Editor/Assistant/AssistantTools`。どちらもテストからサーバーなしで呼べる

## 機能の一覧

| 分野 | 内容 |
| --- | --- |
| JobSystem | ワーカースレッドプール、優先度付きキュー、`ParallelFor`、待機中のスレッドもジョブを手伝う Wait、メインスレッドへの処理の戻し |
| 非同期読み込み | テクスチャ・モデル・音声はワーカーでデコードし、次フレーム先頭で GPU へ転送。読み込み中は代替表示。小さなファイル（マテリアル・シェーダー）は `LoadNow` でその場で読む |
| RHI | 2 フレーム同時進行（CPU と GPU の並列化）、フレームごとの一時アップロード領域、遅延解放、フリーリスト式デスクリプタヒープ |
| 描画 | レンダーグラフ（バリアの自動化・パスの省略・テクスチャの使い回し）、フォワード / ディファードの切り替え、PBR（GGX）、Flat の陰影、Triplanar、自作のサーフェスシェーダー、カスケードシャドウ（最大 4 段・PCF・テクセルスナップ・遠くで薄くする・色分け表示）、点光源 / スポットライト最大 32 / 16 灯（頂点ごとの計算・物ごとの数の制限も可）、法線マップ、空のグラデーション + 太陽、3 色の環境光（空・地平線・地面）、フォグ、HDR + ブルーム、トーンマップ（ACES / Neutral）、露出・コントラスト・彩度、ビネット、レトロ表示（低解像度 + ディザ）、アウトライン（壁越しに見える縁取り）、視錐台カリング、インスタンシングのパーティクル、スプライトのバッチ描画、半透明の三角形、別の描画先への描画（プレビュー・2 つのビュー） |
| アニメーション | ジョブで並列に姿勢計算、クロスフェード、別ファイルのアニメーション流用（単位の違いを自動補正）、AnimatorController（Unity の状態遷移を JSON から動かす）、骨と同じ名前のアクターを動かす、関節の回転の上乗せ（首を向けるなど） |
| 物理 | 剛体（質量・慣性・重力・抵抗）、球 / 箱 / カプセル / メッシュ（BVH）の衝突、動く物体のメッシュは凸包として当たる（分離軸判定）、スイープ&プルーンの広域判定、逐次インパルス法（反発・摩擦）、Enter / Stay / Exit イベント、トリガー、レイキャスト・重なりの問い合わせ、レイヤー、CharacterController（坂・段差・壁に沿って滑る） |
| AI | NavMesh（焼き・A* + funnel・リンク）、NavMeshAgent、行動ツリー（ノードエディタで組むアセット・プレイ中の表示） |
| UI | Canvas・RectTransform・UIImage（Sliced / Filled）・UIText（日本語・リッチテキスト・縁取り） |
| 入力 | キーボード・マウス（Raw Input の移動量）・XInput のゲームパッド、カーソルの固定 |
| 通信 | UDP（ブロードキャスト・送る中身の詰め方）、HTTPS（WinHTTP） |
| シーン | Actor + Component、親子、プレハブ、参照（ActorRef / ComponentRef。保存と複製でつなぎ直す）、場面の切り替え（フレームの区切りで差し替える）、SceneSystem（物理・ナビゲーション） |
| オーディオ | BGM / SE のバス音量、ループ、ピッチ、距離減衰とパンによる 3D 定位、WAV に加えて MP3 / AAC(m4a) / FLAC / WMA（Windows の Media Foundation で展開） |
| エディタ | ゲームのコードのホットリロード（Compile / Ctrl+Alt+F11）、ドッキング UI（グラデーションと動きのある見た目・浮いたツールバー・ステータスバー）、行動ツリーのノードエディタ、レンダーグラフの表示とフォワード / ディファードの切り替え、ナビメッシュを焼くボタン、Scene と Game の 2 つのビュー（並べると両方映る）、Undo/Redo、移動 / 回転 / 拡縮ギズモ（Ctrl でスナップ）、クリック選択、階層のドラッグ&ドロップで親子付け、アセットブラウザからのドラッグ&ドロップ、エクスプローラーからのファイルの取り込み、コンポーネント追加メニュー（検索付き）、プレイ / 一時停止 / ステップ / 停止（停止で元に戻る。プレイ中にゲームが場面を切り替えても続く）、アクターのコピー&貼り付け・名前の変更、コンポーネントの並び替え・値のコピー&貼り付け、プレハブの作成と配置、Project パネル（フォルダ・マテリアル・シェーダーの作成、名前変更、ごみ箱へ削除、ドラッグで移動、モデルのサムネイル）、テクスチャとモデルのプレビュー（回転・拡大・クリップの再生）、Log パネル（レベルごとの件数・まとめ表示・プレイ時に消去）、NavMesh と物理の表示、日本語表示、終了時の状態の保存、保存していない変更の確認、統計、シェーダーのホットリロード（Ctrl+R） |

## ゲームコードの書き方

```cpp
// MyComponent.h
class Spinner final : public rd::Component
{
    RD_COMPONENT(Spinner)
public:
    float speed = 90.0f;
    void Reflect(rd::PropertyVisitor& v) override { v.Property("Speed", speed); }
    void Update(float dt) override
    {
        GetTransform().Rotate(rd::Quaternion::AngleAxis(rd::Vector3::kUnitY, rd::math::ToRadians(speed * dt)), false);
    }
};

// MyComponent.cpp
RD_REGISTER_COMPONENT(Spinner, "Game");// これだけでエディタの「Add Component」に出る
```

起動の設定は関数 1 つに書いて `RD_GAME_MAIN` に渡す（`Sandbox/Source/Main.cpp`）。`wWinMain` は書かない。
```cpp
#include "GameLauncher.h"

static void ConfigureGame(rd::EngineConfig& config)
{
    config.window.title = "My Game";
    config.startScene = "Assets/MyGame/Scenes/Main.scene";
}
RD_GAME_MAIN(ConfigureGame)
```

新しいゲームのプロジェクトを作るときは、`Sandbox/Sandbox.vcxproj` を写して名前を変える。大事なのは次の 3 つ:
- `ConfigurationType` を Game 構成は `Application`、それ以外は `DynamicLibrary` にする
- `Build/Common.props` の後に `Build/GameModule.props` を読む
- フォルダ名・プロジェクト名・DLL の名前をそろえる（`MyGame/MyGame.vcxproj` → `MyGame.dll`。エディタの Compile はこの名前でプロジェクトを探す）

## エディタの操作

| 操作 | 内容 |
| --- | --- |
| 右ドラッグ + WASD/QE | カメラ移動（Shift で加速、ホイールで速度） |
| 中ドラッグ / Alt + 左ドラッグ | パン / 回り込み |
| F | 選択中のアクターへフォーカス |
| W / E / R | 移動 / 回転 / 拡縮ギズモ |
| Ctrl+Z / Ctrl+Y | Undo / Redo |
| Ctrl+S / Ctrl+Shift+S / Ctrl+O | シーン保存 / 名前を付けて保存 / 開く |
| Ctrl+C / Ctrl+V | コピー / 貼り付け |
| Ctrl+D / Delete / F2 | 複製 / 削除 / 名前の変更 |
| Ctrl+P / Ctrl+Shift+P / Ctrl+Alt+P | プレイと停止 / 一時停止 / 1 フレーム進める |
| Ctrl+R | シェーダーの読み直し |
| Ctrl+Alt+F11 | ゲームのコードをビルドして読み直す |
| F8 / F9 | AI アシスタント / プロファイラ |
| F11 | 全画面の切り替え |

## 座標系の規約
左手座標系・行ベクトル（`v * M`）・行列は「先に適用するものを左」（`S * R * T`）。旧エンジンと同じ。
クォータニオンの掛け算の順番は Unity と逆（Unity の `A * B` はこのエンジンでは `B * A`）。

## 旧エンジンからの主な変更

### 設計の整理
- **型情報を整数 enum で保存するのをやめた**: 旧版は `Actor::Type` / `Component::Type` の番号を JSON に保存していたため、enum の順番を変えると既存データが壊れた。新版は型名の文字列で保存する。
- **リフレクションの一本化**: 旧版の `Load` / `Save` / `UpdateForDev` の手書きを、`Reflect` 1 つにまとめた。
- **レンダラーとエディタの分離**: 旧 `Renderer` は ImGui や `gEngine` を直接触っていた。新版の Renderer は描画リスト（`RenderList`）を受け取って描くだけで、エディタのことを知らない。
- **所有権の明確化**: Actor / Component は `unique_ptr` で Scene / Actor が所有。破棄はフレーム末に遅延実行。GPU リソースはフェンスを待ってから解放（`DeferRelease`）。
- **アニメーション状態の分離**: 旧版は姿勢を共有の `Mesh` に書き込んでいたため、同じモデルを複数置くと全員が同じ動きになった。新版は `Animator` コンポーネントごとに姿勢を持つ。
- **グローバル変数の削減**: `gEngine` / `gGraphicsEngine` / エディタの extern 群を整理した。エディタ・シーン切替・フレームの組み立ては必要なものを受け取る形にし、`Xxx::Get()` は描画やアセットなど、コンポーネントから使うサービスに限っている。
- **1 クラス 1 ファイル**: モジュールの中をクラスごとのファイルと役割ごとのフォルダに分けた。

### 見つけて直した旧版のバグ
- `Matrix3` の行列式・逆行列の計算式の誤り（積が引き算になっていた）
- `Matrix4::operator*` がヘッダでは非 inline、cpp では inline という不整合
- `Mouse::GetButtonUp` / `GetButtonDown` の判定が逆
- `Matrix4` の逆行列がアフィン変換専用だった（一般の 4x4 に対応）
- 毎フレーム GPU の完了を待っていた（フレーム多重化で解消）

## 既知の制限
- 旧版のレベルファイル（`.rdlv`）と互換はない（型を番号で保存していたため）
- 影を落とすのは最初の平行光源 1 つだけ（点光源・スポットライトの影はない）
- ディファードでは物ごとのライトの数の制限（Lights Per Object）が効かない。描画の設定（Render Path など）は保存せず、起動のたびに既定に戻る
- 行動ツリーのノードエディタは、ノードのコピー&貼り付けに未対応（複製 Ctrl+D はできる）
- 動く物体のメッシュコライダーは凸包として当たる（凹んだ形は凹みが埋まる。Unity の Convex と同じ）。凸包でないメッシュ同士は頂点の食い込みで調べるので、辺と辺だけの交差は拾えない
- NavMesh を焼くのは MeshRenderer の三角形だけ（コライダー・NavMeshModifier・区画ごとの塗り分けは未対応）。面は四角形の集まりなので、Unity より三角形が多い
- UIImage の円形の塗りと Tiled の繰り返しは未対応
- 音声は Ogg Vorbis に未対応（Windows 標準では読めないため）
- ゲームのコードの読み直しはプレイ中にはしない（止めたときに読み直す）。エンジンのコードは読み直せない（エディタの再起動が必要）
