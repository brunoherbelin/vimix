#ifndef VMIX_DEFINES_H
#define VMIX_DEFINES_H

#define APP_NAME "vimix"
#define APP_TITLE "Video Live Mixer"
#define APP_SETTINGS "vimix.xml"
#define XML_VERSION_MAJOR 0
#define XML_VERSION_MINOR 5
#define MAX_RECENT_HISTORY 20
#define MAX_SESSION_LEVEL 3
#define MAX_BUNDLE_LEVEL 4
#define MAX_OUTPUT_WINDOW 3
#define MAX_OUTPUT_CANVAS 12
#define MAX_VIEWSPACE 4

#define VIMIX_GL_VERSION "opengl3"
#define VIMIX_GLSL_VERSION "#version 150"
#define VIMIX_GL_MULTISAMPLE 3

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

#define VIMIX_ALL_FILES_TYPE "vimix files (sessions, playlists)"
#define VIMIX_ALL_FILES_PATTERN \
    { \
        "*.mix", "*.lix" \
    }

#define MEDIA_FILES_TYPE "Supported formats (videos, images, sessions)"
#define MEDIA_FILES_PATTERN \
    { \
        "*.mix", "*.mp4", "*.mpg", "*.mpeg", "*.m2v", "*.m4v", "*.avi", "*.mov", "*.mkv", \
        "*.webm", "*.mod", "*.wmv", "*.mxf", "*.ogg", "*.flv", "*.hevc", "*.asf", "*.jpg", "*.jpeg", \
        "*.png", "*.gif", "*.tif", "*.tiff", "*.webp", "*.bmp", "*.ppm", "*.svg" \
    }
#define AUDIO_FILES_TYPE "Audio (WAV, MP3, FLAC, AAC, OPUS, AIFF, etc.)"
#define AUDIO_FILES_PATTERN \
    { \
        "*.wav", "*.mp3", "*.flac", "*.aac", "*.m4a", "*.oga", "*.opus", "*.aif", "*.aiff", "*.wma" \
    }
#define IMAGES_FILES_TYPE "Image (JPG, PNG, WEBP, BMP, PPM, GIF)"
#define IMAGES_FILES_PATTERN \
    { \
        "*.jpg", "*.jpeg", "*.png", "*.webp", "*.bmp", "*.ppm", "*.gif" \
    }
#define SUBTITLE_FILES_TYPE "Subtitle (SRT, SUB)"
#define SUBTITLE_FILES_PATTERN \
    { \
        "*.srt", "*.sub" \
    }
#define SETTINGS_FILE_TYPE "vimix settings (XML)"
#define SETTINGS_FILE_EXT "xml"
#define SETTINGS_FILE_PATTERN \
{ \
        "*.xml" \
}
#define APP_CANVAS "config.vic"
#define CONFIG_FILE_TYPE "vimix configuration (VIC)"
#define CONFIG_FILE_EXT "vic"
#define CONFIG_FILE_PATTERN \
{ \
        "*.vic" \
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
#define GEOMETRY_DEFAULT_SCALE 1.8f
#define GEOMETRY_MIN_SCALE 0.4f
#define GEOMETRY_MAX_SCALE 7.0f
#define LAYER_DEFAULT_SCALE 0.6f
#define LAYER_MIN_SCALE 0.25f
#define LAYER_MAX_SCALE 1.7f
#define LAYER_PERSPECTIVE 2.0f
#define LAYER_DEFAULT_DEPTH 2.f
#define LAYER_STEP 0.25f
#define APPEARANCE_DEFAULT_SCALE 2.f
#define APPEARANCE_MIN_SCALE 0.4f
#define APPEARANCE_MAX_SCALE 7.0f
#define BRUSH_MIN_SIZE 0.01f
#define BRUSH_MAX_SIZE 2.f
#define BRUSH_MIN_PRESS 0.005f
#define BRUSH_MAX_PRESS 1.f
#define SHAPE_MIN_BLUR 0.f
#define SHAPE_MAX_BLUR 1.f
#define TRANSITION_DEFAULT_SCALE 5.0f
#define TRANSITION_MIN_DURATION 0.2f
#define TRANSITION_MAX_DURATION 10.f
#define DISPLAYS_UNIT 0.001f
#define DISPLAYS_DEFAULT_SCALE 1.2f
#define DISPLAYS_MIN_SCALE 0.4f
#define DISPLAYS_MAX_SCALE 7.0f
#define ARROWS_MOVEMENT_FACTOR 0.1f
#define SESSION_THUMBNAIL_HEIGHT 120.f
#define RECORD_MAX_TIMEOUT 1200000

#define SOURCE_TYPE_MEDIA "MediaSource"
#define SOURCE_TYPE_SPLIT_MEDIA "SplitMediaSource"
#define SOURCE_TYPE_AUDIO "AudioSource"
#define SOURCE_TYPE_SESSION "SessionSource"
#define SOURCE_TYPE_GROUP "GroupSource"
#define SOURCE_TYPE_RENDER "RenderSource"
#define SOURCE_TYPE_CLONE "CloneSource"
#define SOURCE_TYPE_PATTERN "PatternSource"
#define SOURCE_TYPE_TEXT "TextSource"
#define SOURCE_TYPE_SHADER "ShaderSource"
#define SOURCE_TYPE_DEVICE "DeviceSource"
#define SOURCE_TYPE_SCREEN_CAPTURE "ScreenCaptureSource"
#define SOURCE_TYPE_NETWORK "NetworkSource"
#define SOURCE_TYPE_MULTI_FILE "MultiFileSource"
#define SOURCE_TYPE_GENERIC_STREAM "GenericStreamSource"
#define SOURCE_TYPE_SRT_RECEIVER "SrtReceiverSource"
#define SOURCE_TYPE_CANVAS "CanvasSource"
#define SOURCE_TYPE_CANVAS_SURFACE "CanvasSurface"
#define SOURCE_TYPE_DRAFT "DraftSource"
#define SOURCE_TYPE_THUMBNAIL "SessionThumbnailSource"

#define IMGUI_TITLE_LOGS ICON_FA_LIST_UL "  Logs"
#define IMGUI_LABEL_RECENT_FILES " Recent files"
#define IMGUI_LABEL_RECENT_RECORDS " Recent recordings"
#define IMGUI_RIGHT_ALIGN -3.8f * ImGui::GetTextLineHeightWithSpacing()
#define IMGUI_SAME_LINE 8.f
#define IMGUI_TOP_ALIGN 10.f
#define IMGUI_COLOR_OVERLAY IM_COL32(5, 5, 5, 150)
#define IMGUI_COLOR_LIGHT_OVERLAY IM_COL32(5, 5, 5, 50)
#define IMGUI_COLOR_CAPTURE 1.0, 0.55, 0.05
#define IMGUI_COLOR_RECORD 1.0, 0.05, 0.05
#define IMGUI_COLOR_STREAM 0.1, 0.9, 1.0
#define IMGUI_COLOR_BROADCAST 1.0, 0.9, 0.3
#define IMGUI_NOTIFICATION_DURATION 2.5f
#define IMGUI_TOOLTIP_TIMEOUT 80
#define IMGUI_COLOR_FAILED 1.0, 0.4, 0.3
#define IMGUI_COLOR_RED 1.0, 0.2, 0.2
#define IMGUI_COLOR_GREEN 0.2, 1.0, 0.2
#define PLOT_CIRCLE_SEGMENTS 64

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
#undef COLOR_WINDOW  // (a constant of windows.h)
#define COLOR_WINDOW 0.2f, 0.85f, 0.85f
#define COLOR_MENU_HOVERED 0.3f, 0.3f, 0.3f
#define COLOR_AUTOMATON 0.95f, 0.3f, 0.3f
#define COLOR_PRESENTATION_BGROUND 0.1f, 0.1f, 0.1f

//
// Look of automatons (Kinetics)
//
// Sizes of graphics on screen (in units of the projection, independent of zoom)
#define AUTOMATON_GRIP_SIZE          0.04f
#define AUTOMATON_GRIP_PICK          0.25f
#define AUTOMATON_GRIP_HOVER_SCALE   1.3f   // grips under the cursor are bigger
#define AUTOMATON_GRIP_BORDER    1.3f   // border around marked grips (relative to grip size)
#define AUTOMATON_MOVER_SIZE         0.07f
#define AUTOMATON_MOVER_PICK_SCALE   1.3f   // mover easier to pick than grips
#define AUTOMATON_MOVER_HOVER_SCALE  1.2f
#define AUTOMATON_LINK_POINT_SIZE    0.02f  // point at the end of link lines, on the source
#define AUTOMATON_KEY_BUTTON_SIZE    0.06f  // buttons to add / remove key points of free paths
#define AUTOMATON_KEY_BUTTON_PICK    0.07f
#define AUTOMATON_KEY_BUTTON_AHEAD   0.16f  // distance of buttons after the end point (along the path)
#define AUTOMATON_KEY_BUTTON_SIDE    0.07f // distance of buttons on each side of the path
// Width of lines (in pixels)
#define AUTOMATON_LINE_WIDTH         3.0f   // path of path automatons
#define AUTOMATON_LINE_WIDTH_MOTION  (AUTOMATON_LINE_WIDTH * 1.6f) // moving lines of field automatons (heart ring, rift lines)
#define AUTOMATON_LINE_WIDTH_GUIDE   (AUTOMATON_LINE_WIDTH * 0.8f) // limits of field automatons (heart max ring, rift axis)
#define AUTOMATON_LINE_WIDTH_LINK    (AUTOMATON_LINE_WIDTH * 0.8f)  // lines from mover to linked sources
// Opacity of drawings [0 1]
#define AUTOMATON_ALPHA_PATH         0.8f
#define AUTOMATON_ALPHA_MOVER        1.0f
#define AUTOMATON_ALPHA_MOTION       0.9f   // moving lines of field automatons
#define AUTOMATON_ALPHA_GUIDE        0.5f   // limits of field automatons
#define AUTOMATON_ALPHA_FILL         0.25f  // disk of heart
#define AUTOMATON_ALPHA_LINK         0.5f
#define AUTOMATON_ALPHA_LINK_POINT   0.8f
#define AUTOMATON_ALPHA_LINKING      1.0f   // line while dragging the mover to link
#define AUTOMATON_ALPHA_GRIP         0.7f
#define AUTOMATON_ALPHA_GRIP_MOVE    0.6f   // grip to move whole automaton
#define AUTOMATON_ALPHA_HOVER        1.0f   // grip under the cursor
// Factors of opacity
#define AUTOMATON_ALPHA_DISABLED     0.4f   // automaton disabled
#define AUTOMATON_ALPHA_VIEWSPACE    0.3f   // other ViewSpaces shown (not interactive)
#define AUTOMATON_ALPHA_HIGHLIGHT    1.3f   // automaton selected in the control window
// Sampling of curves
#define AUTOMATON_PATH_SAMPLES 96
// half size of the meshes of symbols used for drawing
#define AUTOMATON_MESH_POINT_SIZE 0.044f
#define AUTOMATON_MESH_CROSS_SIZE 0.7f
#define AUTOMATON_MESH_SQUARE_SIZE 0.574f
#define AUTOMATON_MESH_ICON_SIZE   0.11f

#define OSC_PORT_RECV_DEFAULT 7000
#define OSC_PORT_SEND_DEFAULT 7001
#define OSC_CONFIG_FILE "osc.xml"

#define IMGUI_TITLE_MEDIAPLAYER ICON_FA_PLAY_CIRCLE "  Player"
#define IMGUI_TITLE_TIMER ICON_FA_CLOCK " Timer"
#define IMGUI_TITLE_AUTOMATONS ICON_FA_ATOM " Kinetics"
#define IMGUI_TITLE_INPUT_MAPPING ICON_FA_HAND_PAPER " Input Mapping"
#define IMGUI_TITLE_HELP ICON_FA_LIFE_RING "  Help"
#define IMGUI_TITLE_TOOLBOX ICON_FA_HAMSA "  Guru Toolbox"
#define IMGUI_TITLE_SHADEREDITOR ICON_FA_CODE "  Shader Editor"
#define IMGUI_TITLE_PREVIEW ICON_FA_DESKTOP "  Mix"

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
#define MENU_DELETE_FILE      ICON_FA_TRASH_ALT "  Delete"
#define MENU_SAVE_ON_EXIT     ICON_FA_SIGN_OUT_ALT " Save on exit"
#define MENU_OPEN_ON_START    ICON_FA_SIGN_IN_ALT " Restore on start"
#define SHORTCUT_SAVEAS_FILE  CTRL_MOD "Shift+S"
#define MENU_EXPORT_SETTINGS  ICON_FA_FILE_EXCEL "  Export settings"
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
#define MENU_OUTPUTDISABLE    ICON_FA_EYE_SLASH " Disable outputs"
#define SHORTCUT_OUTPUTDISABLE "F12"
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
#define MENU_OUTPUT           ICON_FA_DESKTOP "  Mix "
#define TOOLTIP_OUTPUT        "Mix "
#define SHORTCUT_OUTPUT       CTRL_MOD "M"
#define MENU_TIMER            ICON_FA_CLOCK "  Timer "
#define TOOLTIP_TIMER         "Timer "
#define SHORTCUT_TIMER        CTRL_MOD "T"
#define MENU_AUTOMATONS       ICON_FA_ATOM " Kinetics "
#define SHORTCUT_AUTOMATONS   CTRL_MOD "K"
#define MENU_INPUTS           ICON_FA_HAND_PAPER "  Input mapping "
#define TOOLTIP_INPUTS        "Inputs mapping "
#define SHORTCUT_INPUTS       CTRL_MOD "I"
#define MENU_SHADEREDITOR     ICON_FA_CODE " Shader Editor "
#define TOOLTIP_SHADEREDITOR  "Shader Editor "
#define SHORTCUT_SHADEREDITOR CTRL_MOD "E"
#define TOOLTIP_FULLSCREEN    "Fullscreen "
#define SHORTCUT_FULLSCREEN   CTRL_MOD "F"
#define TOOLTIP_MAIN          "Home panel "
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
#define SHORTCUT_BUNDLE       CTRL_MOD "B"
#define MENU_DRAFT            ICON_FA_PENCIL_RULER "  Draft"
#define SHORTCUT_DRAFT        CTRL_MOD "D"
#define MENU_DRAFT_START      "Start draft"
#define MENU_DRAFT_APPLY      "Apply draft"
#define MENU_DRAFT_CANCEL     ICON_FA_TIMES "  Cancel draft"
#define SHORTCUT_DRAFT_CANCEL CTRL_MOD "Shift+D"

#define LABEL_PLAYER_SELECTION ICON_FA_USER_CIRCLE "  User selection"
#define LABEL_PLAYER_BATCH    ICON_FA_CHECK_CIRCLE "  Batch #"
#define LABEL_PLAYER_BATCH_ADD "  Add batch"
#define LABEL_PLAYER_TIMELINE_ADD    "Add timeline"
#define LABEL_EDIT_FADING     ICON_FA_RANDOM "  Edit timeline fading"
#define LABEL_EDIT_GAPS       ICON_FA_CUT "  Cut timeline"
#define LABEL_VIDEO_SEQUENCE  ICON_FA_FILM "  Encode an image sequence"
#define LABEL_VIDEO_CONVERT   ICON_FA_FILM "  Convert videos"
#define DIALOG_TIMELINE_DURATION    ICON_FA_CLOCK "  Set timeline duration"
#define DIALOG_GST_EFFECT     " Gstreamer Video effect"
#define DIALOG_EXPORT_SECTIONS ICON_FA_FILE_EXPORT ICON_FA_FILE_VIDEO "  Export timeline to video"
#define LABEL_SHADER_EMBEDDED ICON_FA_FILE_ALT "  Embedded code"
#define LABEL_SHADER_SAVE     ICON_FA_FILE_DOWNLOAD "  Save as"
#define LABEL_SHADER_ADD      ICON_FA_FILE_MEDICAL  "  Load"

#endif // VMIX_DEFINES_H
