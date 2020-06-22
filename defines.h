#ifndef VMIX_DEFINES_H
#define VMIX_DEFINES_H

#define APP_NAME "vimix"
#define APP_TITLE " -- Video Live Mixer"
#define APP_SETTINGS "vimix.xml"
#define APP_VERSION_MAJOR 0
#define APP_VERSION_MINOR 1
#define XML_VERSION_MAJOR 0
#define XML_VERSION_MINOR 1
#define MAX_RECENT_HISTORY 16

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
#define SCENE_DEPTH 12.f
#define CIRCLE_SQUARE_DIST(x,y) ( (x*x + y*y) / (SCENE_UNIT * SCENE_UNIT * SCENE_UNIT * SCENE_UNIT) )
#define MIN_SCALE 0.01f
#define MAX_SCALE 10.f
#define CLAMP_SCALE(x) SIGN(x) * CLAMP( ABS(x), MIN_SCALE, MAX_SCALE)
#define MIXING_DEFAULT_SCALE 2.4f
#define MIXING_MIN_SCALE 0.8f
#define MIXING_MAX_SCALE 7.0f
#define GEOMETRY_DEFAULT_SCALE 1.2f
#define GEOMETRY_MIN_SCALE 0.2f
#define GEOMETRY_MAX_SCALE 10.0f
#define LAYER_DEFAULT_SCALE 0.8f
#define LAYER_MIN_SCALE 0.4f
#define LAYER_MAX_SCALE 1.7f

#define IMGUI_TITLE_MAINWINDOW ICON_FA_CIRCLE_NOTCH "  vimix"
#define IMGUI_TITLE_MEDIAPLAYER ICON_FA_FILM "  Media Player"
#define IMGUI_TITLE_TOOLBOX ICON_FA_WRENCH "  Tools"
#define IMGUI_TITLE_SHADEREDITOR ICON_FA_CODE "  Shader Editor"
#define IMGUI_TITLE_PREVIEW ICON_FA_LAPTOP "  Preview"
#define IMGUI_TITLE_DELETE ICON_FA_BROOM " Delete?"
#define IMGUI_RIGHT_ALIGN -3.5f * ImGui::GetTextLineHeightWithSpacing()
#define IMGUI_NOTIFICATION_DURATION 1.5f
#ifdef APPLE
#define CTRL_MOD "Cmd+"
#else
#define CTRL_MOD "Ctrl+"
#endif

#define COLOR_BGROUND 0.2f, 0.2f, 0.2f
#define COLOR_NAVIGATOR 0.1f, 0.1f, 0.1f
#define COLOR_DEFAULT_SOURCE 0.8f, 0.8f, 0.8f
#define COLOR_HIGHLIGHT_SOURCE 1.f, 1.f, 1.f
#define COLOR_FRAME 0.8f, 0.f, 0.8f
#define COLOR_LIMBO_CIRCLE 0.16f, 0.16f, 0.16f

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
//#define COLOR_LIMBO_CIRCLE 210, 160, 210
#define COLOR_FADING 25, 25, 25
#define COLOR_FLASHING 250, 250, 250
#define COLOR_FRAME_MOVE 230, 30, 230
#define COLOR_CURSOR 10, 100, 255



#endif // VMIX_DEFINES_H
