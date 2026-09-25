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

#include "NavigatorInternal.h"
#include "IconsVimixImage.h"

std::vector< std::pair<int, int> > icons_ordering_files = { {ICON_VI_ORDER_ALPHABETICAL}, {ICON_VI_ORDER_ALPHABETICAL_INVERT}, {ICON_VI_ORDER_OLDEST_FIRST}, {ICON_VI_ORDER_NEWEST_FIRST} };
std::vector< std::string > tooltips_ordering_files = { "Alphabetical", "Invert alphabetical", "Older files first", "Recent files first" };
