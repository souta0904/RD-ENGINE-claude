# assimp をソースから静的ライブラリとしてビルドし、External/assimp を差し替える
#  使い方: powershell -File Tools\BuildAssimp.ps1 -Version v6.0.6
#  読み込めるのは OBJ / glTF / FBX だけにして小さくする（GitHub は 100MB を超えるファイルを受け付けないため）
#  Debug もデバッグ情報なし・軽い最適化でビルドする（assimp の中までステップ実行することはないので）
#  ランタイムは /MD・/MDd（エンジンと同じ）。zlib は assimp の lib にまとめて 1 本にする
param(
	[string]$Version = "v6.0.6",
	[string]$WorkDir = "$env:TEMP\rd_assimp_build"
)
# git や cmake は経過を stderr に出すので、Stop にすると 5.1 ではそれだけで止まってしまう。終了コードで調べる
$ErrorActionPreference = "Continue"

$vs = "C:\Program Files\Microsoft Visual Studio\18\Community"
$git = "$vs\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe"
$cmake = "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$msvc = Get-ChildItem "$vs\VC\Tools\MSVC" | Sort-Object Name | Select-Object -Last 1
$libExe = "$($msvc.FullName)\bin\Hostx64\x64\lib.exe"
$dest = Join-Path (Split-Path $PSScriptRoot -Parent) "External\assimp"

$src = "$WorkDir\src"
$bld = "$WorkDir\build"
if (-not (Test-Path $src))
{
	& $git -c advice.detachedHead=false clone --quiet --depth 1 --branch $Version https://github.com/assimp/assimp.git $src
	if ($LASTEXITCODE -ne 0) { throw "assimp $Version を取得できませんでした" }
}

# assimp の CMakeLists は Debug に /Zi /Od を足すので、それを外す
$cml = "$src\CMakeLists.txt"
$text = [IO.File]::ReadAllText($cml)
$text = $text.Replace('SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /D_DEBUG /Zi /Od")', 'SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /D_DEBUG")')
[IO.File]::WriteAllText($cml, $text)

& $cmake -S $src -B $bld -G "Visual Studio 18 2026" -A x64 `
	-DBUILD_SHARED_LIBS=OFF -DASSIMP_BUILD_TESTS=OFF -DASSIMP_BUILD_ASSIMP_TOOLS=OFF -DASSIMP_BUILD_SAMPLES=OFF `
	-DASSIMP_INSTALL=OFF -DASSIMP_INSTALL_PDB=OFF -DASSIMP_BUILD_ZLIB=ON -DASSIMP_NO_EXPORT=ON -DASSIMP_WARNINGS_AS_ERRORS=OFF `
	-DASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT=OFF `
	-DASSIMP_BUILD_OBJ_IMPORTER=ON -DASSIMP_BUILD_GLTF_IMPORTER=ON -DASSIMP_BUILD_FBX_IMPORTER=ON `
	"-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`$<`$<CONFIG:Debug>:Debug>DLL" `
	"-DCMAKE_CXX_FLAGS_DEBUG=/O1 /Ob1" "-DCMAKE_C_FLAGS_DEBUG=/O1 /Ob1"
if ($LASTEXITCODE -ne 0) { throw "cmake の構成に失敗しました" }
foreach ($config in "Release", "Debug")
{
	& $cmake --build $bld --config $config --parallel -- /v:minimal /nologo
	if ($LASTEXITCODE -ne 0) { throw "$config のビルドに失敗しました" }
}

# 古い lib とヘッダーを消してから置く
Remove-Item "$dest\lib", "$dest\include" -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force "$dest\lib\Release", "$dest\lib\Debug", "$dest\include" | Out-Null
& $libExe /nologo "/OUT:$dest\lib\Release\assimp-md.lib" "$bld\lib\Release\assimp-vc145-mt.lib" "$bld\contrib\zlib\Release\zlibstatic.lib"
& $libExe /nologo "/OUT:$dest\lib\Debug\assimp-mdd.lib" "$bld\lib\Debug\assimp-vc145-mtd.lib" "$bld\contrib\zlib\Debug\zlibstaticd.lib"
Copy-Item "$src\include\assimp" "$dest\include\assimp" -Recurse
# config.h と revision.h は CMake が作る
Copy-Item "$bld\include\assimp\*" "$dest\include\assimp" -Force
Copy-Item "$src\LICENSE" "$dest\LICENSE"
Set-Content -Path "$dest\VERSION.txt" -Value "assimp $Version (OBJ / glTF / FBX, static, /MD /MDd) - Tools/BuildAssimp.ps1 で作成" -Encoding utf8
Get-ChildItem "$dest\lib" -Recurse -File | Select-Object FullName, Length
