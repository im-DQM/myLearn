#pragma once
#include<string>
#include <easyx.h>
#include "frame.h"

class Button {
private:
	int x, y, width, height;
	std::string text;
	COLORREF bgColor;
	COLORREF hoverColor;
	COLORREF textColor;
	bool hovered;

public:
	Button();
	Button(int x, int y, int width, int height, std::string text,
		COLORREF bg = RGB(102, 126, 234), COLORREF hover = RGB(86, 100, 180), COLORREF txt = WHITE);

	void draw();
	bool checkClick(const ExMessage msg);
	void updateHover(int mouseX, int mouseY);
};

void drawGradientBg(COLORREF topColor, COLORREF bottomColor);
void drawCard(int x, int y, int w, int h, int radius = 12);
void drawShadowText(int x, int y, const TCHAR* text, COLORREF shadowColor = RGB(200, 200, 220), COLORREF textColor = RGB(44, 62, 80));

void menuView();
void chooseDateMenu();
void dataMenu();
void dairyMenu();
void finalMenu();
void searchMenu();
void personMenu();

void initClickSound();
