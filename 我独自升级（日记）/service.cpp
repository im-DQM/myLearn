#include "service.h"
#include "view.h"
#include <fstream>
#include <sstream>
#include <direct.h>
#include <locale>
#include <codecvt>
#include <windows.h>

std::wstring utf8_to_wstring(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    return result;
}

std::string wstring_to_utf8(const std::wstring& str) {
    if (str.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, &result[0], size, nullptr, nullptr);
    return result;
}

// 判断日记是否存在
bool haveThisDairy() {
    // 遍历日记列表
    for (const auto& dairy : list) {
        // 比较日期是否匹配
        if (dairy.year == curDairy.year && 
            dairy.month == curDairy.month && 
            dairy.day == curDairy.day) {
            // 找到匹配的日记，设置路径并返回true
            curDairy.path = dairy.path;
            return true;
        }
    }
    // 没有找到匹配的日记，返回false
    return false;
}

// 保存日记
void saveDairy(std::wstring str) {
    std::wstringstream filename;
    filename << L"dairy_" << curDairy.year << L"_";
    if (curDairy.month < 10) {
        filename << L"0" << curDairy.month;
    } else {
        filename << curDairy.month;
    }
    filename << L"_";
    if (curDairy.day < 10) {
        filename << L"0" << curDairy.day;
    } else {
        filename << curDairy.day;
    }
    filename << L".txt";
    
    std::wstring wfilename = filename.str();
    std::string sfilename(wfilename.begin(), wfilename.end());
    std::string filepath = "diaries/" + sfilename;
    
    _mkdir("diaries");
    
    std::ofstream file(filepath, std::ios::binary);
    if (file.is_open()) {
        std::string utf8content = wstring_to_utf8(str);
        file.write(utf8content.c_str(), utf8content.size());
        file.close();
    }
    
    curDairy.path = filepath;
    
    bool found = false;
    for (auto& dairy : list) {
        if (dairy.year == curDairy.year && dairy.month == curDairy.month && dairy.day == curDairy.day) {
            dairy.path = curDairy.path;
            found = true;
            break;
        }
    }
    if (!found) {
        Dairy newDairy;
        newDairy.year = curDairy.year;
        newDairy.month = curDairy.month;
        newDairy.day = curDairy.day;
        newDairy.path = curDairy.path;
        list.push_back(newDairy);
    }
    
    savePropertyToFile();
}

void loadDairyList() {
    list.clear();
    _mkdir("diaries");
    
    std::string searchPath = "diaries\\*.txt";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) return;
    
    do {
        std::string filename = findData.cFileName;
        if (filename.find("dairy_") != 0) continue;
        
        int year = 0, month = 0, day = 0;
        if (sscanf_s(filename.c_str(), "dairy_%d_%d_%d", &year, &month, &day) == 3) {
            Dairy d;
            d.year = year;
            d.month = month;
            d.day = day;
            d.path = "diaries/" + filename;
            list.push_back(d);
        }
    } while (FindNextFileA(hFind, &findData));
    
    FindClose(hFind);
}

const int PROPERTY_FILE_MAGIC = 0x50524F50;

void savePropertyToFile() {
    std::ofstream file("property.dat", std::ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(&PROPERTY_FILE_MAGIC), sizeof(int));
        file.write(reinterpret_cast<const char*>(&myProperty.mood), sizeof(int));
        file.write(reinterpret_cast<const char*>(&myProperty.health), sizeof(int));
        file.write(reinterpret_cast<const char*>(&myProperty.computer), sizeof(int));
        file.write(reinterpret_cast<const char*>(&myProperty.game), sizeof(int));
        file.write(reinterpret_cast<const char*>(&myProperty.charm), sizeof(int));
        file.write(reinterpret_cast<const char*>(&myProperty.computerLevel), sizeof(int));
        file.write(reinterpret_cast<const char*>(&myProperty.gameLevel), sizeof(int));
        file.write(reinterpret_cast<const char*>(&myProperty.charmLevel), sizeof(int));
        
        std::wstring wintro = utf8_to_wstring(myProperty.introduction);
        std::string utf8intro = wstring_to_utf8(wintro);
        int len = (int)utf8intro.size();
        file.write(reinterpret_cast<const char*>(&len), sizeof(int));
        file.write(utf8intro.c_str(), len);
        file.close();
    }
}

void loadPropertyFromFile() {
    std::ifstream file("property.dat", std::ios::binary);
    if (!file.is_open()) return;
    
    int magic = 0;
    file.read(reinterpret_cast<char*>(&magic), sizeof(int));
    
    if (magic == PROPERTY_FILE_MAGIC) {
        file.read(reinterpret_cast<char*>(&myProperty.mood), sizeof(int));
        file.read(reinterpret_cast<char*>(&myProperty.health), sizeof(int));
        file.read(reinterpret_cast<char*>(&myProperty.computer), sizeof(int));
        file.read(reinterpret_cast<char*>(&myProperty.game), sizeof(int));
        file.read(reinterpret_cast<char*>(&myProperty.charm), sizeof(int));
        file.read(reinterpret_cast<char*>(&myProperty.computerLevel), sizeof(int));
        file.read(reinterpret_cast<char*>(&myProperty.gameLevel), sizeof(int));
        file.read(reinterpret_cast<char*>(&myProperty.charmLevel), sizeof(int));
        
        int len = 0;
        if (file.peek() != EOF) {
            file.read(reinterpret_cast<char*>(&len), sizeof(int));
        }
        if (len > 0 && len < 10000) {
            std::string utf8intro(len, 0);
            file.read(&utf8intro[0], len);
            myProperty.introduction = std::string(utf8intro.begin(), utf8intro.end());
        }
    } else {
        file.seekg(0);
        file.read(reinterpret_cast<char*>(&myProperty.mood), sizeof(int));
        file.read(reinterpret_cast<char*>(&myProperty.health), sizeof(int));
        file.read(reinterpret_cast<char*>(&myProperty.computer), sizeof(int));
        file.read(reinterpret_cast<char*>(&myProperty.game), sizeof(int));
        file.read(reinterpret_cast<char*>(&myProperty.charm), sizeof(int));
        
        myProperty.computerLevel = 1;
        myProperty.gameLevel = 1;
        myProperty.charmLevel = 1;
        
        int len = 0;
        if (file.peek() != EOF) {
            file.read(reinterpret_cast<char*>(&len), sizeof(int));
        }
        if (len > 0 && len < 10000) {
            std::string utf8intro(len, 0);
            file.read(&utf8intro[0], len);
            myProperty.introduction = std::string(utf8intro.begin(), utf8intro.end());
        }
    }
    file.close();
}

// 修改属性
void applyPropertyDelta(std::wstring& str, WhitchProperty wp, int delta) {
    switch (wp) {
    case MOOD:
        myProperty.mood += delta;
        str += L"   心情" + std::wstring(delta > 0 ? L"+" : L"") + std::to_wstring(delta) + L"\n";
        break;

    case HEALTH:
        myProperty.health += delta;
        str += L"   健康" + std::wstring(delta > 0 ? L"+" : L"") + std::to_wstring(delta) + L"\n";
        break;

    case COMPUTER:
        myProperty.computer += delta;
        str += L"   编程" + std::wstring(delta > 0 ? L"+" : L"") + std::to_wstring(delta) + L"\n";
        while (myProperty.computer >= 100) {
            myProperty.computer -= 100;
            myProperty.computerLevel++;
            str += L"   ★ 编程升级！Lv." + std::to_wstring(myProperty.computerLevel) + L"\n";
        }
        while (myProperty.computer < 0 && myProperty.computerLevel > 1) {
            myProperty.computer += 100;
            myProperty.computerLevel--;
            str += L"   ★ 编程降级！Lv." + std::to_wstring(myProperty.computerLevel) + L"\n";
        }
        if (myProperty.computer < 0) myProperty.computer = 0;
        break;

    case GAME:
        myProperty.game += delta;
        str += L"   游戏" + std::wstring(delta > 0 ? L"+" : L"") + std::to_wstring(delta) + L"\n";
        while (myProperty.game >= 100) {
            myProperty.game -= 100;
            myProperty.gameLevel++;
            str += L"   ★ 游戏升级！Lv." + std::to_wstring(myProperty.gameLevel) + L"\n";
        }
        while (myProperty.game < 0 && myProperty.gameLevel > 1) {
            myProperty.game += 100;
            myProperty.gameLevel--;
            str += L"   ★ 游戏降级！Lv." + std::to_wstring(myProperty.gameLevel) + L"\n";
        }
        if (myProperty.game < 0) myProperty.game = 0;
        break;

    case CHARM:
        myProperty.charm += delta;
        str += L"   魅力" + std::wstring(delta > 0 ? L"+" : L"") + std::to_wstring(delta) + L"\n";
        while (myProperty.charm >= 100) {
            myProperty.charm -= 100;
            myProperty.charmLevel++;
            str += L"   ★ 魅力升级！Lv." + std::to_wstring(myProperty.charmLevel) + L"\n";
        }
        while (myProperty.charm < 0 && myProperty.charmLevel > 1) {
            myProperty.charm += 100;
            myProperty.charmLevel--;
            str += L"   ★ 魅力降级！Lv." + std::to_wstring(myProperty.charmLevel) + L"\n";
        }
        if (myProperty.charm < 0) myProperty.charm = 0;
        break;
    }
}

void changeProperty(std::wstring& str, WhitchProperty wp, bool b) {
    switch (wp) {
    case MOOD:
        myProperty.mood += b ? 2 : -2;
        str += b ? L"   心情+2\n" : L"    心情-2\n";
        break;

    case HEALTH:
        myProperty.health += b ? 2 : -2;
        str += b ? L"   健康+2\n" : L"    健康-2\n";
        break;

    case COMPUTER:
        myProperty.computer += b ? 5 : -5;
        str += b ? L"   计算机+5\n" : L"   计算机-5\n";
        while (myProperty.computer >= 100) {
            myProperty.computer -= 100;
            myProperty.computerLevel++;
            str += L"   ★ 编程升级！Lv." + std::to_wstring(myProperty.computerLevel) + L"\n";
        }
        while (myProperty.computer < 0 && myProperty.computerLevel > 1) {
            myProperty.computer += 100;
            myProperty.computerLevel--;
            str += L"   ★ 编程降级！Lv." + std::to_wstring(myProperty.computerLevel) + L"\n";
        }
        if (myProperty.computer < 0) myProperty.computer = 0;
        break;

    case GAME:
        myProperty.game += b ? 5 : -5;
        str += b ? L"   游戏+5\n" : L"    游戏-5\n";
        while (myProperty.game >= 100) {
            myProperty.game -= 100;
            myProperty.gameLevel++;
            str += L"   ★ 游戏升级！Lv." + std::to_wstring(myProperty.gameLevel) + L"\n";
        }
        while (myProperty.game < 0 && myProperty.gameLevel > 1) {
            myProperty.game += 100;
            myProperty.gameLevel--;
            str += L"   ★ 游戏降级！Lv." + std::to_wstring(myProperty.gameLevel) + L"\n";
        }
        if (myProperty.game < 0) myProperty.game = 0;
        break;

    case CHARM:
        myProperty.charm += b ? 1 : -1;
        str += b ? L"   魅力+1\n" : L"    魅力-1\n";
        while (myProperty.charm >= 100) {
            myProperty.charm -= 100;
            myProperty.charmLevel++;
            str += L"   ★ 魅力升级！Lv." + std::to_wstring(myProperty.charmLevel) + L"\n";
        }
        while (myProperty.charm < 0 && myProperty.charmLevel > 1) {
            myProperty.charm += 100;
            myProperty.charmLevel--;
            str += L"   ★ 魅力降级！Lv." + std::to_wstring(myProperty.charmLevel) + L"\n";
        }
        if (myProperty.charm < 0) myProperty.charm = 0;
        break;
    }
    return;
}

// 判断日期是否合理
bool isVailedData(int year, int month, int day) {
    // 判断年份是否合理（假设年份在 1-9999 之间）
    if (year < 1 || year > 9999) {
        return false;
    }
    
    // 判断月份是否合理
    if (month < 1 || month > 12) {
        return false;
    }
    
    // 判断日期是否合理
    if (day < 1) {
        return false;
    }
    
    // 每个月的天数
    int daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    
    // 判断是否为闰年，如果是闰年则 2 月为 29 天
    bool isLeapYear = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    if (isLeapYear) {
        daysInMonth[1] = 29;
    }
    
    // 判断日期是否超过当月最大天数
    if (day > daysInMonth[month - 1]) {
        return false;
    }
    
    return true;
}

void loadCustomEvents() {
    customEvents.clear();
    std::ifstream file("events.dat", std::ios::binary);
    if (!file.is_open()) return;

    int count = 0;
    file.read(reinterpret_cast<char*>(&count), sizeof(int));
    for (int i = 0; i < count && i < 100; i++) {
        CustomEvent ev;
        int nameLen = 0;
        file.read(reinterpret_cast<char*>(&nameLen), sizeof(int));
        if (nameLen > 0 && nameLen < 500) {
            std::string utf8name(nameLen, 0);
            file.read(&utf8name[0], nameLen);
            ev.name = utf8_to_wstring(utf8name);
        }
        file.read(reinterpret_cast<char*>(&ev.moodDelta), sizeof(int));
        file.read(reinterpret_cast<char*>(&ev.healthDelta), sizeof(int));
        file.read(reinterpret_cast<char*>(&ev.computerDelta), sizeof(int));
        file.read(reinterpret_cast<char*>(&ev.gameDelta), sizeof(int));
        file.read(reinterpret_cast<char*>(&ev.charmDelta), sizeof(int));
        customEvents.push_back(ev);
    }
    file.close();
}

void saveCustomEvents() {
    std::ofstream file("events.dat", std::ios::binary);
    if (!file.is_open()) return;

    int count = (int)customEvents.size();
    file.write(reinterpret_cast<const char*>(&count), sizeof(int));
    for (int i = 0; i < count; i++) {
        std::string utf8name = wstring_to_utf8(customEvents[i].name);
        int nameLen = (int)utf8name.size();
        file.write(reinterpret_cast<const char*>(&nameLen), sizeof(int));
        file.write(utf8name.c_str(), nameLen);
        file.write(reinterpret_cast<const char*>(&customEvents[i].moodDelta), sizeof(int));
        file.write(reinterpret_cast<const char*>(&customEvents[i].healthDelta), sizeof(int));
        file.write(reinterpret_cast<const char*>(&customEvents[i].computerDelta), sizeof(int));
        file.write(reinterpret_cast<const char*>(&customEvents[i].gameDelta), sizeof(int));
        file.write(reinterpret_cast<const char*>(&customEvents[i].charmDelta), sizeof(int));
    }
    file.close();
}

std::wstring loadDairyContent() {
    if (curDairy.path.empty()) return L"";
    std::ifstream file(curDairy.path, std::ios::binary);
    if (!file.is_open()) return L"";

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string rawContent = buffer.str();
    file.close();

    if (rawContent.size() >= 3 &&
        (unsigned char)rawContent[0] == 0xEF &&
        (unsigned char)rawContent[1] == 0xBB &&
        (unsigned char)rawContent[2] == 0xBF) {
        rawContent = rawContent.substr(3);
    }

    int utf8Size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        rawContent.c_str(), (int)rawContent.size(), nullptr, 0);
    if (utf8Size > 0) {
        return utf8_to_wstring(rawContent);
    }
    else {
        int acpSize = MultiByteToWideChar(CP_ACP, 0,
            rawContent.c_str(), (int)rawContent.size(), nullptr, 0);
        if (acpSize > 0) {
            std::wstring result(acpSize, 0);
            MultiByteToWideChar(CP_ACP, 0,
                rawContent.c_str(), (int)rawContent.size(), &result[0], acpSize);
            return result;
        }
    }
    return L"";
}

void runPages() {
    while (true) {
        switch (curPage) {
        case Page_Menu:
            menuView();
            break;
        case Page_Choose_Date:
            chooseDateMenu();
            break;
        case Page_Input_Data:
            dataMenu();
            break;
        case Page_Write_Dairy:
            dairyMenu();
            break;
        case Page_Final_Dairy:
            finalMenu();
            break;
        case Page_Search_Dairy:
            searchMenu();
            break;
        case Page_Property:
            personMenu();
            break;
        }
    }
}