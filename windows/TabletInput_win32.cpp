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

#include "TabletInput.h"

// Windows: no tablet pressure support yet, the tablet input is never active

TabletInput::TabletInput()
    : data_({0.0f, false, 0.0f, 0.0f, false, false})
    , active_(false)
{
}

TabletInput::~TabletInput()
{
}

bool TabletInput::init()
{
    return false;
}

void TabletInput::pollEvents()
{
}

void TabletInput::terminate()
{
}
