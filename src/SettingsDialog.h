#pragma once
#include <windows.h>
#include "Config/AppSettings.h"

// Показывает модальный диалог настроек.
// Возвращает true если пользователь нажал OK.
bool ShowSettingsDialog(HWND hParent, HINSTANCE hInstance, AppSettings& settings);
