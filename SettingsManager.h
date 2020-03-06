#ifndef vlmixer_app_settings
#define vlmixer_app_settings

#include <string>
#include <list>
using namespace std;

class WindowSettings
{
public:
	int x,y,w,h;
	string name;
	bool fullscreen;

	WindowSettings() : x(0), y(0), w(100), h(100), name("Untitled"), fullscreen(false)
	{
	}

	WindowSettings(int x, int y, int w, int h, const string& name)
	{
		this->x=x;
		this->y=y;
		this->w=w;
		this->h=h;
		this->name=name;
		this->fullscreen=false;
	}
};

class AppSettings
{
public:
	string name;
    string filename;
	list<WindowSettings> windows;
	float scale;
	int color;

	AppSettings(const string& name);
};

class Settings
{    
public:
	static AppSettings application;

	static void Save();
	static void Load();
	static void Check();
};

#endif /* vlmixer_app_settings */
