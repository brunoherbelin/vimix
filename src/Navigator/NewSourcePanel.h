/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2023 Bruno Herbelin <bruno.herbelin@gmail.com>
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
#ifndef NEWSOURCEPANEL_H
#define NEWSOURCEPANEL_H

#include <string>
#include <list>

#include "NavigatorPanel.h"
#include "NavigatorWidgets.h"

class Source;
class Navigator;
struct ImVec2;

///
/// Side pannel to create a new source (or replace an existing one):
/// file, image sequence, connected device, generated pattern, or bundle.
///
class NewSourcePanel : public NavigatorPanel
{
public:
    NewSourcePanel();

    typedef enum {
        SOURCE_FILE = 0,
        SOURCE_SEQUENCE,
        SOURCE_CONNECTED,
        SOURCE_GENERATED,
        SOURCE_BUNDLE
    } NewSourceType;

    typedef enum {
        MEDIA_RECENT = 0,
        MEDIA_RECORDING,
        MEDIA_FOLDER
    } MediaCreateMode;

    void Render(Navigator *navigator, const ImVec2 &iconsize);

    void setNewMedia(MediaCreateMode mode, std::string path = std::string());
    void clearNewPannel();

    // ask to rebuild the list of media of the current mode
    inline void mediaModeChanged() { new_media_mode_changed = true; }

private:
    SourcePreview new_source_preview_;
    std::list<std::string> sourceSequenceFiles;
    std::list<std::string> sourceMediaFiles;
    std::string sourceMediaFileCurrent;
    MediaCreateMode new_media_mode;
    bool new_media_mode_changed;
    int  pattern_type;
    int  generated_type;
    int  custom_type;
};

#endif /* NEWSOURCEPANEL_H */
