#include <graphics.h>
#include "service.h"
#include "frame.h"
#include "view.h"

// 定义全局变量
Page curPage = Page_Menu;
Property myProperty;
std::vector<Dairy> list;
Dairy curDairy = { 0, 0, 0, "" };
std::vector<CustomEvent> customEvents;
bool loadExistingDairy = false;

int main() {
    FreeConsole();

    initgraph(800, 600);

    SetWindowText(GetHWnd(), L"我独自升级");

    initClickSound();

    loadDairyList();
    loadPropertyFromFile();
    loadCustomEvents();

    // 开始为菜单
    curPage = Page_Menu;

    // 调用运行函数
    runPages();

    // 关闭图形界面
    closegraph();
    return 0;
}