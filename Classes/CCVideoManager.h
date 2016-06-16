#ifndef _CVIDEO_MANAGER_
#define _CVIDEO_MANAGER_

#include "CCMediaPlayer.h"

#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)


#define IDD_MFPLAYBACK_DIALOG           102
#define IDM_EXIT                        105
#define IDC_MFPLAYBACK                  109
#define IDD_OPENURL                     129
#define IDC_STATIC                      -1


LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);


class CCVideoManager
{

private:

	std::wstring _videoFilePath;
	CCMediaPlayer * _pPlayer = NULL; //player object
	static CCVideoManager * m_instance; //the singleton instance we will use for this class.
	CCVideoManager();//default constructor is private to prevent access
	CCVideoManager(CCVideoManager const &); // copy constructor is also private.
	CCVideoManager & operator = (const CCVideoManager&); // so as the assignment operator is also private

public:

	static void DestroyInstance();
	
	static CCVideoManager * Instance(); // this is what we will be calling when create a CVideoManager object.
	void PlayVideo(std::string path);
	~CCVideoManager();
	
	// Message handlers
	LRESULT OnCreateWindow(HWND hwnd); //Handler for WM_CREATE message.
	void OnFileOpen(HWND hwnd);
	void OnPlayerEvent(HWND hwnd, WPARAM pUnkPtr);  // Handler for Media Session events.
	void OnPlayerFinishedEvent(HWND hwnd, WPARAM pUnkPtr);  // Handler for Media Session events after finishing playing.

	
};

#elif (CC_TARGET_PLATFORM == CC_PLATFORM_WINRT)


class CCVideoManager
{

private:

	std::wstring _videoFilePath;

	CocosAppWinRT::CCMediaPlayerClient^ _pPlayer; //player object
	static CCVideoManager * m_instance; //the singleton instance we will use for this class.
	CCVideoManager();//default constructor is private to prevent access
	CCVideoManager(CCVideoManager const &); // copy constructor is also private.
	CCVideoManager & operator = (const CCVideoManager&); // so as the assignment operator is also private


public:
	static void DestroyInstance();

	static CCVideoManager * Instance(); // this is what we will be calling when create a CVideoManager object.
	void PlayVideo(std::string path);
	~CCVideoManager();
};

#endif /*Platform specific*/
#endif /*_CVIDEO_MANAGER*/
