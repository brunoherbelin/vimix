#ifndef VMIX_DEFINES_H
#define VMIX_DEFINES_H

#define APP_NAME "vimix"
#define APP_TITLE "Video Live Mixer"
#define APP_SETTINGS "vimix.xml"
#define XML_VERSION_MAJOR 0
#define XML_VERSION_MINOR 4
#define MAX_RECENT_HISTORY 20
#define MAX_SESSION_LEVEL 3
#define MAX_OUTPUT_WINDOW 3

#define VIMIX_GL_VERSION "opengl3"
#define VIMIX_GLSL_VERSION "#version 150"

#define VIMIX_FILE_TYPE "vimix session (MIX)"
#define VIMIX_FILE_EXT "mix"
#define VIMIX_FILE_PATTERN \
    { \
        "*.mix" \
    }
#define VIMIX_PLAYLIST_FILE_TYPE "vimix playlist (LIX)"
#define VIMIX_PLAYLIST_FILE_EXT "lix"
#define VIMIX_PLAYLIST_FILE_PATTERN \
    { \
        "*.lix" \
    }
#define MEDIA_FILES_TYPE "Supported formats (videos, images, sessions)"
#define MEDIA_FILES_PATTERN \
    { \
        "*.mix", "*.mp4", "*.mpg", "*.mpeg", "*.m2v", "*.m4v", "*.avi", "*.mov", "*.mkv", \
        "*.webm", "*.mod", "*.wmv", "*.mxf", "*.ogg", "*.flv", "*.hevc", "*.asf", "*.jpg", \
        "*.png", "*.gif", "*.tif", "*.tiff", "*.webp", "*.bmp", "*.ppm", "*.svg," \
    }
#define IMAGES_FILES_TYPE "Image (JPG, PNG, BMP, PPM, GIF)"
#define IMAGES_FILES_PATTERN \
    { \
        "*.jpg", "*.png", "*.bmp", "*.ppm", "*.gif" \
    }
#define SUBTITLE_FILES_TYPE "Subtitle (SRT, SUB)"
#define SUBTITLE_FILES_PATTERN \
    { \
        "*.srt", "*.sub" \
    }
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
#define DISPLAYS_UNIT 0.001f
#define DISPLAYS_DEFAULT_SCALE 1.4f
#define DISPLAYS_MIN_SCALE 0.4f
#define DISPLAYS_MAX_SCALE 7.0f
#define ARROWS_MOVEMENT_FACTOR 0.1f
#define SESSION_THUMBNAIL_HEIGHT 120.f
#define RECORD_MAX_TIMEOUT 1200000

#define IMGUI_TITLE_LOGS ICON_FA_LIST_UL "  Logs"
#define IMGUI_LABEL_RECENT_FILES " Recent files"
#define IMGUI_LABEL_RECENT_RECORDS " Recent recordings"
#define IMGUI_RIGHT_ALIGN -3.8f * ImGui::GetTextLineHeightWithSpacing()
#define IMGUI_SAME_LINE 8
#define IMGUI_TOP_ALIGN 10
#define IMGUI_COLOR_OVERLAY IM_COL32(5, 5, 5, 150)
#define IMGUI_COLOR_LIGHT_OVERLAY IM_COL32(5, 5, 5, 50)
#define IMGUI_COLOR_CAPTURE 1.0, 0.55, 0.05
#define IMGUI_COLOR_RECORD 1.0, 0.05, 0.05
#define IMGUI_COLOR_STREAM 0.1, 0.9, 1.0
#define IMGUI_COLOR_BROADCAST 1.0, 0.9, 0.3
#define IMGUI_NOTIFICATION_DURATION 2.5f
#define IMGUI_TOOLTIP_TIMEOUT 80
#define IMGUI_COLOR_FAILED 1.0, 0.3, 0.2

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
#define COLOR_CIRCLE_OVER 0.75f, 0.5f, 0.75f
#define COLOR_CIRCLE_ARROW 0.78f, 0.8f, 0.8f
#define COLOR_MIXING_GROUP 0.f, 0.95f, 0.2f
#define COLOR_LIMBO_CIRCLE 0.173f, 0.173f, 0.173f
#define COLOR_SLIDER_CIRCLE 0.11f, 0.11f, 0.11f
#define COLOR_STASH_CIRCLE 0.06f, 0.06f, 0.06f
#define COLOR_MONITOR 0.90f, 0.90f, 0.90f
#define COLOR_WINDOW 0.2f, 0.85f, 0.85f
#define COLOR_MENU_HOVERED 0.3f, 0.3f, 0.3f

#define OSC_PORT_RECV_DEFAULT 7000
#define OSC_PORT_SEND_DEFAULT 7001
#define OSC_CONFIG_FILE "osc.xml"

#define IMGUI_TITLE_MEDIAPLAYER ICON_FA_PLAY_CIRCLE "  Player"
#define IMGUI_TITLE_TIMER ICON_FA_CLOCK " Timer"
#define IMGUI_TITLE_INPUT_MAPPING ICON_FA_HAND_PAPER " Input Mapping"
#define IMGUI_TITLE_HELP ICON_FA_LIFE_RING "  Help"
#define IMGUI_TITLE_TOOLBOX ICON_FA_HAMSA "  Guru Toolbox"
#define IMGUI_TITLE_SHADEREDITOR ICON_FA_CODE "  Shader Editor"
#define IMGUI_TITLE_PREVIEW ICON_FA_DESKTOP "  Display"

#ifdef APPLE
#define CTRL_MOD "Cmd+"
#define ALT_MOD "option"
#define ALT_LOCK " OPT LOCK"
#else
#define CTRL_MOD "Ctrl+"
#define ALT_MOD "Alt"
#define ALT_LOCK " ALT LOCK"
#endif

#define MENU_NEW_FILE         ICON_FA_FILE "+ New"
#define SHORTCUT_NEW_FILE     CTRL_MOD "W"
#define MENU_OPEN_FILE        ICON_FA_FILE_UPLOAD "  Open"
#define SHORTCUT_OPEN_FILE    CTRL_MOD "O"
#define MENU_REOPEN_FILE      ICON_FA_FILE_UPLOAD "  Re-open"
#define SHORTCUT_REOPEN_FILE  CTRL_MOD "Shift+O"
#define MENU_SAVE_FILE        ICON_FA_FILE_DOWNLOAD "  Save"
#define SHORTCUT_SAVE_FILE    CTRL_MOD "S"
#define MENU_SAVEAS_FILE      ICON_FA_FILE_DOWNLOAD "  Save as"
#define MENU_SAVE_ON_EXIT     ICON_FA_LEVEL_DOWN_ALT "  Save on exit"
#define MENU_OPEN_ON_START    ICON_FA_LEVEL_UP_ALT "  Restore on start"
#define SHORTCUT_SAVEAS_FILE  CTRL_MOD "Shift+S"
#define MENU_QUIT             ICON_FA_POWER_OFF "  Quit"
#define SHORTCUT_QUIT         CTRL_MOD "Q"
#define MENU_CUT              ICON_FA_CUT "  Cut"
#define SHORTCUT_CUT          CTRL_MOD "X"
#define MENU_COPY             ICON_FA_COPY "  Copy"
#define SHORTCUT_COPY         CTRL_MOD "C"
#define MENU_DELETE           ICON_FA_ERASER " Delete"
#define SHORTCUT_DELETE       "Del"
#define ACTION_DELETE         ICON_FA_BACKSPACE " Delete"
#define MENU_PASTE            ICON_FA_PASTE "  Paste"
#define SHORTCUT_PASTE        CTRL_MOD "V"
#define MENU_SELECTALL        ICON_FA_TH_LIST "  Select all"
#define SHORTCUT_SELECTALL    CTRL_MOD "A"
#define MENU_UNDO             ICON_FA_UNDO "  Undo"
#define SHORTCUT_UNDO         CTRL_MOD "Z"
#define MENU_REDO             ICON_FA_REDO "  Redo"
#define SHORTCUT_REDO         CTRL_MOD "Shift+Z"
#define MENU_RECORD           ICON_FA_CIRCLE "  Record"
#define SHORTCUT_RECORD       CTRL_MOD "R"
#define MENU_RECORDCONT       ICON_FA_STOP_CIRCLE "  Save & continue"
#define SHORTCUT_RECORDCONT   CTRL_MOD "Shift+R"
#define MENU_RECORDPAUSE      ICON_FA_PAUSE_CIRCLE "  Pause Record"
#define SHORTCUT_RECORDPAUSE  CTRL_MOD "Space"
#define MENU_CAPTUREFRAME     ICON_FA_CAMERA_RETRO "  Capture frame"
#define SHORTCUT_CAPTURE_DISPLAY "F11"
#define SHORTCUT_CAPTURE_PLAYER "F10"
#define MENU_CAPTUREGUI       ICON_FA_CAMERA "  Screenshot vimix"
#define SHORTCUT_CAPTURE_GUI  "F9"
#define MENU_OUTPUTDISABLE    ICON_FA_EYE_SLASH " Disable"
#define SHORTCUT_OUTPUTDISABLE "F12"
#define ICON_PREVIEW          4, 15
#define MENU_PREVIEW          "Preview"
#define SHORTCUT_PREVIEW_OUT  "F6"
#define SHORTCUT_PREVIEW_SRC  "F7"
#define MENU_CLOSE            ICON_FA_TIMES "   Close"
#define DIALOG_FAILED_SOURCE  ICON_FA_EXCLAMATION_TRIANGLE " Source failure"

#define MENU_METRICS          ICON_FA_TACHOMETER_ALT "  Metrics"
#define MENU_SOURCE_TOOL      ICON_FA_WRENCH "  Source toolbar"
#define MENU_HELP             ICON_FA_LIFE_RING "  Help"
#define SHORTCUT_HELP         CTRL_MOD "H"
#define MENU_LOGS             ICON_FA_LIST_UL "  Logs"
#define SHORTCUT_LOGS         CTRL_MOD "L"
#define MENU_PLAYER           ICON_FA_PLAY_CIRCLE "  Player "
#define TOOLTIP_PLAYER        "Player "
#define SHORTCUT_PLAYER       CTRL_MOD "P"
#define MENU_OUTPUT           ICON_FA_DESKTOP " Display "
#define TOOLTIP_OUTPUT        "Display "
#define SHORTCUT_OUTPUT       CTRL_MOD "D"
#define MENU_TIMER            ICON_FA_CLOCK "  Timer "
#define TOOLTIP_TIMER         "Timer "
#define SHORTCUT_TIMER        CTRL_MOD "T"
#define MENU_INPUTS           ICON_FA_HAND_PAPER "  Inputs mapping "
#define TOOLTIP_INPUTS        "Inputs mapping "
#define SHORTCUT_INPUTS       CTRL_MOD "I"
#define TOOLTIP_SHADEREDITOR  "Shader Editor "
#define SHORTCUT_SHADEREDITOR CTRL_MOD "E"
#define TOOLTIP_FULLSCREEN    "Fullscreen "
#define SHORTCUT_FULLSCREEN   CTRL_MOD "F"
#define TOOLTIP_MAIN          "Main menu "
#define SHORTCUT_MAIN         "HOME"
#define TOOLTIP_NEW_SOURCE    "Insert "
#define SHORTCUT_NEW_SOURCE   "INS"
#define TOOLTIP_SNAP_CURSOR   "Snap cursor "
#define TOOLTIP_HIDE          "Hide windows "
#define TOOLTIP_SHOW          "Show windows "
#define SHORTCUT_HIDE         "ESC"
#define TOOLTIP_PANEL_VISIBLE "Panel always visible "
#define TOOLTIP_PANEL_AUTO    "Panel auto hide "
#define SHORTCUT_PANEL_MODE   "HOME"
#define MENU_PLAY_PAUSE       ICON_FA_PLAY "  Play | Pause"
#define SHORTCUT_PLAY_PAUSE   "Space"
#define MENU_PLAY_BEGIN       ICON_FA_FAST_BACKWARD "  Go to Beginning"
#define SHORTCUT_PLAY_BEGIN   CTRL_MOD "B"

#define LABEL_AUTO_MEDIA_PLAYER ICON_FA_USER_CIRCLE "  User selection"
#define LABEL_STORE_SELECTION "  Create batch"
#define LABEL_EDIT_FADING ICON_FA_RANDOM "  Fade in & out"
#define LABEL_VIDEO_SEQUENCE  "  Encode an image sequence"
#define LABEL_ADD_TIMELINE    "Add timeline"
#define DIALOG_TIMELINE_DURATION    ICON_FA_HOURGLASS_HALF " Set timeline duration"
#define DIALOG_GST_EFFECT     "Gstreamer Video effect"

#endif // VMIX_DEFINES_H
