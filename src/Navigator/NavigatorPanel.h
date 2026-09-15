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
#ifndef NAVIGATORPANEL_H
#define NAVIGATORPANEL_H

///
/// Base class of the side pannels of the Navigator.
///
/// Holds the geometry, which the Navigator computes once per frame and gives
/// to every pannel, and the creation of the pannel window, identical for all.
/// NB: this is not a WorkspaceWindow; the pannels are not independent windows
/// but alternative contents of the Navigator side pannel.
///
class NavigatorPanel
{
public:
    NavigatorPanel();
    virtual ~NavigatorPanel() {}

    // geometry of the pannel, given by the Navigator at every frame
    void setGeometry(float bar_width, float pannel_width, float height, float padding, float alpha);

protected:
    // begin the side pannel window; the name shall be a literal, unchanged,
    // and ImGui::End() shall be called in the same function.
    bool beginPannelWindow(const char *name);

    float width_;
    float pannel_width_;
    float height_;
    float padding_width_;
    float pannel_alpha_;
};

#endif /* NAVIGATORPANEL_H */
