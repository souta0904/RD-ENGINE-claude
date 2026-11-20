#include "GameLauncher.h"
#include <Windows.h>

// エディタの起動用の exe（RdEditor.exe -game <名前>）
//  中身はエンジンの RunEditorLauncher に任せる。ゲームのコードは <名前>.dll にあり、
//  エディタを開いたまま読み直せるよう、この exe には入れない
int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
	return rd::RunEditorLauncher();
}
