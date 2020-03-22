#ifndef __SETTINGS_H_
#define __SETTINGS_H_

#include "defines.h"


#include <string>
#include <list>
using namespace std;

namespace Settings {

struct Window
{
    string name;
    int x,y,w,h;
    bool fullscreen;

    Window() : name(APP_TITLE), x(15), y(15), w(1280), h(720), fullscreen(false) { }

};

struct Application
{
    string name;
    float scale;
    int color;
    list<Window> windows;

    Application() : name(APP_NAME), scale(1.f), color(0){
        windows.push_back(Window());
    }

};

// minimal implementation of settings
// Can be accessed r&w anywhere
extern Application application;

// Save and Load store settings in XML file
void Save();
void Load();
void Check();

}

#endif /* __SETTINGS_H_ */
