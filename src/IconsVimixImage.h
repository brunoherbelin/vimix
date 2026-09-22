/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2026 Bruno Herbelin <bruno.herbelin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
**/

#ifndef ICONSVIMIXIMAGE_H
#define ICONSVIMIXIMAGE_H

// sources icons
#define ICON_VI_SOURCE_VIDEO         18, 13
#define ICON_VI_SOURCE_IMAGE         4, 9
#define ICON_VI_SOURCE_DEVICE_SCREEN 0, 2
#define ICON_VI_SOURCE_DEVICE        2, 14
#define ICON_VI_SOURCE_SEQUENCE      3, 9
#define ICON_VI_SOURCE_NETWORK       18, 11
#define ICON_VI_SOURCE_PATTERN       5, 3
#define ICON_VI_SOURCE_SESSION       19, 6
#define ICON_VI_SOURCE_GROUP         6, 3
#define ICON_VI_SOURCE_RENDER        19, 1
#define ICON_VI_SOURCE_CLONE         9, 2
#define ICON_VI_SOURCE_GSTREAMER     16, 16
#define ICON_VI_SOURCE_SRT           14, 5
#define ICON_VI_SOURCE_TEXT          0, 13
#define ICON_VI_SOURCE_SHADER        16, 14
#define ICON_VI_SOURCE               14, 11

// views icons
#define ICON_VI_DISPLAYS          10, 7
#define ICON_VI_DISPLAYS_DISABLED 10, 8
#define ICON_VI_LAYERS            13, 16
#define ICON_VI_LAYERS_BACKGROUND 10, 16
#define ICON_VI_LAYERS_CENTRAL    11, 16
#define ICON_VI_LAYERS_FOREGROUND 12, 16

// filters icons
#define ICON_VI_FILTER_NONE          7, 11
#define ICON_VI_FILTER_DELAY         10, 15
#define ICON_VI_FILTER_RESAMPLE      1, 10
#define ICON_VI_FILTER_BLUR          0, 9
#define ICON_VI_FILTER_SHARPEN       2, 1
#define ICON_VI_FILTER_SMOOTH        14, 8
#define ICON_VI_FILTER_EDGE          16, 8
#define ICON_VI_FILTER_ALPHA         13, 4
#define ICON_VI_FILTER_IMAGE         1, 4

// navigator pannels and views
#define ICON_VI_VIMIX_LOGO        2, 16
#define ICON_VI_PANNEL_SESSION    7, 1
#define ICON_VI_PANNEL_PLAYLIST   4, 8
#define ICON_VI_PANNEL_SETTINGS   13, 5
#define ICON_VI_VIEW_DISPLAYS     10, 7
#define ICON_VI_ZOOM_RESET        8, 7
#define ICON_VI_GRID_SQUARE       19, 2
#define ICON_VI_GRID_ASPECT_RATIO 18, 2
#define ICON_VI_HIDDEN_SOURCES    12, 0

// new source pannel
#define ICON_VI_OPEN_FILE            2, 5
#define ICON_VI_NEW_SOURCE_CONNECTED 10, 9
#define ICON_VI_SELECT_ADD           1, 5
#define ICON_VI_SELECT_REMOVE        14, 1
#define ICON_VI_SELECT_ALL           13, 1
#define ICON_VI_SELECT_VISIBLE       8, 1
#define ICON_VI_RELOAD               5, 15
#define ICON_VI_CLEAR_LIST           12, 14

// files, folders, playlists and sessions
#define ICON_VI_SHOW_IN_FINDER      3, 5
#define ICON_VI_FOLDER              6, 5
#define ICON_VI_FOLDER_ADD          5, 5
#define ICON_VI_FOLDER_CLOSE        4, 5
#define ICON_VI_PLAYLIST            12, 3
#define ICON_VI_PLAYLIST_NEW        13, 3
#define ICON_VI_PLAYLIST_CREATE     16, 3
#define ICON_VI_FAVORITES           16, 4
#define ICON_VI_FAVORITE_REMOVE     15, 4
#define ICON_VI_ADD                 18, 4
#define ICON_VI_MULTIPLE_REMOVE     0, 5
#define ICON_VI_REMOVE              19, 4
#define ICON_VI_DRAG_HANDLE         8, 15
#define ICON_VI_THUMBNAIL_AUTOMATIC 2, 8
#define ICON_VI_THUMBNAIL_CUSTOM    7, 8

// windows, menus and dialogs
#define ICON_VI_CLOSE_WIDGET    4, 16
#define ICON_VI_PANNEL_EXPAND   11, 0
#define ICON_VI_PANNEL_COLLAPSE 10, 0
#define ICON_VI_MENU_OPTIONS    5, 8
#define ICON_VI_MENU_AUDIO      6, 2
#define ICON_VI_MENU_FLAGS      3, 0
#define ICON_VI_WARNING         7, 14
#define ICON_VI_STREAM          19, 11

// bundles
#define ICON_VI_BUNDLE_CREATE  11, 2
#define ICON_VI_BUNDLE_UNCOVER 7, 2

// source geometry and properties
#define ICON_VI_TRANSFORM_RESET       1, 16
#define ICON_VI_POSITION_RESET        6, 15
#define ICON_VI_SCALE_RESET           3, 15
#define ICON_VI_ANGLE                 18, 9
#define ICON_VI_ASPECT_UNLINKED       5, 1
#define ICON_VI_ASPECT_LINKED         6, 1
#define ICON_VI_UNLOCKED              15, 6
#define ICON_VI_LOCKED                17, 6
#define ICON_VI_COLOR                 10, 2
#define ICON_VI_HARDWARE_DECODING_OFF 14, 2

// text source alignment
#define ICON_VI_ALIGN_LEFT       17, 16
#define ICON_VI_ALIGN_CENTER_H   18, 16
#define ICON_VI_ALIGN_RIGHT      19, 16
#define ICON_VI_ALIGN_ABSOLUTE_H 6, 10
#define ICON_VI_ALIGN_BOTTOM     1, 17
#define ICON_VI_ALIGN_CENTER_V   3, 17
#define ICON_VI_ALIGN_TOP        2, 17
#define ICON_VI_ALIGN_ABSOLUTE_V 3, 10
#define ICON_VI_FONT             1, 13

// media player : playback
#define ICON_VI_PLAY_FORWARD           8, 0
#define ICON_VI_PLAY_BACKWARD          9, 0
#define ICON_VI_PLAY                   12, 7
#define ICON_VI_SPEED_RESET            19, 15
#define ICON_VI_SPEED_ACCELERATE       14, 16
#define ICON_VI_SPEED_SLOWDOWN         15, 16
#define ICON_VI_DURATION_FASTER_SLOWER 0, 12
#define ICON_VI_DURATION_FASTER        8, 12
#define ICON_VI_DURATION_SLOWER        9, 12
#define ICON_VI_SLIDER_RENDER          8, 9
#define ICON_VI_SLIDER_SPLIT           6, 9
#define ICON_VI_SLIDER_INPUT           7, 9

// media player : timeline
#define ICON_VI_TIMELINE_ADD    0, 14
#define ICON_VI_TIMELINE_REMOVE 1, 14
#define ICON_VI_TIMELINE_CLEAR  11, 14
#define ICON_VI_FLAG_ADD_CURSOR 0, 0
#define ICON_VI_FLAG_ADD_TIME   1, 0
#define ICON_VI_FLAG_DELETE     2, 0
#define ICON_VI_FLAG_NEXT       5, 0
#define ICON_VI_FLAG_PREVIOUS   6, 0
#define ICON_VI_TIMELINE_CUT    11, 3
#define ICON_VI_CUT_TIME_LEFT   17, 3
#define ICON_VI_CUT_TIME_RIGHT  18, 3
#define ICON_VI_GAP_REMOVE_END  7, 0
#define ICON_VI_GAP_NONE        0, 4
#define ICON_VI_GAP_MERGE_NONE  19, 3
#define ICON_VI_GAP_CLEAN_CURVE 3, 7
#define ICON_VI_SMOOTH_FILTER   2, 7
#define ICON_VI_CLEAR_TEXT      11, 13

// input mapping, metronome and timer
#define ICON_VI_KEY_PRESS         2, 13
#define ICON_VI_KEY_DOWN          3, 13
#define ICON_VI_KEY_REPEAT        18, 5
#define ICON_VI_METRONOME         4, 13
#define ICON_VI_SYNC_NONE         5, 13
#define ICON_VI_SYNC_BEAT         6, 13
#define ICON_VI_SYNC_PHASE        7, 13
#define ICON_VI_METRONOME_RESTART 9, 13
#define ICON_VI_TIMER_RESET       8, 13
#define ICON_VI_LINK_PEERS        16, 5

// transition view
#define ICON_VI_TRANSITION_FADE_BLACK 9, 8
#define ICON_VI_TRANSITION_CROSS_FADE 0, 8
#define ICON_VI_TRANSITION_LINEAR     11, 12
#define ICON_VI_TRANSITION_QUADRATIC  10, 12

// displays, texture view and settings
#define ICON_VI_BRIGHTNESS      4, 1
#define ICON_VI_BRIGHTNESS_LOW  3, 1
#define ICON_VI_CONTRAST        5, 16   
#define ICON_VI_CONTRAST_LOW    6, 16
#define ICON_VI_TEST_PATTERN    11, 1
#define ICON_VI_TABLET_PRESSURE 13, 0
#define ICON_VI_BRUSH_SMALL     15, 1
#define ICON_VI_BRUSH_LARGE     16, 1
#define ICON_VI_BLUR_SHARP      8, 16
#define ICON_VI_BUFFER          4, 6
#define ICON_VI_GPU             13, 2
#define ICON_VI_GPU_OFF         14, 2

// session notes
#define ICON_VI_NOTE_ALL_VIEWS 5, 2
#define ICON_VI_NOTE_THIS_VIEW 4, 2

// media player : loop modes and edition tools
#define ICON_VI_LOOP_STOP     0, 15
#define ICON_VI_LOOP_RESTART  1, 15
#define ICON_VI_LOOP_BOUNCE   19, 14
#define ICON_VI_LOOP_BLACKOUT 18, 14
#define ICON_VI_TOOL_CUT      8, 3
#define ICON_VI_TOOL_FADE     7, 4
#define ICON_VI_FLAG_BOOKMARK 11, 6
#define ICON_VI_FLAG_STOP     12, 6
#define ICON_VI_FLAG_BLACKOUT 13, 6
#define ICON_VI_CUT_START     9, 3
#define ICON_VI_CUT_END       10, 3

// media player : fading curves inserted in the timeline
#define ICON_VI_FADE_SHARP         12, 12
#define ICON_VI_FADE_LINEAR        13, 12
#define ICON_VI_FADE_SMOOTH        14, 12
#define ICON_VI_FADE_SHARP_IN      14, 10
#define ICON_VI_FADE_SHARP_OUT     11, 10
#define ICON_VI_FADE_SHARP_IN_OUT  9, 10
#define ICON_VI_FADE_LINEAR_IN     15, 10
#define ICON_VI_FADE_LINEAR_OUT    12, 10
#define ICON_VI_FADE_LINEAR_IN_OUT 7, 10
#define ICON_VI_FADE_SMOOTH_IN     16, 10
#define ICON_VI_FADE_SMOOTH_OUT    13, 10
#define ICON_VI_FADE_SMOOTH_IN_OUT 17, 10
#define ICON_VI_FADE_IN            19, 7
#define ICON_VI_FADE_OUT           18, 7

// blending modes
#define ICON_VI_BLENDING            5, 6
#define ICON_VI_BLEND_SCREEN        7, 6
#define ICON_VI_BLEND_MULTIPLY      9, 6
#define ICON_VI_BLEND_SUBTRACT      8, 6
#define ICON_VI_BLEND_SOFT_SUBTRACT 6, 6
#define ICON_VI_BLEND_HARD_LIGHT    2, 6
#define ICON_VI_BLEND_SOFT_LIGHT    3, 6
#define ICON_VI_BLEND_LIGHTEN       10, 6

// render source provenance
#define ICON_VI_RENDER_CANVAS      2, 10
#define ICON_VI_RENDER_RECURSIVE   16, 12
#define ICON_VI_RENDER_LOCAL_SCENE 17, 12

// ordering of a list of files
#define ICON_VI_ORDER_ALPHABETICAL        2, 12
#define ICON_VI_ORDER_ALPHABETICAL_INVERT 3, 12
#define ICON_VI_ORDER_OLDEST_FIRST        4, 12
#define ICON_VI_ORDER_NEWEST_FIRST        5, 12

// input mapping : source attributes
#define ICON_VI_SCALE_INCREMENT 2, 15
#define ICON_VI_ALPHA           18, 12
#define ICON_VI_ALPHA_INCREMENT 19, 12
#define ICON_VI_SEEK_STEP       13, 7
#define ICON_VI_TIME_TARGET     15, 7

// color correction
#define ICON_VI_COLOR_HUE        3, 4
#define ICON_VI_COLOR_INVERT     4, 4
#define ICON_VI_COLOR_THRESHOLD  5, 4
#define ICON_VI_COLOR_GAMMA      6, 4
#define ICON_VI_COLOR_SATURATION 9, 16

// input mapping : speed of the applied value
#define ICON_VI_SPEED_FASTEST 18, 15
#define ICON_VI_SPEED_FAST    17, 15
#define ICON_VI_SPEED_SMOOTH  16, 15
#define ICON_VI_SPEED_SLOW    15, 15
#define ICON_VI_SPEED_SLOWEST 14, 15
#define ICON_VI_EXECUTE       8,  0

// fading curve shapes (same images as the cut and gap buttons above)
#define ICON_VI_CURVE_ABRUPT      17, 3
#define ICON_VI_CURVE_LINEAR      18, 3
#define ICON_VI_CURVE_PROGRESSIVE 19, 3

//
// UNUSED ICONS
//
//
// #define ICON_VI_UNDEFINED_1    4,  0
// #define ICON_VI_UNDEFINED_2   14,  0
// #define ICON_VI_UNDEFINED_3   15,  0
// #define ICON_VI_UNDEFINED_4   16,  0
// #define ICON_VI_UNDEFINED_5   17,  0
// #define ICON_VI_UNDEFINED_6   18,  0
// #define ICON_VI_UNDEFINED_7   19,  0
//
// #define ICON_VI_UNDEFINED_8    0,  1
// #define ICON_VI_UNDEFINED_9    1,  1
// #define ICON_VI_UNDEFINED_10   9,  1
// #define ICON_VI_UNDEFINED_11  10,  1
// #define ICON_VI_UNDEFINED_12  12,  1
// #define ICON_VI_UNDEFINED_13  17,  1
// #define ICON_VI_UNDEFINED_14  18,  1
//
// #define ICON_VI_UNDEFINED_15   1,  2
// #define ICON_VI_UNDEFINED_16   2,  2
// #define ICON_VI_UNDEFINED_17   3,  2
// #define ICON_VI_UNDEFINED_18   8,  2
// #define ICON_VI_UNDEFINED_19  12,  2
// #define ICON_VI_UNDEFINED_20  15,  2
// #define ICON_VI_UNDEFINED_21  16,  2
// #define ICON_VI_UNDEFINED_22  17,  2
//
// #define ICON_VI_UNDEFINED_23   0,  3
// #define ICON_VI_UNDEFINED_24   1,  3
// #define ICON_VI_UNDEFINED_25   2,  3
// #define ICON_VI_UNDEFINED_26   3,  3
// #define ICON_VI_UNDEFINED_27   4,  3
// #define ICON_VI_UNDEFINED_28   6,  3
// #define ICON_VI_UNDEFINED_29  14,  3
// #define ICON_VI_UNDEFINED_30  15,  3
//
// #define ICON_VI_UNDEFINED_31   2,  4
// #define ICON_VI_UNDEFINED_32   8,  4
// #define ICON_VI_UNDEFINED_33   9,  4
// #define ICON_VI_UNDEFINED_34  10,  4
// #define ICON_VI_UNDEFINED_35  11,  4
// #define ICON_VI_UNDEFINED_36  12,  4
// #define ICON_VI_UNDEFINED_37  14,  4
// #define ICON_VI_UNDEFINED_38  17,  4
//
// #define ICON_VI_UNDEFINED_39   7,  5
// #define ICON_VI_UNDEFINED_40   8,  5
// #define ICON_VI_UNDEFINED_41   9,  5
// #define ICON_VI_UNDEFINED_42  10,  5
// #define ICON_VI_UNDEFINED_43  11,  5
// #define ICON_VI_UNDEFINED_44  12,  5
// #define ICON_VI_UNDEFINED_45  15,  5
// #define ICON_VI_UNDEFINED_46  17,  5
// #define ICON_VI_UNDEFINED_47  19,  5
//
// #define ICON_VI_UNDEFINED_48   0,  6
// #define ICON_VI_UNDEFINED_49   1,  6
// #define ICON_VI_UNDEFINED_50  14,  6
// #define ICON_VI_UNDEFINED_51  16,  6
// #define ICON_VI_UNDEFINED_52  18,  6
//
// #define ICON_VI_UNDEFINED_53   1,  7
// #define ICON_VI_UNDEFINED_54   4,  7
// #define ICON_VI_UNDEFINED_55   5,  7
// #define ICON_VI_UNDEFINED_56   6,  7
// #define ICON_VI_UNDEFINED_57   7,  7
// #define ICON_VI_UNDEFINED_58   9,  7
// #define ICON_VI_UNDEFINED_59  11,  7
// #define ICON_VI_UNDEFINED_60  14,  7
// #define ICON_VI_UNDEFINED_62  17,  7
//
// #define ICON_VI_UNDEFINED_63   1,  8
// #define ICON_VI_UNDEFINED_64   3,  8
// #define ICON_VI_UNDEFINED_65   6,  8
// #define ICON_VI_UNDEFINED_66   8,  8
// #define ICON_VI_UNDEFINED_67  11,  8
// #define ICON_VI_UNDEFINED_68  12,  8
// #define ICON_VI_UNDEFINED_69  13,  8
// #define ICON_VI_UNDEFINED_70  15,  8
// #define ICON_VI_UNDEFINED_71  17,  8
// #define ICON_VI_UNDEFINED_72  18,  8
// #define ICON_VI_UNDEFINED_73  19,  8
//
// #define ICON_VI_UNDEFINED_74   1,  9
// #define ICON_VI_UNDEFINED_75   2,  9
// #define ICON_VI_UNDEFINED_76   5,  9
// #define ICON_VI_UNDEFINED_77   9,  9
// #define ICON_VI_UNDEFINED_78  17,  9
// #define ICON_VI_UNDEFINED_79  19,  9
//
// #define ICON_VI_UNDEFINED_80   0, 10
// #define ICON_VI_UNDEFINED_81   4, 10
// #define ICON_VI_UNDEFINED_82   5, 10
// #define ICON_VI_UNDEFINED_83   8, 10
// #define ICON_VI_UNDEFINED_84  10, 10
// #define ICON_VI_UNDEFINED_85  18, 10
// #define ICON_VI_UNDEFINED_86  19, 10
//
// #define ICON_VI_UNDEFINED_87   0, 11
// #define ICON_VI_UNDEFINED_88   1, 11
// #define ICON_VI_UNDEFINED_89   2, 11
// #define ICON_VI_UNDEFINED_90   3, 11
// #define ICON_VI_UNDEFINED_91   4, 11
// #define ICON_VI_UNDEFINED_92   5, 11
// #define ICON_VI_UNDEFINED_93   6, 11
// #define ICON_VI_UNDEFINED_94   8, 11
// #define ICON_VI_UNDEFINED_95   9, 11
// #define ICON_VI_UNDEFINED_96  10, 11
// #define ICON_VI_UNDEFINED_97  11, 11
// #define ICON_VI_UNDEFINED_98  12, 11
// #define ICON_VI_UNDEFINED_99  13, 11
// #define ICON_VI_UNDEFINED_100 15, 11
// #define ICON_VI_UNDEFINED_101 16, 11
// #define ICON_VI_UNDEFINED_102 17, 11
//
// #define ICON_VI_UNDEFINED_103  1, 12
// #define ICON_VI_UNDEFINED_104  6, 12
// #define ICON_VI_UNDEFINED_105  7, 12
// #define ICON_VI_UNDEFINED_106 15, 12
//
// #define ICON_VI_UNDEFINED_107 10, 13
// #define ICON_VI_UNDEFINED_108 12, 13
// #define ICON_VI_UNDEFINED_109 13, 13
// #define ICON_VI_UNDEFINED_110 14, 13
// #define ICON_VI_UNDEFINED_111 15, 13
// #define ICON_VI_UNDEFINED_112 16, 13
// #define ICON_VI_UNDEFINED_113 17, 13
// #define ICON_VI_UNDEFINED_114 19, 13
//
// #define ICON_VI_UNDEFINED_115  3, 14
// #define ICON_VI_UNDEFINED_116  4, 14
// #define ICON_VI_UNDEFINED_117  5, 14
// #define ICON_VI_UNDEFINED_118  6, 14
// #define ICON_VI_UNDEFINED_119  8, 14
// #define ICON_VI_UNDEFINED_120  9, 14
// #define ICON_VI_UNDEFINED_121 10, 14
// #define ICON_VI_UNDEFINED_122 13, 14
// #define ICON_VI_UNDEFINED_123 14, 14
// #define ICON_VI_UNDEFINED_124 15, 14
// #define ICON_VI_UNDEFINED_125 17, 14
//
// #define ICON_VI_UNDEFINED_126  7, 15
// #define ICON_VI_UNDEFINED_127  9, 15
// #define ICON_VI_UNDEFINED_128 11, 15
// #define ICON_VI_UNDEFINED_129 12, 15
// #define ICON_VI_UNDEFINED_130 13, 15
//
// #define ICON_VI_UNDEFINED_131  0, 16
// #define ICON_VI_UNDEFINED_132  3, 16
// #define ICON_VI_UNDEFINED_133  7, 16
//

#endif // ICONSVIMIXIMAGE_H
