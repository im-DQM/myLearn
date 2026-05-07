#pragma once
//项目架构
#include<stdio.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <graphics.h>     
#include <conio.h> 


//---------------------------------------------------------数据设计-----------------------------------------------------
//页面组
enum Page {
	Page_Menu,//菜单界面
	Page_Property, //个人属性界面
	Page_Choose_Date, //选择日期方式界面
	Page_Input_Data, //手动输入日期的界面
	Page_Write_Dairy, //写日记的界面
	Page_Final_Dairy, //结算界面
	Page_Search_Dairy//查询对应日期日记的界面
};
extern Page curPage;

//个人属性
struct Property {
	int mood, health, computer, game, charm;
	int computerLevel, gameLevel, charmLevel;
	std::string introduction;

	Property() {
		mood = 0;
		health = 0;
		computer = 0;
		game = 0;
		charm = 0;
		computerLevel = 1;
		gameLevel = 1;
		charmLevel = 1;
		introduction = "木嘿嘿嘿";
	}

	Property(const Property& other) {
		mood = other.mood;
		health = other.health;
		computer = other.computer;
		game = other.game;
		charm = other.charm;
		computerLevel = other.computerLevel;
		gameLevel = other.gameLevel;
		charmLevel = other.charmLevel;
	}

	Property& operator=(const Property& other) {
		if (this == &other)
			return *this;

		mood = other.mood;
		health = other.health;
		computer = other.computer;
		game = other.game;
		charm = other.charm;
		computerLevel = other.computerLevel;
		gameLevel = other.gameLevel;
		charmLevel = other.charmLevel;
		return *this;
	}
};
enum WhitchProperty {
	MOOD,
	HEALTH,
	COMPUTER,
	GAME,
	CHARM
};

struct CustomEvent {
	std::wstring name;
	int moodDelta;
	int healthDelta;
	int computerDelta;
	int gameDelta;
	int charmDelta;
	CustomEvent() : moodDelta(0), healthDelta(0), computerDelta(0), gameDelta(0), charmDelta(0) {}
};
extern std::vector<CustomEvent> customEvents;

extern bool loadExistingDairy;

extern Property myProperty;


//日记表
struct Dairy {//用结构体和vector
	int year, month, day;//日记日期
	std::string path;//文件路径
};
extern std::vector<Dairy> list;
//创建当前日记对象,后续操作日记的时候直接在这个对象上操作，这个对象是动态的，可以随时更改，充当指针使用
extern Dairy curDairy;


//---------------------------------------------------------数据设计-------------------------------------------------------

