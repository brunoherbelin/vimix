#ifndef VMIX_DEFINES_H
#define VMIX_DEFINES_H

#define APP_NAME "vimix"
#define APP_TITLE "Video Live Mixer"
#define APP_SETTINGS "vimix.xml"
#define XML_VERSION_MAJOR 0
#define XML_VERSION_MINOR 3
#define MAX_RECENT_HISTORY 20
#define MAX_SESSION_LEVEL 3

#define VIMIX_FILE_EXT "mix"
#define VIMIX_FILE_PATTERN "*.mix"
#define MEDIA_FILES_PATTERN "*.mix", "*.mp4", "*.mpg", "*.mpeg", "*.m2v", "*.m4v", "*.avi", "*.mov",\
    "*.mkv", "*.webm", "*.mod", "*.wmv", "*.mxf", "*.ogg",\
    "*.flv", "*.hevc", "*.asf", "*.jpg", "*.png",  "*.gif",\
    "*.tif", "*.tiff", "*.webp", "*.bmp", "*.ppm", "*.svg,"
#define IMAGES_FILES_PATTERN "*.jpg", "*.png", "*.bmp", "*.ppm", "*.gif"

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
#define ROUND(val, factor) float( int( val * factor ) ) / factor;

#define SCENE_UNIT 5.f
#define MIN_SCALE 0.01f
#define MAX_SCALE 10.f
#define CLAMP_SCALE(x) SIGN(x) * CLAMP( ABS(x), MIN_SCALE, MAX_SCALE)
#define SCENE_DEPTH 14.f
#define MIN_DEPTH 0.f
#define MAX_DEPTH 12.f
#define DELTA_DEPTH 0.05f
#define DELTA_ALPHA 0.0005f
#define MIXING_DEFAULT_SCALE 2.4f
#define MIXING_MIN_SCALE 0.8f
#define MIXING_MAX_SCALE 7.0f
#define MIXING_MIN_THRESHOLD 1.3f
#define MIXING_MAX_THRESHOLD 1.9f
#define MIXING_ICON_SCALE 0.15f, 0.15f, 1.f
#define GEOMETRY_DEFAULT_SCALE 1.4f
#define GEOMETRY_MIN_SCALE 0.4f
#define GEOMETRY_MAX_SCALE 7.0f
#define LAYER_DEFAULT_SCALE 0.6f
#define LAYER_MIN_SCALE 0.25f
#define LAYER_MAX_SCALE 1.7f
#define LAYER_PERSPECTIVE 2.0f
#define LAYER_BACKGROUND 2.f
#define LAYER_FOREGROUND 10.f
#define LAYER_STEP 0.25f
#define APPEARANCE_DEFAULT_SCALE 2.f
#define APPEARANCE_MIN_SCALE 0.4f
#define APPEARANCE_MAX_SCALE 7.0f
#define BRUSH_MIN_SIZE 0.05f
#define BRUSH_MAX_SIZE 2.f
#define BRUSH_MIN_PRESS 0.005f
#define BRUSH_MAX_PRESS 1.f
#define SHAPE_MIN_BLUR 0.f
#define SHAPE_MAX_BLUR 1.f
#define TRANSITION_DEFAULT_SCALE 5.0f
#define TRANSITION_MIN_DURATION 0.2f
#define TRANSITION_MAX_DURATION 10.f
#define ARROWS_MOVEMENT_FACTOR 0.1f
#define SESSION_THUMBNAIL_HEIGHT 120.f
#define RECORD_MAX_TIMEOUT 1200000

#define IMGUI_TITLE_LOGS ICON_FA_LIST_UL "  Logs"
#define IMGUI_LABEL_RECENT_FILES " Recent files"
#define IMGUI_LABEL_RECENT_RECORDS " Recent recordings"
#define IMGUI_RIGHT_ALIGN -3.5f * ImGui::GetTextLineHeightWithSpacing()
#define IMGUI_SAME_LINE 10
#define IMGUI_TOP_ALIGN 10
#define IMGUI_COLOR_OVERLAY IM_COL32(5, 5, 5, 150)
#define IMGUI_COLOR_LIGHT_OVERLAY IM_COL32(5, 5, 5, 50)
#define IMGUI_COLOR_CAPTURE 1.0, 0.55, 0.05
#define IMGUI_COLOR_RECORD 1.0, 0.05, 0.05
#define IMGUI_COLOR_STREAM 0.05, 0.8, 1.0
#define IMGUI_COLOR_BROADCAST 0.1, 0.9, 0.1
#define IMGUI_NOTIFICATION_DURATION 2.5f
#define IMGUI_TOOLTIP_TIMEOUT 80

#define COLOR_BGROUND 0.2f, 0.2f, 0.2f
#define COLOR_NAVIGATOR 0.1f, 0.1f, 0.1f
#define COLOR_DEFAULT_SOURCE 0.7f, 0.7f, 0.7f
#define COLOR_HIGHLIGHT_SOURCE 1.f, 1.f, 1.f
#define COLOR_TRANSITION_SOURCE 1.f, 0.5f, 1.f
#define COLOR_TRANSITION_LINES 0.9f, 0.9f, 0.9f
#define COLOR_APPEARANCE_SOURCE 0.9f, 0.9f, 0.1f
#define COLOR_APPEARANCE_LIGHT 1.0f, 1.0f, 0.9f
#define COLOR_APPEARANCE_MASK 0.9f, 0.9f, 0.9f
#define COLOR_APPEARANCE_MASK_DISABLE 0.6f, 0.6f, 0.6f
#define COLOR_FRAME 0.75f, 0.2f, 0.75f
#define COLOR_FRAME_LIGHT 0.9f, 0.6f, 0.9f
#define COLOR_CIRCLE 0.75f, 0.2f, 0.75f
#define COLOR_CIRCLE_OVER 0.8f, 0.8f, 0.8f
#define COLOR_MIXING_GROUP 0.f, 0.95f, 0.2f
#define COLOR_LIMBO_CIRCLE 0.173f, 0.173f, 0.173f
#define COLOR_SLIDER_CIRCLE 0.11f, 0.11f, 0.11f
#define COLOR_STASH_CIRCLE 0.06f, 0.06f, 0.06f

#define OSC_PORT_RECV_DEFAULT 7000
#define OSC_PORT_SEND_DEFAULT 7001
#define OSC_CONFIG_FILE "osc.xml"

#endif // VMIX_DEFINES_H
