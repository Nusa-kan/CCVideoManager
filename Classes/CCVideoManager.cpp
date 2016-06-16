#include "CCVideoManager.h"
#include "cocos2d.h"

USING_NS_CC;
//-------------------------------CCVideoManager----------------------------------------------//
CCVideoManager * CCVideoManager::m_instance = NULL;

CCVideoManager * CCVideoManager::Instance()
{
	//if instance wasnt created yet we call the constructor.
	if (m_instance == NULL)
	{
		m_instance = new CCVideoManager();
	}

	return m_instance;
}


CCVideoManager::CCVideoManager() {}

void CCVideoManager::DestroyInstance()
{
	
	m_instance = nullptr;
}

CCVideoManager::~CCVideoManager()
{
	
	_pPlayer = nullptr;
}


#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
		return CCVideoManager::Instance()->OnCreateWindow(hwnd);

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDM_EXIT:
			DestroyWindow(hwnd);
			break;
		default:
			return DefWindowProc(hwnd, message, wParam, lParam);
		}
		break;

	

	case WM_ERASEBKGND:
		// Suppress window erasing, to reduce flickering while the video is playing.
		return 1;

	case WM_DESTROY:
		//PostQuitMessage(0);
		break;

	

	case WM_APP_PLAYER_EVENT:
		CCVideoManager::Instance()->OnPlayerEvent(hwnd, wParam);
		break;

	case WM_APP_PLAYER_FINISHED_EVENT:
		CCVideoManager::Instance()->OnPlayerFinishedEvent(hwnd, wParam);
		break;

	default:
		return DefWindowProc(hwnd, message, wParam, lParam);
	}
	return 0;
}





void CCVideoManager::PlayVideo(std::string path)
{
	std::wstring wsTmp(path.begin(), path.end()); //Warning, the conversation between wstring and string only happens correctly if all the characters are single byte (ASCII or ISO-8859-1)
	_videoFilePath = wsTmp;

	PCWSTR szTitle = L"BasicPlayback";
	PCWSTR szWindowClass = L"MFBASICPLAYBACK";

	HWND  mainWindow = CCDirector::getInstance()->getOpenGLView()->getWin32Window();
	
	HWND hwnd;
	WNDCLASSEX wcex =  {0};
	HINSTANCE hInstance = (HINSTANCE)GetWindowLong(mainWindow, GWL_HINSTANCE);

	//register the windows class
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.lpfnWndProc = WndProc;
	wcex.hInstance = hInstance;
	wcex.lpszClassName = szWindowClass;
	
	if (RegisterClassEx(&wcex) == 0)
	{
		return;
	}
	
	RECT rect;
	GetClientRect(mainWindow, &rect);
	float width = rect.right - rect.left;
	float height = rect.bottom - rect.top;
	// Create the application window.
	hwnd = CreateWindow(szWindowClass,
		szTitle, 
		WS_CHILD | WS_VISIBLE,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		width,
		height,
		mainWindow,
		NULL, 
		hInstance,
		NULL);

	if (hwnd == 0)
	{
		
		return;
	}


	ShowWindow(hwnd, SW_SHOWNORMAL);
	UpdateWindow(hwnd);

	//return TRUE;

}



//-------------------------------CCVideoManager Handlers------------------------------------//
LRESULT CCVideoManager::OnCreateWindow(HWND hwnd)
{
	// Initialize the player object.
	HRESULT hr = CCMediaPlayer::CreateInstance(hwnd, hwnd, &_pPlayer);
	if (SUCCEEDED(hr))
	{
		OnFileOpen(hwnd);
		return 0;   // Success.
	}

}
void CCVideoManager::OnFileOpen(HWND hwnd)
{

	
	const wchar_t* szfile = _videoFilePath.c_str();
	// Display the file name to the user.
	HRESULT  hr = _pPlayer->OpenURL(szfile);
	
}

void CCVideoManager::OnPlayerEvent(HWND hwnd, WPARAM pUnkPtr)
{
	
	HRESULT hr = _pPlayer->HandleEvent(pUnkPtr);
}

void CCVideoManager::OnPlayerFinishedEvent(HWND hwnd, WPARAM pUnkPtr)
{
	if (_pPlayer)
	{
		_pPlayer->Shutdown();
		SafeRelease(&_pPlayer);
	}
}


#elif (CC_TARGET_PLATFORM == CC_PLATFORM_WINRT)

void CCVideoManager::PlayVideo(std::string path)
{
	std::wstring wsTmp(path.begin(), path.end()); //Warning, the conversation between wstring and string only happens correctly if all the characters are single byte (ASCII or ISO-8859-1)
	Platform::String^ p_string = ref new Platform::String(wsTmp.c_str());
	_pPlayer = ref new CocosAppWinRT::CCMediaPlayerClient(p_string);
}
#endif /*Platform specific*/