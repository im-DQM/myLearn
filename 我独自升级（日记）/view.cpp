#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include "view.h"
#include"service.h"
#include <locale>
#include <codecvt>
#include <fstream>
#include <sstream>
#include <cmath>
#include <mmsystem.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

static IMAGE g_bgImage;
static IMAGE g_bgScaled;
static bool g_bgLoaded = false;

static std::wstring getExeDir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring p(path);
    return p.substr(0, p.find_last_of(L"\\/") + 1);
}

static void loadBgImage() {
    if (g_bgLoaded) return;
    std::wstring path = getExeDir() + L"background.png";
    loadimage(&g_bgImage, path.c_str());
    g_bgLoaded = (g_bgImage.getwidth() > 0 && g_bgImage.getheight() > 0);
}

static BYTE* g_wavData = nullptr;
static DWORD g_wavSize = 0;

void initClickSound() {
    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) return;

    std::wstring filePath = getExeDir() + L"switch-button.mp3";

    IMFSourceReader* pReader = nullptr;
    hr = MFCreateSourceReaderFromURL(filePath.c_str(), nullptr, &pReader);
    if (FAILED(hr)) { MFShutdown(); return; }

    IMFMediaType* pMediaType = nullptr;
    MFCreateMediaType(&pMediaType);
    pMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    pMediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    hr = pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pMediaType);
    pMediaType->Release();

    if (FAILED(hr)) { pReader->Release(); MFShutdown(); return; }

    IMFMediaType* pActualType = nullptr;
    pReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pActualType);
    UINT32 waveFormatSize = 0;
    WAVEFORMATEX* pWaveFormat = nullptr;
    MFCreateWaveFormatExFromMFMediaType(pActualType, &pWaveFormat, &waveFormatSize);
    pActualType->Release();

    std::vector<BYTE> pcmBuffer;
    while (true) {
        DWORD flags = 0;
        IMFSample* pSample = nullptr;
        hr = pReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &pSample);
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) break;
        if (pSample) {
            IMFMediaBuffer* pBuffer = nullptr;
            pSample->ConvertToContiguousBuffer(&pBuffer);
            BYTE* pData = nullptr;
            DWORD dataSize = 0;
            pBuffer->Lock(&pData, nullptr, &dataSize);
            pcmBuffer.insert(pcmBuffer.end(), pData, pData + dataSize);
            pBuffer->Unlock();
            pBuffer->Release();
            pSample->Release();
        }
    }
    pReader->Release();
    MFShutdown();

    g_wavSize = 44 + (DWORD)pcmBuffer.size();
    g_wavData = new BYTE[g_wavSize];

    memcpy(g_wavData, "RIFF", 4);
    *(DWORD*)(g_wavData + 4) = g_wavSize - 8;
    memcpy(g_wavData + 8, "WAVE", 4);
    memcpy(g_wavData + 12, "fmt ", 4);
    *(DWORD*)(g_wavData + 16) = 16;
    *(WORD*)(g_wavData + 20) = pWaveFormat->wFormatTag;
    *(WORD*)(g_wavData + 22) = pWaveFormat->nChannels;
    *(DWORD*)(g_wavData + 24) = pWaveFormat->nSamplesPerSec;
    *(DWORD*)(g_wavData + 28) = pWaveFormat->nAvgBytesPerSec;
    *(WORD*)(g_wavData + 32) = pWaveFormat->nBlockAlign;
    *(WORD*)(g_wavData + 34) = pWaveFormat->wBitsPerSample;
    memcpy(g_wavData + 36, "data", 4);
    *(DWORD*)(g_wavData + 40) = (DWORD)pcmBuffer.size();
    memcpy(g_wavData + 44, pcmBuffer.data(), pcmBuffer.size());

    CoTaskMemFree(pWaveFormat);
}

static void playClickSound() {
    if (g_wavData) {
        PlaySound((LPCWSTR)g_wavData, nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
    }
}

static COLORREF lightenColor(COLORREF c, int amount) {
    int r = min(GetRValue(c) + amount, 255);
    int g = min(GetGValue(c) + amount, 255);
    int b = min(GetBValue(c) + amount, 255);
    return RGB(r, g, b);
}

static std::wstring stringToWstring(const std::string& str) {
    try {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        return converter.from_bytes(str);
    } catch (...) {
        std::wstring result;
        for (unsigned char c : str) {
            result += (wchar_t)c;
        }
        return result;
    }
}

void drawGradientBg(COLORREF topColor, COLORREF bottomColor) {
    loadBgImage();
    int w = getwidth();
    int h = getheight();

    if (g_bgLoaded) {
        int imgW = g_bgImage.getwidth();
        int imgH = g_bgImage.getheight();
        if (imgW != w || imgH != h) {
            g_bgScaled.Resize(w, h);
            DWORD* srcBuf = GetImageBuffer(&g_bgImage);
            DWORD* dstBuf = GetImageBuffer(&g_bgScaled);
            for (int y = 0; y < h; y++) {
                int srcY = y * imgH / h;
                for (int x = 0; x < w; x++) {
                    int srcX = x * imgW / w;
                    dstBuf[y * w + x] = srcBuf[srcY * imgW + srcX];
                }
            }
            putimage(0, 0, &g_bgScaled);
        } else {
            putimage(0, 0, &g_bgImage);
        }

        DWORD* pixels = GetImageBuffer();
        int r1 = GetRValue(topColor), g1 = GetGValue(topColor), b1 = GetBValue(topColor);
        int r2 = GetRValue(bottomColor), g2 = GetGValue(bottomColor), b2 = GetBValue(bottomColor);
        for (int y = 0; y < h; y++) {
            int r = r1 + (r2 - r1) * y / h;
            int g = g1 + (g2 - g1) * y / h;
            int b = b1 + (b2 - b1) * y / h;
            for (int x = 0; x < w; x++) {
                int idx = y * w + x;
                BYTE pr = GetRValue(pixels[idx]);
                BYTE pg = GetGValue(pixels[idx]);
                BYTE pb = GetBValue(pixels[idx]);
                int nr = (pr * 55 + r * 45) / 100;
                int ng = (pg * 55 + g * 45) / 100;
                int nb = (pb * 55 + b * 45) / 100;
                pixels[idx] = RGB(nr, ng, nb);
            }
        }
    } else {
        int r1 = GetRValue(topColor), g1 = GetGValue(topColor), b1 = GetBValue(topColor);
        int r2 = GetRValue(bottomColor), g2 = GetGValue(bottomColor), b2 = GetBValue(bottomColor);
        for (int y = 0; y < h; y++) {
            int r = r1 + (r2 - r1) * y / h;
            int g = g1 + (g2 - g1) * y / h;
            int b = b1 + (b2 - b1) * y / h;
            setlinecolor(RGB(r, g, b));
            line(0, y, w, y);
        }
    }
}

void drawCard(int x, int y, int w, int h, int radius) {
    setfillcolor(RGB(200, 215, 240));
    solidroundrect(x + 3, y + 3, x + w + 3, y + h + 3, radius, radius);

    IMAGE cardImg;
    cardImg.Resize(w, h);
    DWORD* buf = GetImageBuffer(&cardImg);
    for (int cy = 0; cy < h; cy++) {
        int r = 240 + (248 - 240) * cy / h;
        int g = 245 + (252 - 245) * cy / h;
        int b = 255;
        for (int cx = 0; cx < w; cx++) {
            buf[cy * w + cx] = RGB(r, g, b);
        }
    }
    putimage(x, y, &cardImg);

    setlinecolor(RGB(190, 210, 240));
    roundrect(x, y, x + w, y + h, radius, radius);
}

void drawShadowText(int x, int y, const TCHAR* text, COLORREF shadowColor, COLORREF txtColor) {
    settextcolor(shadowColor);
    outtextxy(x + 2, y + 2, text);
    settextcolor(txtColor);
    outtextxy(x, y, text);
}

Button::Button() : x(0), y(0), width(0), height(0), text(""),
    bgColor(RGB(102, 126, 234)), hoverColor(RGB(86, 100, 180)), textColor(WHITE), hovered(false) {}

Button::Button(int x, int y, int width, int height, std::string text,
    COLORREF bg, COLORREF hover, COLORREF txt)
    : x(x), y(y), width(width), height(height), text(text),
    bgColor(bg), hoverColor(hover), textColor(txt), hovered(false) {}

void Button::draw() {
    setfillcolor(RGB(180, 180, 200));
    solidroundrect(x + 2, y + 2, x + width + 2, y + height + 2, 12, 12);

    COLORREF topC = hovered ? lightenColor(bgColor, 35) : bgColor;
    int r1 = GetRValue(topC), g1 = GetGValue(topC), b1 = GetBValue(topC);
    int r2 = r1 * 85 / 100, g2 = g1 * 85 / 100, b2 = b1 * 85 / 100;
    for (int i = 0; i < height; i++) {
        int r = r1 + (r2 - r1) * i / height;
        int g = g1 + (g2 - g1) * i / height;
        int b = b1 + (b2 - b1) * i / height;
        setlinecolor(RGB(r, g, b));
        line(x + 1, y + i, x + width - 1, y + i);
    }

    if (hovered) {
        setlinecolor(lightenColor(bgColor, 60));
        setlinestyle(PS_SOLID, 2);
    } else {
        setlinecolor(RGB(r1 * 70 / 100, g1 * 70 / 100, b1 * 70 / 100));
        setlinestyle(PS_SOLID, 1);
    }
    roundrect(x, y, x + width, y + height, 12, 12);
    setlinestyle(PS_SOLID, 1);

    settextcolor(textColor);
    settextstyle(16, 0, L"微软雅黑");
    setbkmode(TRANSPARENT);
    std::wstring wtext = stringToWstring(text);
    int tw = textwidth(wtext.c_str());
    int th = textheight(wtext.c_str());
    outtextxy(x + (width - tw) / 2, y + (height - th) / 2, wtext.c_str());
}

bool Button::checkClick(const ExMessage msg) {
    if (msg.message == WM_LBUTTONDOWN) {
        if (msg.x >= x && msg.x <= x + width && msg.y >= y && msg.y <= y + height) {
            playClickSound();
            setfillcolor(RGB(120, 120, 140));
            solidroundrect(x + 2, y + 2, x + width + 2, y + height + 2, 12, 12);
            setfillcolor(hoverColor);
            solidroundrect(x + 1, y + 1, x + width - 1, y + height - 1, 12, 12);
            settextcolor(textColor);
            settextstyle(16, 0, L"微软雅黑");
            setbkmode(TRANSPARENT);
            std::wstring wtext = stringToWstring(text);
            int tw = textwidth(wtext.c_str());
            int th = textheight(wtext.c_str());
            outtextxy(x + (width - tw) / 2, y + (height - th) / 2, wtext.c_str());
            return true;
        }
    }
    else if (msg.message == WM_LBUTTONUP) {
        draw();
        if (msg.x >= x && msg.x <= x + width && msg.y >= y && msg.y <= y + height) {
            return true;
        }
    }
    return false;
}

void Button::updateHover(int mouseX, int mouseY) {
    bool wasHovered = hovered;
    hovered = (mouseX >= x && mouseX <= x + width && mouseY >= y && mouseY <= y + height);
    if (hovered != wasHovered) {
        draw();
        SetCursor(LoadCursor(nullptr, hovered ? MAKEINTRESOURCE(32649) : IDC_ARROW));
    }
}

void redrawDiaryContent(int x, int y, int width, int height, const std::wstring& content, int maxChars, int lineH) {
    IMAGE areaImg;
    areaImg.Resize(width - 2, height - 2);
    DWORD* buf = GetImageBuffer(&areaImg);
    int aw = width - 2, ah = height - 2;
    for (int cy = 0; cy < ah; cy++) {
        int r = 242 + (250 - 242) * cy / ah;
        int g = 246 + (253 - 246) * cy / ah;
        int b = 255;
        for (int cx = 0; cx < aw; cx++) {
            buf[cy * aw + cx] = RGB(r, g, b);
        }
    }
    putimage(x + 1, y + 1, &areaImg);

    settextcolor(RGB(50, 50, 60));
    settextstyle(16, 0, L"微软雅黑");
    setbkmode(TRANSPARENT);
    std::wstring line;
    int currentY = y + 15;
    for (wchar_t c : content) {
        if (c == L'\n') {
            outtextxy(x + 15, currentY, line.c_str());
            line.clear();
            currentY += lineH;
            if (currentY > y + height - 15) break;
        } else {
            line += c;
            if (line.length() >= maxChars) {
                outtextxy(x + 15, currentY, line.c_str());
                line.clear();
                currentY += lineH;
                if (currentY > y + height - 15) break;
            }
        }
    }
    if (!line.empty() && currentY <= y + height - 15) {
        outtextxy(x + 15, currentY, line.c_str());
    }
}

void chooseDateMenu() {
    drawGradientBg(RGB(232, 244, 253), RGB(245, 237, 255));

    drawCard(150, 80, 500, 380);

    settextstyle(26, 0, L"微软雅黑");
    setbkmode(TRANSPARENT);
    std::wstring title = L"选择日期方式";
    drawShadowText((getwidth() - textwidth(title.c_str())) / 2, 110, title.c_str());

    SYSTEMTIME st;
    GetLocalTime(&st);

    settextcolor(RGB(120, 140, 180));
    settextstyle(16, 0, L"微软雅黑");
    TCHAR todayStr[50];
    swprintf(todayStr, 50, L"今天是 %d年%d月%d日", st.wYear, st.wMonth, st.wDay);
    outtextxy((getwidth() - textwidth(todayStr)) / 2, 165, todayStr);

    int buttonWidth = 220;
    int buttonHeight = 50;
    int centerX = (getwidth() - buttonWidth) / 2;

    Button btnToday(centerX, 220, buttonWidth, buttonHeight, "使用当前日期", RGB(102, 156, 234), RGB(80, 120, 200));
    Button btnManual(centerX, 300, buttonWidth, buttonHeight, "手动输入日期", RGB(80, 190, 170), RGB(60, 160, 140));
    Button btnBack(centerX, 380, buttonWidth, buttonHeight, "返回菜单", RGB(160, 170, 190), RGB(130, 140, 160));

    btnToday.draw();
    btnManual.draw();
    btnBack.draw();

    ExMessage msg;
    while (true) {
        if (peekmessage(&msg, EX_MOUSE)) {
            if (msg.message == WM_MOUSEMOVE) {
                btnToday.updateHover(msg.x, msg.y);
                btnManual.updateHover(msg.x, msg.y);
                btnBack.updateHover(msg.x, msg.y);
            }
            if (btnToday.checkClick(msg)) {
                if (msg.message == WM_LBUTTONUP) {
                    curDairy.year = st.wYear;
                    curDairy.month = st.wMonth;
                    curDairy.day = st.wDay;
                    curDairy.path = "";

                    if (haveThisDairy()) {
                        int result = MessageBox(GetHWnd(),
                            L"该日期已有日记，请选择操作：\n\n是(Y) - 查看日记\n否(N) - 继续修改日记\n取消 - 返回",
                            L"日记已存在", MB_YESNOCANCEL | MB_ICONQUESTION);
                        if (result == IDYES) {
                            curPage = Page_Final_Dairy;
                            return;
                        } else if (result == IDNO) {
                            loadExistingDairy = true;
                            curPage = Page_Write_Dairy;
                            return;
                        }
                        continue;
                    }

                    curPage = Page_Write_Dairy;
                    return;
                }
            }
            if (btnManual.checkClick(msg)) {
                if (msg.message == WM_LBUTTONUP) {
                    curPage = Page_Input_Data;
                    return;
                }
            }
            if (btnBack.checkClick(msg)) {
                if (msg.message == WM_LBUTTONUP) {
                    curPage = Page_Menu;
                    return;
                }
            }
        }
        Sleep(10);
    }
}

void dataMenu() {
    std::wstring yearText, monthText, dayText;
    int activeField = 0;

    Button btnConfirm(250, 410, 130, 42, "确认", RGB(102, 126, 234), RGB(80, 100, 200));
    Button btnBack(420, 410, 130, 42, "返回", RGB(160, 170, 190), RGB(130, 140, 160));

    auto redrawDataMenu = [&]() {
        drawGradientBg(RGB(232, 244, 253), RGB(245, 237, 255));
        drawCard(100, 50, 600, 440);

        settextstyle(26, 0, L"微软雅黑");
        setbkmode(TRANSPARENT);
        std::wstring title = L"输入日记日期";
        drawShadowText((getwidth() - textwidth(title.c_str())) / 2, 80, title.c_str());

        settextcolor(RGB(120, 140, 180));
        settextstyle(13, 0, L"微软雅黑");
        outtextxy((getwidth() - textwidth(L"点击输入框后键入数字，Tab 切换字段")) / 2, 125, L"点击输入框后键入数字，Tab 切换字段");

        struct Field { const wchar_t* label; std::wstring* text; int y; };
        Field fields[] = {
            { L"年  份", &yearText, 175 },
            { L"月  份", &monthText, 255 },
            { L"日  期", &dayText, 335 }
        };

        for (int i = 0; i < 3; i++) {
            int labelX = 160;
            int inputX = 280;
            int inputW = 280;
            int inputH = 42;
            int cy = fields[i].y;

            settextcolor(RGB(80, 100, 140));
            settextstyle(18, 0, L"微软雅黑");
            outtextxy(labelX, cy + 10, fields[i].label);

            if (i == activeField) {
                IMAGE inputImg;
                inputImg.Resize(inputW, inputH);
                DWORD* ibuf = GetImageBuffer(&inputImg);
                for (int iy = 0; iy < inputH; iy++) {
                    int ir = 235 + (245 - 235) * iy / inputH;
                    int ig = 240 + (250 - 240) * iy / inputH;
                    int ib = 255;
                    for (int ix = 0; ix < inputW; ix++) {
                        ibuf[iy * inputW + ix] = RGB(ir, ig, ib);
                    }
                }
                putimage(inputX, cy, &inputImg);
                setlinecolor(RGB(102, 126, 234));
            } else {
                IMAGE inputImg;
                inputImg.Resize(inputW, inputH);
                DWORD* ibuf = GetImageBuffer(&inputImg);
                for (int iy = 0; iy < inputH; iy++) {
                    int ir = 245 + (252 - 245) * iy / inputH;
                    int ig = 248 + (254 - 248) * iy / inputH;
                    int ib = 255;
                    for (int ix = 0; ix < inputW; ix++) {
                        ibuf[iy * inputW + ix] = RGB(ir, ig, ib);
                    }
                }
                putimage(inputX, cy, &inputImg);
                setlinecolor(RGB(190, 210, 240));
            }
            roundrect(inputX, cy, inputX + inputW, cy + inputH, 8, 8);

            settextcolor(RGB(50, 60, 90));
            settextstyle(18, 0, L"微软雅黑");
            std::wstring display = *fields[i].text;
            if (i == activeField) display += L"|";
            outtextxy(inputX + 15, cy + 10, display.c_str());
        }

        btnConfirm.draw();
        btnBack.draw();
    };

    redrawDataMenu();

    ExMessage msg;
    while (true) {
        if (peekmessage(&msg, EX_MOUSE | EX_CHAR)) {
            if (msg.message == WM_MOUSEMOVE) {
                btnConfirm.updateHover(msg.x, msg.y);
                btnBack.updateHover(msg.x, msg.y);
            }
            if (msg.message == WM_LBUTTONDOWN) {
                int inputX = 280, inputW = 280, inputH = 42;
                int fieldYs[] = { 175, 255, 335 };
                bool hit = false;
                for (int i = 0; i < 3; i++) {
                    if (msg.x >= inputX && msg.x <= inputX + inputW &&
                        msg.y >= fieldYs[i] && msg.y <= fieldYs[i] + inputH) {
                        activeField = i;
                        hit = true;
                        redrawDataMenu();
                        break;
                    }
                }
                if (!hit) {
                    if (btnConfirm.checkClick(msg)) {
                        int year = _wtoi(yearText.c_str());
                        int month = _wtoi(monthText.c_str());
                        int day = _wtoi(dayText.c_str());
                        if (isVailedData(year, month, day)) {
                            curDairy.year = year;
                            curDairy.month = month;
                            curDairy.day = day;
                            curDairy.path = "";
                            if (haveThisDairy()) {
                                int result = MessageBox(GetHWnd(),
                                    L"该日期已有日记，请选择操作：\n\n是(Y) - 查看日记\n否(N) - 继续修改日记\n取消 - 返回",
                                    L"日记已存在", MB_YESNOCANCEL | MB_ICONQUESTION);
                                if (result == IDYES) {
                                    curPage = Page_Final_Dairy;
                                    return;
                                } else if (result == IDNO) {
                                    loadExistingDairy = true;
                                    curPage = Page_Write_Dairy;
                                    return;
                                }
                            } else {
                                curPage = Page_Write_Dairy;
                                return;
                            }
                        } else {
                            MessageBox(GetHWnd(), L"日期输入不合理，请重新输入！", L"错误", MB_OK | MB_ICONERROR);
                        }
                    }
                    if (btnBack.checkClick(msg)) {
                        curPage = Page_Choose_Date;
                        return;
                    }
                }
            }

            if (msg.message == WM_CHAR) {
                std::wstring* current = nullptr;
                if (activeField == 0) current = &yearText;
                else if (activeField == 1) current = &monthText;
                else if (activeField == 2) current = &dayText;

                if (current) {
                    if (msg.vkcode == '\b') {
                        if (!current->empty()) current->pop_back();
                    } else if (msg.vkcode == '\t') {
                        activeField = (activeField + 1) % 3;
                    } else if (msg.vkcode == '\r') {
                        int year = _wtoi(yearText.c_str());
                        int month = _wtoi(monthText.c_str());
                        int day = _wtoi(dayText.c_str());
                        if (isVailedData(year, month, day)) {
                            curDairy.year = year;
                            curDairy.month = month;
                            curDairy.day = day;
                            curDairy.path = "";
                            if (haveThisDairy()) {
                                int result = MessageBox(GetHWnd(),
                                    L"该日期已有日记，请选择操作：\n\n是(Y) - 查看日记\n否(N) - 继续修改日记\n取消 - 返回",
                                    L"日记已存在", MB_YESNOCANCEL | MB_ICONQUESTION);
                                if (result == IDYES) {
                                    curPage = Page_Final_Dairy;
                                    return;
                                } else if (result == IDNO) {
                                    loadExistingDairy = true;
                                    curPage = Page_Write_Dairy;
                                    return;
                                }
                            } else {
                                curPage = Page_Write_Dairy;
                                return;
                            }
                        } else {
                            MessageBox(GetHWnd(), L"日期输入不合理，请重新输入！", L"错误", MB_OK | MB_ICONERROR);
                        }
                    } else if (msg.ch >= L'0' && msg.ch <= L'9') {
                        int maxLen = (activeField == 0) ? 4 : 2;
                        if ((int)current->length() < maxLen) {
                            *current += msg.ch;
                        }
                    }
                }
                redrawDataMenu();
            }
        }
        Sleep(10);
    }
}

void dairyMenu() {
    std::wstring diaryContent = L"";
    if (loadExistingDairy) {
        diaryContent = loadDairyContent();
        loadExistingDairy = false;
    }
    int eventScrollOffset = 0;
    bool showAddModal = false;
    bool manageMode = false;
    int hoveredEventIdx = -1;

    std::wstring newEventName;
    int newEventDeltas[5] = { 0 };
    bool nameFieldActive = false;

    const int LP_W = 165;
    const int RX = LP_W;

    auto checkLevelUp = [&](const wchar_t* propName, int& value, int& level) {
        while (value >= 100) {
            value -= 100;
            level++;
            diaryContent += std::wstring(L"   ★ ") + propName + L"升级！Lv." + std::to_wstring(level) + L"\n";
        }
        while (value < 0 && level > 1) {
            value += 100;
            level--;
            diaryContent += std::wstring(L"   ★ ") + propName + L"降级！Lv." + std::to_wstring(level) + L"\n";
        }
        if (value < 0) value = 0;
    };

    auto getEventSummary = [](const CustomEvent& ev) -> std::wstring {
        std::wstring summary;
        const wchar_t* names[] = { L"心情", L"健康", L"编程", L"游戏", L"魅力" };
        int deltas[] = { ev.moodDelta, ev.healthDelta, ev.computerDelta, ev.gameDelta, ev.charmDelta };
        for (int i = 0; i < 5; i++) {
            if (deltas[i] != 0) {
                if (!summary.empty()) summary += L" ";
                summary += names[i];
                if (deltas[i] > 0) summary += L"+";
                summary += std::to_wstring(deltas[i]);
            }
        }
        return summary;
    };

    auto applyEvent = [&](const CustomEvent& ev) {
        diaryContent += ev.name + L"\n";
        if (ev.moodDelta != 0) {
            diaryContent += L"   心情" + std::wstring(ev.moodDelta > 0 ? L"+" : L"") + std::to_wstring(ev.moodDelta) + L"\n";
            myProperty.mood += ev.moodDelta;
        }
        if (ev.healthDelta != 0) {
            diaryContent += L"   健康" + std::wstring(ev.healthDelta > 0 ? L"+" : L"") + std::to_wstring(ev.healthDelta) + L"\n";
            myProperty.health += ev.healthDelta;
        }
        if (ev.computerDelta != 0) {
            diaryContent += L"   编程" + std::wstring(ev.computerDelta > 0 ? L"+" : L"") + std::to_wstring(ev.computerDelta) + L"\n";
            myProperty.computer += ev.computerDelta;
            checkLevelUp(L"编程", myProperty.computer, myProperty.computerLevel);
        }
        if (ev.gameDelta != 0) {
            diaryContent += L"   游戏" + std::wstring(ev.gameDelta > 0 ? L"+" : L"") + std::to_wstring(ev.gameDelta) + L"\n";
            myProperty.game += ev.gameDelta;
            checkLevelUp(L"游戏", myProperty.game, myProperty.gameLevel);
        }
        if (ev.charmDelta != 0) {
            diaryContent += L"   魅力" + std::wstring(ev.charmDelta > 0 ? L"+" : L"") + std::to_wstring(ev.charmDelta) + L"\n";
            myProperty.charm += ev.charmDelta;
            checkLevelUp(L"魅力", myProperty.charm, myProperty.charmLevel);
        }
    };

    int propBtnWidth = 65;
    int propBtnHeight = 30;
    int propStartX = RX + 20;
    int propStartY = 88;
    int propGap = 30;

    int diaryAreaX = RX + 10;
    int diaryAreaY = 175;
    int diaryAreaWidth = 800 - RX - 20;
    int diaryAreaHeight = 600 - 175 - 65;
    int lineHeight = 25;
    int maxCharsPerLine = diaryAreaWidth / 12;

    auto drawLeftPanel = [&]() {
        drawCard(5, 5, LP_W - 10, 590);

        settextstyle(16, 0, L"微软雅黑");
        setbkmode(TRANSPARENT);
        settextcolor(RGB(60, 80, 130));
        std::wstring lpTitle = L"常用事件";
        outtextxy((LP_W - textwidth(lpTitle.c_str())) / 2, 20, lpTitle.c_str());

        int itemX = 10;
        int itemW = LP_W - 20;
        int itemH = 42;
        int itemGap = 5;
        int listTop = 50;
        int listBottom = 510;

        HRGN clipRgn = CreateRectRgn(itemX, listTop, itemX + itemW, listBottom);
        setcliprgn(clipRgn);
        DeleteObject(clipRgn);

        for (size_t i = 0; i < customEvents.size(); i++) {
            int itemY = listTop + (int)i * (itemH + itemGap) - eventScrollOffset;
            if (itemY + itemH < listTop || itemY > listBottom) continue;

            bool isHovered = ((int)i == hoveredEventIdx && !showAddModal);

            if (isHovered) {
                setfillcolor(RGB(210, 225, 248));
            }
            else {
                setfillcolor(RGB(225, 235, 252));
            }
            solidroundrect(itemX, itemY, itemX + itemW, itemY + itemH, 6, 6);
            setlinecolor(RGB(190, 210, 240));
            roundrect(itemX, itemY, itemX + itemW, itemY + itemH, 6, 6);

            settextcolor(RGB(50, 60, 100));
            settextstyle(13, 0, L"微软雅黑");
            std::wstring displayName = customEvents[i].name;
            if (textwidth(displayName.c_str()) > itemW - 16) {
                while (textwidth(displayName.c_str()) > itemW - 30 && displayName.size() > 1) {
                    displayName.pop_back();
                }
                displayName += L"...";
            }
            outtextxy(itemX + 8, itemY + 5, displayName.c_str());

            std::wstring summary = getEventSummary(customEvents[i]);
            if (!summary.empty()) {
                settextcolor(RGB(120, 140, 180));
                settextstyle(10, 0, L"微软雅黑");
                if (textwidth(summary.c_str()) > itemW - 16) {
                    while (textwidth(summary.c_str()) > itemW - 30 && summary.size() > 1) {
                        summary.pop_back();
                    }
                    summary += L"...";
                }
                outtextxy(itemX + 8, itemY + 24, summary.c_str());
            }

            if (manageMode) {
                setfillcolor(RGB(220, 80, 80));
                int delX = itemX + itemW - 18;
                int delY = itemY + 3;
                solidroundrect(delX, delY, delX + 15, delY + 15, 3, 3);
                settextcolor(WHITE);
                settextstyle(10, 0, L"微软雅黑");
                outtextxy(delX + 3, delY + 1, L"×");
            }
        }

        setcliprgn(NULL);
    };

    auto drawRightArea = [&]() {
        settextcolor(RGB(80, 80, 120));
        settextstyle(16, 0, L"微软雅黑");
        setbkmode(TRANSPARENT);
        TCHAR dateStr[50];
        swprintf(dateStr, 50, L"%d年%d月%d日", curDairy.year, curDairy.month, curDairy.day);
        outtextxy(800 - textwidth(dateStr) - 20, 25, dateStr);

        drawCard(RX + 10, 55, 800 - RX - 20, 105);

        settextstyle(14, 0, L"微软雅黑");
        settextcolor(RGB(100, 120, 160));
        outtextxy(RX + 30, 63, L"属性变化");

        drawCard(diaryAreaX, diaryAreaY, diaryAreaWidth, diaryAreaHeight);
    };

    auto drawAddModal = [&]() {
        setfillcolor(RGB(30, 30, 50));
        solidrectangle(0, 0, 800, 600);

        int modalX = 190, modalY = 80, modalW = 420, modalH = 440;
        drawCard(modalX, modalY, modalW, modalH);

        settextstyle(22, 0, L"微软雅黑");
        setbkmode(TRANSPARENT);
        settextcolor(RGB(50, 60, 100));
        std::wstring modalTitle = L"添加常用事件";
        outtextxy(modalX + (modalW - textwidth(modalTitle.c_str())) / 2, modalY + 20, modalTitle.c_str());

        settextcolor(RGB(80, 100, 140));
        settextstyle(14, 0, L"微软雅黑");
        outtextxy(modalX + 20, modalY + 68, L"事件名称");

        int nameInputX = modalX + 100;
        int nameInputY = modalY + 60;
        int nameInputW = modalW - 120;
        int nameInputH = 36;

        if (nameFieldActive) {
            setfillcolor(RGB(235, 240, 255));
            setlinecolor(RGB(102, 126, 234));
        }
        else {
            setfillcolor(RGB(245, 248, 255));
            setlinecolor(RGB(190, 210, 240));
        }
        solidroundrect(nameInputX, nameInputY, nameInputX + nameInputW, nameInputY + nameInputH, 6, 6);
        roundrect(nameInputX, nameInputY, nameInputX + nameInputW, nameInputY + nameInputH, 6, 6);

        settextcolor(RGB(50, 60, 90));
        settextstyle(15, 0, L"微软雅黑");
        std::wstring nameDisplay = newEventName;
        if (nameFieldActive) nameDisplay += L"|";
        outtextxy(nameInputX + 10, nameInputY + 8, nameDisplay.c_str());

        const wchar_t* propNames[] = { L"心情", L"健康", L"编程", L"游戏", L"魅力" };
        int increments[] = { 2, 2, 5, 5, 1 };
        int rowStartY = modalY + 115;
        int rowH = 50;

        for (int i = 0; i < 5; i++) {
            int rowY = rowStartY + i * rowH;

            settextcolor(RGB(80, 100, 140));
            settextstyle(15, 0, L"微软雅黑");
            outtextxy(modalX + 30, rowY + 8, propNames[i]);

            int minusX = modalX + 240;
            setfillcolor(RGB(200, 215, 240));
            solidroundrect(minusX, rowY + 2, minusX + 35, rowY + 32, 5, 5);
            settextcolor(RGB(60, 80, 130));
            settextstyle(16, 0, L"微软雅黑");
            outtextxy(minusX + 11, rowY + 6, L"-");

            settextcolor(RGB(50, 60, 90));
            settextstyle(16, 0, L"微软雅黑");
            std::wstring valStr = (newEventDeltas[i] > 0 ? L"+" : L"") + std::to_wstring(newEventDeltas[i]);
            outtextxy(modalX + 295, rowY + 8, valStr.c_str());

            int plusX = modalX + 340;
            setfillcolor(RGB(200, 215, 240));
            solidroundrect(plusX, rowY + 2, plusX + 35, rowY + 32, 5, 5);
            settextcolor(RGB(60, 80, 130));
            settextstyle(16, 0, L"微软雅黑");
            outtextxy(plusX + 11, rowY + 6, L"+");
        }

        Button btnConfirm(modalX + 80, modalY + modalH - 55, 100, 38, "确认", RGB(102, 156, 234), RGB(80, 120, 200));
        Button btnCancel(modalX + modalW - 180, modalY + modalH - 55, 100, 38, "取消", RGB(160, 170, 190), RGB(130, 140, 160));
        btnConfirm.draw();
        btnCancel.draw();
    };

    auto fullRedraw = [&]() {
        drawGradientBg(RGB(232, 244, 253), RGB(245, 237, 255));
        drawLeftPanel();
        drawRightArea();
    };

    fullRedraw();

    Button btnMenu(RX + 10, 20, 80, 35, "菜单", RGB(160, 170, 190), RGB(130, 140, 160), WHITE);
    Button btnSave(800 - 90, 555, 80, 35, "保存", RGB(80, 190, 140), RGB(60, 160, 120), WHITE);

    COLORREF plusColors[] = {
        RGB(102, 156, 234), RGB(80, 190, 170), RGB(100, 180, 230), RGB(150, 140, 220), RGB(255, 183, 139)
    };
    COLORREF minusColors[] = {
        RGB(80, 120, 200), RGB(60, 150, 140), RGB(75, 140, 200), RGB(120, 110, 190), RGB(200, 145, 95)
    };

    Button btnMoodPlus(propStartX, propStartY-5, propBtnWidth, propBtnHeight, "+心情", plusColors[0], minusColors[0]);
    Button btnMoodMinus(propStartX + propBtnWidth + 8, propStartY - 5, propBtnWidth, propBtnHeight, "-心情", minusColors[0], RGB(60, 90, 170));
    Button btnHealthPlus(propStartX + 2 * (propBtnWidth + 8) + propGap, propStartY - 5, propBtnWidth, propBtnHeight, "+健康", plusColors[1], minusColors[1]);
    Button btnHealthMinus(propStartX + 3 * (propBtnWidth + 8) + propGap, propStartY - 5, propBtnWidth, propBtnHeight, "-健康", minusColors[1], RGB(50, 120, 110));
    Button btnComputerPlus(propStartX + 4 * (propBtnWidth + 8) + 2 * propGap, propStartY - 5, propBtnWidth, propBtnHeight, "+编程", plusColors[2], minusColors[2]);
    Button btnComputerMinus(propStartX + 5 * (propBtnWidth + 8) + 2 * propGap, propStartY - 5, propBtnWidth, propBtnHeight, "-编程", minusColors[2], RGB(55, 110, 170));

    Button btnGamePlus(propStartX, propStartY + propBtnHeight + 15 - 10, propBtnWidth, propBtnHeight, "+游戏", plusColors[3], minusColors[3]);
    Button btnGameMinus(propStartX + propBtnWidth + 8, propStartY + propBtnHeight + 15 - 10, propBtnWidth, propBtnHeight, "-游戏", minusColors[3], RGB(90, 80, 155));
    Button btnCharmPlus(propStartX + 2 * (propBtnWidth + 8) + propGap, propStartY + propBtnHeight + 15 - 10, propBtnWidth, propBtnHeight, "+魅力", plusColors[4], minusColors[4]);
    Button btnCharmMinus(propStartX + 3 * (propBtnWidth + 8) + propGap, propStartY + propBtnHeight + 15 - 10, propBtnWidth, propBtnHeight, "-魅力", minusColors[4], RGB(170, 125, 70));

    Button btnAddEvent(10, 520, LP_W - 20, 30, "添加事件", RGB(102, 156, 234), RGB(80, 120, 200));
    Button btnManage(10, 555, LP_W - 20, 30, "管理", RGB(160, 170, 190), RGB(130, 140, 160));

    auto drawAllButtons = [&]() {
        btnMenu.draw(); btnSave.draw();
        btnMoodPlus.draw(); btnMoodMinus.draw();
        btnHealthPlus.draw(); btnHealthMinus.draw();
        btnComputerPlus.draw(); btnComputerMinus.draw();
        btnGamePlus.draw(); btnGameMinus.draw();
        btnCharmPlus.draw(); btnCharmMinus.draw();
        btnAddEvent.draw(); btnManage.draw();
    };

    drawAllButtons();

    if (!diaryContent.empty()) {
        redrawDiaryContent(diaryAreaX, diaryAreaY, diaryAreaWidth, diaryAreaHeight, diaryContent, maxCharsPerLine, lineHeight);
    }

    ExMessage msg;
    while (true) {
        if (peekmessage(&msg, EX_MOUSE | EX_CHAR)) {
            if (showAddModal) {
                if (msg.message == WM_LBUTTONDOWN) {
                    int modalX = 190, modalY = 80, modalW = 420, modalH = 440;

                    int nameInputX = modalX + 100;
                    int nameInputY = modalY + 60;
                    int nameInputW = modalW - 120;
                    int nameInputH = 36;
                    if (msg.x >= nameInputX && msg.x <= nameInputX + nameInputW &&
                        msg.y >= nameInputY && msg.y <= nameInputY + nameInputH) {
                        nameFieldActive = true;
                    }
                    else {
                        nameFieldActive = false;
                    }

                    int increments[] = { 2, 2, 5, 5, 1 };
                    int rowStartY = modalY + 115;
                    int rowH = 50;
                    for (int i = 0; i < 5; i++) {
                        int rowY = rowStartY + i * rowH;
                        int minusX = modalX + 240;
                        if (msg.x >= minusX && msg.x <= minusX + 35 &&
                            msg.y >= rowY + 2 && msg.y <= rowY + 32) {
                            newEventDeltas[i] -= increments[i];
                            drawAddModal();
                            break;
                        }
                        int plusX = modalX + 340;
                        if (msg.x >= plusX && msg.x <= plusX + 35 &&
                            msg.y >= rowY + 2 && msg.y <= rowY + 32) {
                            newEventDeltas[i] += increments[i];
                            drawAddModal();
                            break;
                        }
                    }

                    if (msg.x >= modalX + 80 && msg.x <= modalX + 180 &&
                        msg.y >= modalY + modalH - 55 && msg.y <= modalY + modalH - 17) {
                        if (!newEventName.empty()) {
                            CustomEvent ev;
                            ev.name = newEventName;
                            ev.moodDelta = newEventDeltas[0];
                            ev.healthDelta = newEventDeltas[1];
                            ev.computerDelta = newEventDeltas[2];
                            ev.gameDelta = newEventDeltas[3];
                            ev.charmDelta = newEventDeltas[4];
                            customEvents.push_back(ev);
                            saveCustomEvents();
                        }
                        showAddModal = false;
                        nameFieldActive = false;
                        fullRedraw();
                        drawAllButtons();
                        redrawDiaryContent(diaryAreaX, diaryAreaY, diaryAreaWidth, diaryAreaHeight, diaryContent, maxCharsPerLine, lineHeight);
                    }

                    if (msg.x >= modalX + modalW - 180 && msg.x <= modalX + modalW - 80 &&
                        msg.y >= modalY + modalH - 55 && msg.y <= modalY + modalH - 17) {
                        showAddModal = false;
                        nameFieldActive = false;
                        fullRedraw();
                        drawAllButtons();
                        redrawDiaryContent(diaryAreaX, diaryAreaY, diaryAreaWidth, diaryAreaHeight, diaryContent, maxCharsPerLine, lineHeight);
                    }
                }

                if (msg.message == WM_CHAR && nameFieldActive) {
                    if (msg.vkcode == '\b') {
                        if (!newEventName.empty()) newEventName.pop_back();
                    }
                    else if (msg.ch != 0 && msg.vkcode != '\r') {
                        newEventName += msg.ch;
                    }
                    drawAddModal();
                }
            }
            else {
                if (msg.message == WM_MOUSEMOVE) {
                    btnMenu.updateHover(msg.x, msg.y);
                    btnSave.updateHover(msg.x, msg.y);
                    btnMoodPlus.updateHover(msg.x, msg.y);
                    btnMoodMinus.updateHover(msg.x, msg.y);
                    btnHealthPlus.updateHover(msg.x, msg.y);
                    btnHealthMinus.updateHover(msg.x, msg.y);
                    btnComputerPlus.updateHover(msg.x, msg.y);
                    btnComputerMinus.updateHover(msg.x, msg.y);
                    btnGamePlus.updateHover(msg.x, msg.y);
                    btnGameMinus.updateHover(msg.x, msg.y);
                    btnCharmPlus.updateHover(msg.x, msg.y);
                    btnCharmMinus.updateHover(msg.x, msg.y);
                    btnAddEvent.updateHover(msg.x, msg.y);
                    btnManage.updateHover(msg.x, msg.y);

                    int oldHovered = hoveredEventIdx;
                    hoveredEventIdx = -1;
                    int itemX = 10, itemW = LP_W - 20, itemH = 42, itemGap = 5;
                    int listTop = 50, listBottom = 510;
                    for (size_t i = 0; i < customEvents.size(); i++) {
                        int itemY = listTop + (int)i * (itemH + itemGap) - eventScrollOffset;
                        if (msg.x >= itemX && msg.x <= itemX + itemW &&
                            msg.y >= max(itemY, listTop) && msg.y <= min(itemY + itemH, listBottom)) {
                            hoveredEventIdx = (int)i;
                            break;
                        }
                    }
                    if (hoveredEventIdx != oldHovered) {
                        drawLeftPanel();
                        btnAddEvent.draw();
                        btnManage.draw();
                    }
                }

                if (msg.message == WM_LBUTTONDOWN || msg.message == WM_LBUTTONUP) {
                    if (btnMenu.checkClick(msg)) {
                        if (msg.message == WM_LBUTTONUP) { curPage = Page_Menu; return; }
                    }
                    if (btnSave.checkClick(msg)) {
                        if (msg.message == WM_LBUTTONUP) { saveDairy(diaryContent); curPage = Page_Final_Dairy; return; }
                    }
                    if (btnMoodPlus.checkClick(msg)) {
                        if (msg.message == WM_LBUTTONUP) { diaryContent += L"   心情+2\n"; myProperty.mood += 2; redrawDiaryContent(diaryAreaX, diaryAreaY, diaryAreaWidth, diaryAreaHeight, diaryContent, maxCharsPerLine, lineHeight); }
                    }
                    if (btnMoodMinus.checkClick(msg)) {
                        if (msg.message == WM_LBUTTONUP) { diaryContent += L"    心情-2\n"; myProperty.mood -= 2; redrawDiaryContent(diaryAreaX, diaryAreaY, diaryAreaWidth, diaryAreaHeight, diaryContent, maxCharsPerLine, lineHeight); }
                    }
                    if (btnHealthPlus.checkClick(msg)) {
                        if (msg.message == WM_LBUTTONUP) { diaryContent += L"   健康+2\n"; myProperty.health += 2; redrawDiaryContent(diaryAreaX, diaryAreaY, diaryAreaWidth, diaryAreaHeight, diaryContent, maxCharsPerLine, lineHeight); }
                    }
                    if (btnHealthMinus.checkClick(msg)) {
                        if (msg.message == WM_LBUTTONUP) { diaryContent += L"    健康-2\n"; myProperty.health -= 2; redrawDiaryContent(diaryAreaX, diaryAreaY, diaryAreaWidth, diaryAreaHeight, diaryContent, maxCharsPerLine, lineHeight); }
                    }
                    if (btnComputerPlus.checkClick(msg)) {
                        if (msg.message == WM_LBUTTONUP) { diaryContent += L"   计算机+5\n"; myProperty.computer += 5; checkLevelUp(L"编程", myProperty.computer, myProperty.computerLevel); redrawDiaryContent(diaryAreaX, diaryAreaY, diaryAreaWidth, diaryAreaHeight, diaryContent, maxCharsPerLine, lineHeight); }
                    }
                    if (btnComputerMinus.checkClick(msg)) {
                        if (msg.message == WM_LBUTTONUP) { diaryContent += L"   计算机-5\n"; myProperty.computer -= 5; checkLevelUp(L"编程", myProperty.computer, myProperty.computerLevel); redrawDiaryContent(diaryAreaX, diaryAreaY, diaryAreaWidth, diaryAreaHeight, diaryContent, maxCharsPerLine, lineHeight); }
                    }
                    if (btnGamePlus.checkClick(msg)) {
                        if (msg.message == WM_LBUTTONUP) { diaryContent += L"   游戏+5\n"; myProperty.game += 5; checkLevelUp(L"游戏", myProperty.game, myProperty.gameLevel); redrawDiaryContent(diaryAreaX, diaryAreaY, diaryAreaWidth, diaryAreaHeight, diaryContent, maxCharsPerLine, lineHeight); }
                    }
                    if (btnGameMinus.checkClick(msg)) {
                        if (msg.message == WM_LBUTTONUP) { diaryContent += L"    游戏-5\n"; myProperty.game -= 5; checkLevelUp(L"游戏", myProperty.game, myProperty.gameLevel); redrawDiaryContent(diaryAreaX, diaryAreaY, diaryAreaWidth, diaryAreaHeight, diaryContent, maxCharsPerLine, lineHeight); }
                    }
                    if (btnCharmPlus.checkClick(msg)) {
                        if (msg.message == WM_LBUTTONUP) { diaryContent += L"   魅力+1\n"; myProperty.charm += 1; checkLevelUp(L"魅力", myProperty.charm, myProperty.charmLevel); redrawDiaryContent(diaryAreaX, diaryAreaY, diaryAreaWidth, diaryAreaHeight, diaryContent, maxCharsPerLine, lineHeight); }
                    }
                    if (btnCharmMinus.checkClick(msg)) {
                        if (msg.message == WM_LBUTTONUP) { diaryContent += L"    魅力-1\n"; myProperty.charm -= 1; checkLevelUp(L"魅力", myProperty.charm, myProperty.charmLevel); redrawDiaryContent(diaryAreaX, diaryAreaY, diaryAreaWidth, diaryAreaHeight, diaryContent, maxCharsPerLine, lineHeight); }
                    }

                    if (btnAddEvent.checkClick(msg)) {
                        if (msg.message == WM_LBUTTONUP) {
                            showAddModal = true;
                            newEventName = L"";
                            memset(newEventDeltas, 0, sizeof(newEventDeltas));
                            nameFieldActive = true;
                            drawAddModal();
                        }
                    }

                    if (btnManage.checkClick(msg)) {
                        if (msg.message == WM_LBUTTONUP) {
                            manageMode = !manageMode;
                            btnManage = Button(10, 555, LP_W - 20, 30,
                                manageMode ? "完成" : "管理",
                                manageMode ? RGB(220, 100, 100) : RGB(160, 170, 190),
                                manageMode ? RGB(180, 70, 70) : RGB(130, 140, 160));
                            drawLeftPanel();
                            btnAddEvent.draw();
                            btnManage.draw();
                        }
                    }

                    if (msg.message == WM_LBUTTONUP && msg.x < LP_W && msg.y >= 50 && msg.y <= 510) {
                        int itemX = 10, itemW = LP_W - 20, itemH = 42, itemGap = 5;
                        int listTop = 50, listBottom = 510;
                        for (size_t i = 0; i < customEvents.size(); i++) {
                            int itemY = listTop + (int)i * (itemH + itemGap) - eventScrollOffset;
                            if (msg.y >= max(itemY, listTop) && msg.y <= min(itemY + itemH, listBottom)) {
                                if (manageMode) {
                                    int delX = itemX + itemW - 18;
                                    int delY = itemY + 3;
                                    if (msg.x >= delX && msg.x <= delX + 15 && msg.y >= delY && msg.y <= delY + 15) {
                                        if (MessageBox(GetHWnd(), L"确认删除该事件？", L"提示", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                                            customEvents.erase(customEvents.begin() + i);
                                            saveCustomEvents();
                                            fullRedraw();
                                            drawAllButtons();
                                            redrawDiaryContent(diaryAreaX, diaryAreaY, diaryAreaWidth, diaryAreaHeight, diaryContent, maxCharsPerLine, lineHeight);
                                        }
                                        break;
                                    }
                                }
                                else {
                                    applyEvent(customEvents[i]);
                                    redrawDiaryContent(diaryAreaX, diaryAreaY, diaryAreaWidth, diaryAreaHeight, diaryContent, maxCharsPerLine, lineHeight);
                                    break;
                                }
                            }
                        }
                    }
                }

                if (msg.message == WM_CHAR) {
                    if (msg.vkcode == '\b') {
                        if (!diaryContent.empty()) diaryContent.pop_back();
                    }
                    else if (msg.vkcode == '\r') {
                        diaryContent += L'\n';
                    }
                    else if (msg.ch != 0) {
                        diaryContent += msg.ch;
                    }
                    redrawDiaryContent(diaryAreaX, diaryAreaY, diaryAreaWidth, diaryAreaHeight, diaryContent, maxCharsPerLine, lineHeight);
                }

                if (msg.message == WM_MOUSEWHEEL && msg.x < LP_W) {
                    int maxVisible = 10;
                    int maxScroll = max(0, (int)((customEvents.size() - maxVisible) * 47));
                    eventScrollOffset -= (msg.wheel > 0 ? 47 : -47);
                    eventScrollOffset = max(0, min(eventScrollOffset, maxScroll));
                    drawLeftPanel();
                    btnAddEvent.draw();
                    btnManage.draw();
                }
            }
        }
        Sleep(10);
    }
}

void personMenu() {
    drawGradientBg(RGB(232, 244, 253), RGB(245, 237, 255));

    Button btnBack(20, 20, 80, 35, "菜单", RGB(160, 170, 190), RGB(130, 140, 160), WHITE);
    btnBack.draw();

    settextstyle(28, 0, L"微软雅黑");
    setbkmode(TRANSPARENT);
    std::wstring title = L"个人属性";
    drawShadowText((getwidth() - textwidth(title.c_str())) / 2, 50, title.c_str());

    drawCard(60, 120, getwidth() - 120, 280);

    struct PropInfo { const wchar_t* name; int value; int level; COLORREF color; };
    PropInfo props[] = {
        {L"心情", myProperty.mood, 0, RGB(102, 156, 234)},
        {L"健康", myProperty.health, 0, RGB(80, 190, 170)},
        {L"编程", myProperty.computer, myProperty.computerLevel, RGB(100, 180, 230)},
        {L"游戏", myProperty.game, myProperty.gameLevel, RGB(150, 140, 220)},
        {L"魅力", myProperty.charm, myProperty.charmLevel, RGB(255, 183, 139)}
    };

    int barX = 160;
    int barWidth = 300;
    int barHeight = 22;
    int startY = 155;
    int gap = 48;

    for (int i = 0; i < 5; i++) {
        int cy = startY + i * gap;

        settextcolor(RGB(80, 100, 140));
        settextstyle(16, 0, L"微软雅黑");
        outtextxy(80, cy + 2, props[i].name);

        if (props[i].level > 0) {
            std::wstring lvStr = L"Lv." + std::to_wstring(props[i].level);
            int nameW = textwidth(props[i].name);
            setfillcolor(props[i].color);
            int lvTagX = 80 + nameW + 8;
            int lvTagW = textwidth(lvStr.c_str()) + 12;
            solidroundrect(lvTagX, cy + 2, lvTagX + lvTagW, cy + 22, 4, 4);
            settextcolor(WHITE);
            settextstyle(12, 0, L"微软雅黑");
            outtextxy(lvTagX + 6, cy + 5, lvStr.c_str());
        }

        setfillcolor(RGB(220, 230, 248));
        solidroundrect(barX, cy, barX + barWidth, cy + barHeight, 6, 6);

        int fillW = barWidth * min(max(props[i].value, 0), 100) / 100;
        if (fillW > 0) {
            IMAGE barImg;
            barImg.Resize(fillW, barHeight);
            DWORD* bbuf = GetImageBuffer(&barImg);
            COLORREF c = props[i].color;
            int cr = GetRValue(c), cg = GetGValue(c), cb = GetBValue(c);
            for (int by = 0; by < barHeight; by++) {
                int br = cr + (min(cr + 40, 255) - cr) * by / barHeight;
                int bg = cg + (min(cg + 40, 255) - cg) * by / barHeight;
                int bb = cb + (min(cb + 40, 255) - cb) * by / barHeight;
                for (int bx = 0; bx < fillW; bx++) {
                    bbuf[by * fillW + bx] = RGB(br, bg, bb);
                }
            }
            putimage(barX, cy, &barImg);
            setlinecolor(props[i].color);
            roundrect(barX, cy, barX + fillW, cy + barHeight, 6, 6);
        }

        settextcolor(RGB(60, 80, 120));
        settextstyle(14, 0, L"微软雅黑");
        std::wstring valStr = std::to_wstring(props[i].value) + L"/100";
        outtextxy(barX + barWidth + 15, cy + 3, valStr.c_str());
    }

    settextcolor(RGB(80, 100, 140));
    settextstyle(16, 0, L"微软雅黑");
    outtextxy(80, startY + 5 * gap+10, L"签名");

    int signX = 80;
    int signY = startY + 5 * gap + 30;
    int signWidth = getwidth() - 160;
    int signHeight = 50;
    drawCard(signX, signY, signWidth, signHeight, 8);
    settextcolor(RGB(80, 100, 140));
    settextstyle(15, 0, L"微软雅黑");
    std::wstring signContent = stringToWstring(myProperty.introduction);
    outtextxy(signX + 15, signY + 15, signContent.c_str());

    ExMessage msg;
    while (true) {
        if (peekmessage(&msg, EX_MOUSE)) {
            if (msg.message == WM_MOUSEMOVE) {
                btnBack.updateHover(msg.x, msg.y);
            }
            if (btnBack.checkClick(msg)) {
                if (msg.message == WM_LBUTTONUP) { curPage = Page_Menu; return; }
            }
        }
        Sleep(10);
    }
}

void searchMenu() {
    std::wstring yearText, monthText, dayText;
    int activeField = 0;

    Button btnSearch(250, 410, 130, 42, "查询", RGB(80, 190, 140), RGB(60, 160, 120));
    Button btnBack(420, 410, 130, 42, "返回", RGB(160, 170, 190), RGB(130, 140, 160));

    auto redrawSearchMenu = [&]() {
        drawGradientBg(RGB(232, 244, 253), RGB(245, 237, 255));
        drawCard(100, 50, 600, 440);

        settextstyle(26, 0, L"微软雅黑");
        setbkmode(TRANSPARENT);
        std::wstring title = L"日记查询";
        drawShadowText((getwidth() - textwidth(title.c_str())) / 2, 80, title.c_str());

        settextcolor(RGB(120, 120, 150));
        settextstyle(13, 0, L"微软雅黑");
        outtextxy((getwidth() - textwidth(L"点击输入框后键入数字，Tab 切换字段")) / 2, 125, L"点击输入框后键入数字，Tab 切换字段");

        struct Field { const wchar_t* label; std::wstring* text; int y; };
        Field fields[] = {
            { L"年  份", &yearText, 175 },
            { L"月  份", &monthText, 255 },
            { L"日  期", &dayText, 335 }
        };

        for (int i = 0; i < 3; i++) {
            int labelX = 160;
            int inputX = 280;
            int inputW = 280;
            int inputH = 42;
            int cy = fields[i].y;

            settextcolor(RGB(80, 80, 100));
            settextstyle(18, 0, L"微软雅黑");
            outtextxy(labelX, cy + 10, fields[i].label);

            if (i == activeField) {
                setfillcolor(RGB(235, 240, 255));
                setlinecolor(RGB(102, 126, 234));
            } else {
                setfillcolor(WHITE);
                setlinecolor(RGB(200, 200, 220));
            }
            solidroundrect(inputX, cy, inputX + inputW, cy + inputH, 8, 8);
            roundrect(inputX, cy, inputX + inputW, cy + inputH, 8, 8);

            settextcolor(RGB(50, 50, 60));
            settextstyle(18, 0, L"微软雅黑");
            std::wstring display = *fields[i].text;
            if (i == activeField) display += L"|";
            outtextxy(inputX + 15, cy + 10, display.c_str());
        }

        btnSearch.draw();
        btnBack.draw();
    };

    redrawSearchMenu();

    ExMessage msg;
    while (true) {
        if (peekmessage(&msg, EX_MOUSE | EX_CHAR)) {
            if (msg.message == WM_MOUSEMOVE) {
                btnSearch.updateHover(msg.x, msg.y);
                btnBack.updateHover(msg.x, msg.y);
            }
            if (msg.message == WM_LBUTTONDOWN) {
                int inputX = 280, inputW = 280, inputH = 42;
                int fieldYs[] = { 175, 255, 335 };
                bool hit = false;
                for (int i = 0; i < 3; i++) {
                    if (msg.x >= inputX && msg.x <= inputX + inputW &&
                        msg.y >= fieldYs[i] && msg.y <= fieldYs[i] + inputH) {
                        activeField = i;
                        hit = true;
                        redrawSearchMenu();
                        break;
                    }
                }
                if (!hit) {
                    if (btnSearch.checkClick(msg)) {
                        int year = _wtoi(yearText.c_str());
                        int month = _wtoi(monthText.c_str());
                        int day = _wtoi(dayText.c_str());
                        curDairy.year = year;
                        curDairy.month = month;
                        curDairy.day = day;
                        curDairy.path = "";
                        if (haveThisDairy()) {
                            curPage = Page_Final_Dairy;
                            return;
                        } else {
                            MessageBox(GetHWnd(), L"该日期没有日记！", L"提示", MB_OK | MB_ICONINFORMATION);
                        }
                    }
                    if (btnBack.checkClick(msg)) {
                        curPage = Page_Menu;
                        return;
                    }
                }
            }

            if (msg.message == WM_CHAR) {
                std::wstring* current = nullptr;
                if (activeField == 0) current = &yearText;
                else if (activeField == 1) current = &monthText;
                else if (activeField == 2) current = &dayText;

                if (current) {
                    if (msg.vkcode == '\b') {
                        if (!current->empty()) current->pop_back();
                    } else if (msg.vkcode == '\t') {
                        activeField = (activeField + 1) % 3;
                    } else if (msg.vkcode == '\r') {
                        int year = _wtoi(yearText.c_str());
                        int month = _wtoi(monthText.c_str());
                        int day = _wtoi(dayText.c_str());
                        curDairy.year = year;
                        curDairy.month = month;
                        curDairy.day = day;
                        curDairy.path = "";
                        if (haveThisDairy()) {
                            curPage = Page_Final_Dairy;
                            return;
                        } else {
                            MessageBox(GetHWnd(), L"该日期没有日记！", L"提示", MB_OK | MB_ICONINFORMATION);
                        }
                    } else if (msg.ch >= L'0' && msg.ch <= L'9') {
                        int maxLen = (activeField == 0) ? 4 : 2;
                        if ((int)current->length() < maxLen) {
                            *current += msg.ch;
                        }
                    }
                }
                redrawSearchMenu();
            }
        }
        Sleep(10);
    }
}

void finalMenu() {
    drawGradientBg(RGB(232, 244, 253), RGB(245, 237, 255));

    settextstyle(24, 0, L"微软雅黑");
    setbkmode(TRANSPARENT);
    TCHAR dateStr[50];
    swprintf(dateStr, 50, L"%d年%d月%d日", curDairy.year, curDairy.month, curDairy.day);
    drawShadowText((getwidth() - textwidth(dateStr)) / 2, 30, dateStr);

    std::wstring diaryContent = L"";
    if (!curDairy.path.empty()) {
        std::ifstream file(curDairy.path, std::ios::binary);
        if (file.is_open()) {
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
                diaryContent = utf8_to_wstring(rawContent);
            } else {
                int acpSize = MultiByteToWideChar(CP_ACP, 0,
                    rawContent.c_str(), (int)rawContent.size(), nullptr, 0);
                if (acpSize > 0) {
                    diaryContent.resize(acpSize);
                    MultiByteToWideChar(CP_ACP, 0,
                        rawContent.c_str(), (int)rawContent.size(), &diaryContent[0], acpSize);
                }
            }
        }
    }

    struct PropChange { std::wstring name; int delta; };
    PropChange propChanges[5] = {
        {L"心情", 0}, {L"健康", 0}, {L"编程", 0}, {L"游戏", 0}, {L"魅力", 0}
    };
    const wchar_t* propNames[] = { L"心情", L"健康", L"计算机", L"编程", L"游戏", L"魅力" };
    int propMap[] = { 0, 1, 2, 2, 3, 4 };

    for (size_t pos = 0; pos < diaryContent.size(); pos++) {
        for (int p = 0; p < 6; p++) {
            const wchar_t* pname = propNames[p];
            size_t pnameLen = wcslen(pname);
            if (diaryContent.compare(pos, pnameLen, pname) == 0) {
                size_t numStart = pos + pnameLen;
                if (numStart < diaryContent.size()) {
                    wchar_t sign = diaryContent[numStart];
                    if (sign == L'+' || sign == L'-') {
                        int num = 0;
                        size_t digitPos = numStart + 1;
                        while (digitPos < diaryContent.size() && diaryContent[digitPos] >= L'0' && diaryContent[digitPos] <= L'9') {
                            num = num * 10 + (diaryContent[digitPos] - L'0');
                            digitPos++;
                        }
                        if (num > 0) {
                            if (sign == L'-') num = -num;
                            propChanges[propMap[p]].delta += num;
                        }
                    }
                }
                break;
            }
        }
    }

    int propAreaY = 70;
    int propAreaH = 55;
    drawCard(40, propAreaY, getwidth() - 80, propAreaH);

    settextstyle(12, 0, L"微软雅黑");
    settextcolor(RGB(100, 120, 160));
    outtextxy(60, propAreaY + 5, L"属性变化");

    COLORREF propColors[] = {
        RGB(102, 156, 234), RGB(80, 190, 170), RGB(100, 180, 230), RGB(150, 140, 220), RGB(255, 183, 139)
    };

    int tagX = 60;
    int tagY = propAreaY + 24;
    settextstyle(14, 0, L"微软雅黑");
    for (int i = 0; i < 5; i++) {
        if (propChanges[i].delta == 0) continue;

        std::wstring tagText = propChanges[i].name;
        if (propChanges[i].delta > 0) {
            tagText += L"+" + std::to_wstring(propChanges[i].delta);
        } else {
            tagText += std::to_wstring(propChanges[i].delta);
        }

        int tagW = textwidth(tagText.c_str()) + 20;
        int tagH = 24;

        COLORREF tagBg = propColors[i];
        if (propChanges[i].delta < 0) {
            int r = GetRValue(propColors[i]) * 7 / 10;
            int g = GetGValue(propColors[i]) * 7 / 10;
            int b = GetBValue(propColors[i]) * 7 / 10;
            tagBg = RGB(r, g, b);
        }

        setfillcolor(tagBg);
        solidroundrect(tagX, tagY, tagX + tagW, tagY + tagH, 6, 6);

        settextcolor(WHITE);
        outtextxy(tagX + 10, tagY + 4, tagText.c_str());

        tagX += tagW + 10;
    }

    int contentX = 40;
    int contentY = propAreaY + propAreaH + 10;
    int contentWidth = getwidth() - 80;
    int contentHeight = getheight() - contentY - 65;

    drawCard(contentX, contentY, contentWidth, contentHeight);

    settextcolor(RGB(50, 60, 90));
    settextstyle(16, 0, L"微软雅黑");
    setbkmode(TRANSPARENT);

    int lineHeight = 25;
    int currentY = contentY + 20;
    std::wstring line;
    for (wchar_t c : diaryContent) {
        if (c == L'\n') {
            outtextxy(contentX + 20, currentY, line.c_str());
            line.clear();
            currentY += lineHeight;
            if (currentY > contentY + contentHeight - 20) break;
        } else {
            line += c;
            if (line.length() >= (size_t)(contentWidth / 12)) {
                outtextxy(contentX + 20, currentY, line.c_str());
                line.clear();
                currentY += lineHeight;
                if (currentY > contentY + contentHeight - 20) break;
            }
        }
    }
    if (!line.empty() && currentY <= contentY + contentHeight - 20) {
        outtextxy(contentX + 20, currentY, line.c_str());
    }

    Button btnExit((getwidth() - 100) / 2, getheight() - 55, 100, 40, "返回", RGB(102, 156, 234), RGB(80, 120, 200), WHITE);
    btnExit.draw();

    ExMessage msg;
    while (true) {
        if (peekmessage(&msg, EX_MOUSE)) {
            if (msg.message == WM_MOUSEMOVE) {
                btnExit.updateHover(msg.x, msg.y);
            }
            if (btnExit.checkClick(msg)) {
                if (msg.message == WM_LBUTTONUP) { curPage = Page_Menu; return; }
            }
        }
        Sleep(10);
    }
}

void menuView() {
    drawGradientBg(RGB(232, 244, 253), RGB(245, 237, 255));

    settextstyle(36, 0, L"隶书");
    setbkmode(TRANSPARENT);
    std::wstring title = L"我独自升级";
    drawShadowText((getwidth() - textwidth(title.c_str())) / 2, 55, title.c_str(),
        RGB(160, 180, 220), RGB(60, 80, 150));

    settextcolor(RGB(120, 140, 180));
    settextstyle(14, 0, L"隶书");
    std::wstring subtitle = L"靡不有初，鲜克有终";
    outtextxy((getwidth() - textwidth(subtitle.c_str())) / 2, 105, subtitle.c_str());

    int buttonWidth = 180;
    int buttonHeight = 48;
    int buttonGap = 25;
    int startY = 160;

    COLORREF btnColors[] = {
        RGB(102, 156, 234), RGB(80, 190, 170), RGB(255, 160, 140), RGB(160, 170, 190)
    };
    COLORREF btnHovers[] = {
        RGB(80, 120, 200), RGB(60, 160, 140), RGB(220, 120, 110), RGB(130, 140, 160)
    };

    Button btnStart((getwidth() - buttonWidth) / 2, startY, buttonWidth, buttonHeight, "开始记录", btnColors[0], btnHovers[0]);
    Button btnSearch((getwidth() - buttonWidth) / 2, startY + buttonHeight + buttonGap, buttonWidth, buttonHeight, "日记查询", btnColors[1], btnHovers[1]);
    Button btnProperty((getwidth() - buttonWidth) / 2, startY + 2 * (buttonHeight + buttonGap), buttonWidth, buttonHeight, "个人属性", btnColors[2], btnHovers[2]);
    Button btnExit((getwidth() - buttonWidth) / 2, startY + 3 * (buttonHeight + buttonGap), buttonWidth, buttonHeight, "退出", btnColors[3], btnHovers[3]);

    btnStart.draw();
    btnSearch.draw();
    btnProperty.draw();
    btnExit.draw();

    ExMessage msg;
    while (true) {
        if (peekmessage(&msg, EX_MOUSE)) {
            if (msg.message == WM_MOUSEMOVE) {
                btnStart.updateHover(msg.x, msg.y);
                btnSearch.updateHover(msg.x, msg.y);
                btnProperty.updateHover(msg.x, msg.y);
                btnExit.updateHover(msg.x, msg.y);
            }
            if (btnStart.checkClick(msg)) {
                if (msg.message == WM_LBUTTONUP) { curPage = Page_Choose_Date; return; }
            }
            if (btnSearch.checkClick(msg)) {
                if (msg.message == WM_LBUTTONUP) { curPage = Page_Search_Dairy; return; }
            }
            if (btnProperty.checkClick(msg)) {
                if (msg.message == WM_LBUTTONUP) { curPage = Page_Property; return; }
            }
            if (btnExit.checkClick(msg)) {
                if (msg.message == WM_LBUTTONUP) { closegraph(); exit(0); }
            }
        }
        Sleep(10);
    }
}
