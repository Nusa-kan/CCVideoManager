## Synopsis

CCVideoManager is a simple Video playback manager that allows you to play videos on Cocos2dx frame work for Win32, Windows Metro and Windows Universal apps. Please note that cocos2dx already has an extension to playback videos on iOS and Android.

## Motivation

On our current project we needed display cut scenes during the game play. Since we were targeting Windows market, we really needed a video playback support on these platform. Sadly there wasn't a solution that you can use on cocos2dx.

## How to add CCVideoManager into your project
- Copy and paste the CCMediaPlayer.h/.cpp and CCVideoManager.h/.cpp files into your project directory.
- add the #include "CCMediaPlayer.h" into the the scene where you want to call the VideoManager
- Use "CCVideoManager::Instance()->PlayVideo("fileDirectory/yourvideo");"
- After the movie playback finishes itself, it will automaticly remove itself from the view.
- Make sure to destroy the VideoManager before exiting the app by calling "CCVideoManager::Instance()->DestroyInstance();"

#MoreInfo
This repository contains a demo of the CCVideoManager. You can download the project and run as how you run the helloworld.app demo
created by cocos2dx.

The demo is Compiled with Visual Studio 2015
and Cocos2dx v3.10 is used.



