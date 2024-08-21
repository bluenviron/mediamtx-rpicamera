#!/bin/sh -e

wget -O text_font.ttf https://github.com/IBM/plex/raw/v6.4.2/IBM-Plex-Mono/fonts/complete/ttf/IBMPlexMono-Medium.ttf

xxd --include text_font.ttf > text_font.h
