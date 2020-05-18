#ifndef VMIX_DEFINES_H
#define VMIX_DEFINES_H

#define APP_NAME "vimix"
#define APP_TITLE "vimix -- Video Live Mixer"
#define APP_SETTINGS "vimix.xml"
#define APP_VERSION_MAJOR 0
#define APP_VERSION_MINOR 1
#define XML_VERSION_MAJOR 0
#define XML_VERSION_MINOR 1
#define MAX_RECENT_HISTORY 10

#define MINI(a, b)  (((a) < (b)) ? (a) : (b))
#define MAXI(a, b)  (((a) > (b)) ? (a) : (b))
#define ABS(a)	   (((a) < 0) ? -(a) : (a))
#define ABS_DIFF(a, b) ( (a) < (b) ? (b - a) : (a - b) )
#define SIGN(a)	   (((a) < 0) ? -1.0 : 1.0)
#define SQUARE(a)	   ((a) * (a))
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
#define EPSILON 0.00001
#define LOG100(val) (50.0/log(10.0)*log((float)val + 1.0))
#define EXP100(val) (exp(log(10.0)/50.0*(float)(val))-1.0)
#define EUCLIDEAN(P1, P2) sqrt((P1.x() - P2.x()) * (P1.x() - P2.x()) + (P1.y() - P2.y()) * (P1.y() - P2.y()))

#define SCENE_UNIT 5.f
#define SCENE_DEPTH -12.f
#define CIRCLE_SQUARE_DIST(x,y) ( (x*x + y*y) / (SCENE_UNIT * SCENE_UNIT * SCENE_UNIT * SCENE_UNIT) )

#define IMGUI_TITLE_MAINWINDOW ICON_FA_CIRCLE_NOTCH "  vimix"
#define IMGUI_TITLE_MEDIAPLAYER ICON_FA_FILM "  Media Player"
#define IMGUI_TITLE_TOOLBOX ICON_FA_WRENCH "  Tools"
#define IMGUI_TITLE_SHADEREDITOR ICON_FA_CODE "  Shader Editor"
#define IMGUI_TITLE_PREVIEW ICON_FA_LAPTOP "  Preview"
#define IMGUI_TITLE_DELETE ICON_FA_BROOM " Delete?"
#define IMGUI_RIGHT_ALIGN -3.5f * ImGui::GetTextLineHeightWithSpacing()

#define COLOR_BGROUND 0.2f, 0.2f, 0.2f
#define COLOR_NAVIGATOR 0.1f, 0.1f, 0.1f
#define COLOR_DEFAULT_SOURCE 1.f, 1.f, 1.f

// from glmixer
#define TEXTURE_REQUIRED_MAXIMUM 2048
#define CATALOG_TEXTURE_HEIGHT 96
#define SELECTBUFSIZE 512
#define CIRCLE_SIZE 8.0
#define DEFAULT_LIMBO_SIZE 1.5
#define MIN_LIMBO_SIZE 1.1
#define MAX_LIMBO_SIZE 3.0
#define DEFAULT_ICON_SIZE 1.75
#define MIN_ICON_SIZE 1.0
#define MAX_ICON_SIZE 2.5
#define MIN_DEPTH_LAYER 0.0
#define MAX_DEPTH_LAYER 40.0
#define MIN_SCALE 0.1
#define MAX_SCALE 200.0
#define DEPTH_EPSILON 0.1
#define DEPTH_DEFAULT_SPACING 1.0
#define BORDER_SIZE 0.4
#define CENTER_SIZE 1.2
#define PROPERTY_DECIMALS 8
#define COLOR_SOURCE 230, 230, 0
#define COLOR_SOURCE_STATIC 230, 40, 40
#define COLOR_SELECTION 10, 210, 40
#define COLOR_SELECTION_AREA 50, 210, 50
#define COLOR_CIRCLE 210, 30, 210
#define COLOR_CIRCLE_MOVE 230, 30, 230
#define COLOR_DRAWINGS 180, 180, 180
#define COLOR_LIMBO 35, 35, 35
#define COLOR_LIMBO_CIRCLE 210, 160, 210
#define COLOR_FADING 25, 25, 25
#define COLOR_FLASHING 250, 250, 250
#define COLOR_FRAME 210, 30, 210
#define COLOR_FRAME_MOVE 230, 30, 230
#define COLOR_CURSOR 10, 100, 255



#endif // VMIX_DEFINES_H
