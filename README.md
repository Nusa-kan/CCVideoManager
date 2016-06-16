## Synopsis

CCVideoManager is a simple Video playback manager that allows you to play videos on Cocos2dx frame work for Win32, Windows Metro and Windows Universal apps. Please note that cocos2dx already has an extension to playback videos on iOS and Android.


## Code Example

Show what the library does as concisely as possible, developers should be able to figure out **how** your project solves their problem by looking at the code example. Make sure the API you are showing off is obvious, and that your code is short and concise.

## Motivation

On our current project, we needed display cut scenes during the game play. Since we were targeting Windows market, we really needed a video playback support on these platform. 

## How to add CCVideoManager into your project
- Copy and paste the CCMediaPlayer.h/.cpp and CCVideoManager.h/.cpp files into your project directory.
- add the #include "CCMediaPlayer.h" into the the scene where you want to call the VideoManager
- Use "CCVideoManager::Instance()->PlayVideo("fileDirectory/yourvideo");"
- After the movie playback finishes itself, it will automaticly remove itself from the view.
- Make sure to destroy the VideoManager before exiting the app by calling "CCVideoManager::Instance()->DestroyInstance();"

## How to run the project
I integrated a simple demonstration code on to the default helloworld.app created by cocos2dx. 

#MoreInfo
Compiled with Visual Studio 2015
Cocos2dx engine 3.10 is used



