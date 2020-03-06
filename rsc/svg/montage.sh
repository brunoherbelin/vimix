#!/bin/sh
montage -channel rgba -background "rgba(0,0,0,0)" *.svg -tile 20x20 -geometry 128x128+2+2 icons.png
