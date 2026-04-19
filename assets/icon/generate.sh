#!/usr/bin/env sh

magick -background none ../logo.svg -thumbnail 512x512 -gravity center -extent 512x512 icon-512.png
magick -background none ../logo.svg -thumbnail 256x256 -gravity center -extent 256x256 icon-256.png
magick -background none ../logo.svg -thumbnail 128x128 -gravity center -extent 128x128 icon-128.png
magick -background none ../logo.svg -thumbnail 64x64 -gravity center -extent 64x64 icon-64.png
magick -background none ../logo.svg -thumbnail 48x48 -gravity center -extent 48x48 icon-48.png
magick -background none ../logo.svg -thumbnail 40x40 -gravity center -extent 40x40 icon-40.png
#magick -background none ../logo.svg -thumbnail 32x32 -gravity center -extent 32x32 icon-32.png
#magick -background none ../logo.svg -thumbnail 24x24 -gravity center -extent 24x24 icon-24.png
#magick -background none ../logo.svg -thumbnail 20x20 -gravity center -extent 20x20 icon-20.png
#magick -background none ../logo.svg -thumbnail 16x16 -gravity center -extent 16x16 icon-16.png
magick icon-256.png icon-64.png icon-48.png icon-40.png icon-32.png icon-24.png icon-20.png icon-16.png icon.ico

sh ../make-archive.sh icon-512.png icon-32.png icon-24.png icon-20.png icon-16.png
