Open Source Sanguosha
==========

| Homepage:      | https://qsanguosha.org                        |
|----------------|-----------------------------------------------|
| API reference: | http://gaodayihao.github.com/QSanguosha/api   |
| Documentation: | https://github.com/gaodayihao/QSanguosha/wiki (Chinese) |

Introduction
----------

Sanguosha is both a popular board game and online game,
this project try to clone the Sanguosha online version.
The whole project is written in C++, 
using Qt's graphics view framework as the game engine.
I've tried many other open source game engines, 
such as SDL, HGE, Clanlib and others, 
but many of them lack some important features. 
Although Qt is an application framework instead of a game engine, 
its graphics view framework is suitable for my game developing. By Moligaloo

Features
----------

1. Framework
    * Open source with Qt graphics view framework
    * Use FMOD as sound engine
    * Use Freetype in Font Rendering
    * Use Lua as AI and extension script

2. Operation experience
    * Full package (include all yoka extension package)
    * Keyboard shortcut
    * Double-click to use cards
    * Cards sorting (by card type and card suit)
    * Multilayer display when cards are more than an upperlimit

3. Extensible
    * Some MODs are available based on this game
    * Lua Packages are supported in this game
