/*
 * This file is part of vimix - video live mixer
 *
 * **Copyright** (C) 2019-2025 Bruno Herbelin <bruno.herbelin@gmail.com>
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

#ifdef APPLE

#include <string>
#include <utility>
#include <vector>
#import <Cocoa/Cocoa.h>

// Screens, as avfvideosrc capture-screen=true knows them: in the order of
// [NSScreen screens], which is the order of its device-index (avfvideosrc
// resolves the index with that list too), index 0 being the screen with the
// menu bar. Each is given by its display identifier and its name (e.g.
// "Built-in Retina Display").
std::vector< std::pair<unsigned long, std::string> > getListMacOSScreens()
{
    std::vector< std::pair<unsigned long, std::string> > screens;

    @autoreleasepool {
        for (NSScreen *screen in [NSScreen screens]) {
            NSNumber *number = [[screen deviceDescription] objectForKey:@"NSScreenNumber"];
            NSString *name = [screen localizedName];
            screens.emplace_back(number ? [number unsignedLongValue] : 0,
                                 name ? [name UTF8String] : "");
        }
    }

    return screens;
}

#endif
