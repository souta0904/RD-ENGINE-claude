# エンジンのモジュール間の依存（#include）が層の決まりを守っているか調べる
#   使い方:  powershell -ExecutionPolicy Bypass -File Tools\CheckDependencies.ps1
#   違反があれば一覧を出して終了コード 1 を返す
#
# 層（下の層は上の層を include しない）
#   Math, Core            基礎（数学・ログ・JSON・ジョブ）
#   Platform, RHI         OS / GPU
#   Asset                 アセットの読み込み（テクスチャ・モデル・音声データ）
#   Render, Audio         描画・音の再生
#   Scene                 Actor / Component / Scene（コンポーネントの具体的な型は知らない）
#   Components            描画・音・アニメーションの組み込みコンポーネント
#   Physics               物理ワールドと Collider / Rigidbody（シーンのシステムとして動く）
#   Runtime               エンジンの組み立て（シーン切替・フレームの組み立て）
#   Editor                エディタ（最上位。Engine.cpp と同じく何でも使える）

$allowed = @{
    'Math'       = @('Core')
    'Core'       = @('Math')
    'Platform'   = @('Core', 'Math')
    'RHI'        = @('Core', 'Math', 'Platform')
    'Asset'      = @('Core', 'Math', 'RHI')
    'Render'     = @('Core', 'Math', 'RHI', 'Asset')
    'Audio'      = @('Core', 'Math', 'Asset')
    'Scene'      = @('Core', 'Math', 'Asset', 'Render')
    'Components' = @('Core', 'Math', 'Platform', 'RHI', 'Asset', 'Render', 'Audio', 'Scene')
    'Physics'    = @('Core', 'Math', 'Asset', 'Render', 'Scene', 'Components')
    'Runtime'    = @('Core', 'Math', 'Platform', 'RHI', 'Asset', 'Render', 'Audio', 'Scene', 'Components', 'Physics')
    'Editor'     = @('*')
}

$source = Join-Path $PSScriptRoot '..\Engine\Source' | Resolve-Path
$violations = @()
Get-ChildItem $source -Recurse -Include *.h, *.cpp | ForEach-Object {
    $relative = $_.FullName.Substring($source.Path.Length + 1)
    $module = $relative.Split('\')[0]
    if (-not $allowed.ContainsKey($module)) { return }# ルート（Engine.cpp など）は対象外
    $rules = $allowed[$module]
    if ($rules -contains '*') { return }
    $lineNo = 0
    foreach ($line in [IO.File]::ReadAllLines($_.FullName)) {
        $lineNo++
        if ($line -match '^\s*#include\s+"(\w+)/') {
            $target = $Matches[1]
            if ($target -ne $module -and $allowed.ContainsKey($target) -and $rules -notcontains $target) {
                $violations += "{0}:{1}  {2} -> {3}" -f $relative, $lineNo, $module, $target
            }
        }
        elseif ($line -match '^\s*#include\s+"(Engine|EngineConfig)\.h"') {
            $violations += "{0}:{1}  {2} -> Engine (最上位)" -f $relative, $lineNo, $module
        }
    }
}

if ($violations.Count -gt 0) {
    Write-Host "依存の決まりに違反しています ($($violations.Count) 件):" -ForegroundColor Red
    $violations | ForEach-Object { Write-Host "  $_" }
    exit 1
}
Write-Host "OK: 依存の決まりを守っています" -ForegroundColor Green
exit 0
