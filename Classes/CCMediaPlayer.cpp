// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.

#include "CCMediaPlayer.h"

#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32)
#include <assert.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi")
#pragma comment(lib, "mfuuid")
#pragma comment(lib, "strmiids")
#pragma comment(lib, "Mf")
#pragma comment(lib, "Mfplat")

template <class Q>
HRESULT GetEventObject(IMFMediaEvent *pEvent, Q **ppObject)
{
	*ppObject = NULL;   // zero output

	PROPVARIANT var;
	HRESULT hr = pEvent->GetValue(&var);
	if (SUCCEEDED(hr))
	{
		if (var.vt == VT_UNKNOWN)
		{
			hr = var.punkVal->QueryInterface(ppObject);
		}
		else
		{
			hr = MF_E_INVALIDTYPE;
		}
		PropVariantClear(&var);
	}
	return hr;
}

HRESULT CreateMediaSource(PCWSTR pszURL, IMFMediaSource **ppSource);

HRESULT CreatePlaybackTopology(IMFMediaSource *pSource,
	IMFPresentationDescriptor *pPD, HWND hVideoWnd, IMFTopology **ppTopology);

//  Static class method to create the CPlayer object.

HRESULT CCMediaPlayer::CreateInstance(
	HWND hVideo,                  // Video window.
	HWND hEvent,                  // Window to receive notifications.
	CCMediaPlayer **ppPlayer)           // Receives a pointer to the CPlayer object.
{
	if (ppPlayer == NULL)
	{
		return E_POINTER;
	}

	CCMediaPlayer *pPlayer = new (std::nothrow) CCMediaPlayer(hVideo, hEvent);
	if (pPlayer == NULL)
	{
		return E_OUTOFMEMORY;
	}

	HRESULT hr = pPlayer->Initialize();
	if (SUCCEEDED(hr))
	{
		*ppPlayer = pPlayer;
	}
	else
	{
		pPlayer->Release();
	}
	return hr;
}

HRESULT CCMediaPlayer::Initialize()
{
	// Start up Media Foundation platform.
	HRESULT hr = MFStartup(MF_VERSION);
	if (SUCCEEDED(hr))
	{
		m_hCloseEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (m_hCloseEvent == NULL)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
		}
	}
	return hr;
}

CCMediaPlayer::CCMediaPlayer(HWND hVideo, HWND hEvent) :
	m_pSession(NULL),
	m_pSource(NULL),
	m_pVideoDisplay(NULL),
	m_hwndVideo(hVideo),
	m_hwndEvent(hEvent),
	m_state(Closed),
	m_hCloseEvent(NULL),
	m_nRefCount(1)
{
}

CCMediaPlayer::~CCMediaPlayer()
{
	assert(m_pSession == NULL);
	Shutdown();
}

// IUnknown methods

HRESULT CCMediaPlayer::QueryInterface(REFIID riid, void** ppv)
{
	static const QITAB qit[] =
	{
		QITABENT(CCMediaPlayer, IMFAsyncCallback),
		{ 0 }
	};
	return QISearch(this, qit, riid, ppv);
}

ULONG CCMediaPlayer::AddRef()
{
	return InterlockedIncrement(&m_nRefCount);
}

ULONG CCMediaPlayer::Release()
{
	ULONG uCount = InterlockedDecrement(&m_nRefCount);
	if (uCount == 0)
	{
		delete this;
	}
	return uCount;
}

//  Open a URL for playback.
HRESULT CCMediaPlayer::OpenURL(const WCHAR *sURL)
{
	// 1. Create a new media session.
	// 2. Create the media source.
	// 3. Create the topology.
	// 4. Queue the topology [asynchronous]
	// 5. Start playback [asynchronous - does not happen in this method.]

	IMFTopology *pTopology = NULL;
	IMFPresentationDescriptor* pSourcePD = NULL;

	// Create the media session.
	HRESULT hr = CreateSession();
	if (FAILED(hr))
	{
		goto done;
	}

	// Create the media source.
	hr = CreateMediaSource(sURL, &m_pSource);
	if (FAILED(hr))
	{
		goto done;
	}

	// Create the presentation descriptor for the media source.
	hr = m_pSource->CreatePresentationDescriptor(&pSourcePD);
	if (FAILED(hr))
	{
		goto done;
	}

	// Create a partial topology.
	hr = CreatePlaybackTopology(m_pSource, pSourcePD, m_hwndVideo, &pTopology);
	if (FAILED(hr))
	{
		goto done;
	}

	// Set the topology on the media session.
	hr = m_pSession->SetTopology(0, pTopology);
	if (FAILED(hr))
	{
		goto done;
	}

	m_state = OpenPending;

	// If SetTopology succeeds, the media session will queue an 
	// MESessionTopologySet event.

done:
	if (FAILED(hr))
	{
		m_state = Closed;
	}

	SafeRelease(&pSourcePD);
	SafeRelease(&pTopology);
	return hr;
}

//  Pause playback.
HRESULT CCMediaPlayer::Pause()
{
	if (m_state != Started)
	{
		return MF_E_INVALIDREQUEST;
	}
	if (m_pSession == NULL || m_pSource == NULL)
	{
		return E_UNEXPECTED;
	}

	HRESULT hr = m_pSession->Pause();
	if (SUCCEEDED(hr))
	{
		m_state = Paused;
	}

	return hr;
}

// Stop playback.
HRESULT CCMediaPlayer::Stop()
{
	if (m_state != Started && m_state != Paused)
	{
		return MF_E_INVALIDREQUEST;
	}
	if (m_pSession == NULL)
	{
		return E_UNEXPECTED;
	}

	HRESULT hr = m_pSession->Stop();
	if (SUCCEEDED(hr))
	{
		m_state = Stopped;
	}
	return hr;
}

//  Resize the video rectangle.
//
//  Call this method if the size of the video window changes.

HRESULT CCMediaPlayer::ResizeVideo(WORD width, WORD height)
{
	if (m_pVideoDisplay)
	{
		// Set the destination rectangle.
		// Leave the default source rectangle (0,0,1,1).

		RECT rcDest = { 0, 0, width, height };

		return m_pVideoDisplay->SetVideoPosition(NULL, &rcDest);
	}
	else
	{
		return S_OK;
	}
}

//  Callback for the asynchronous BeginGetEvent method.

HRESULT CCMediaPlayer::Invoke(IMFAsyncResult *pResult)
{
	MediaEventType meType = MEUnknown;  // Event type

	IMFMediaEvent *pEvent = NULL;

	// Get the event from the event queue.
	HRESULT hr = m_pSession->EndGetEvent(pResult, &pEvent);
	if (FAILED(hr))
	{
		goto done;
	}

	// Get the event type. 
	hr = pEvent->GetType(&meType);
	if (FAILED(hr))
	{
		goto done;
	}

	if (meType == MESessionClosed)
	{
		// The session was closed. 
		// The application is waiting on the m_hCloseEvent event handle. 
		SetEvent(m_hCloseEvent);
	}
	else
	{
		// For all other events, get the next event in the queue.
		hr = m_pSession->BeginGetEvent(this, NULL);
		if (FAILED(hr))
		{
			goto done;
		}
	}

	// Check the application state. 

	// If a call to IMFMediaSession::Close is pending, it means the 
	// application is waiting on the m_hCloseEvent event and
	// the application's message loop is blocked. 

	// Otherwise, post a private window message to the application. 

	if (m_state != Closing && m_state != finishedPlayback)
	{
		// Leave a reference count on the event.
		pEvent->AddRef();
		PostMessage(m_hwndEvent, WM_APP_PLAYER_EVENT,
			(WPARAM)pEvent, (LPARAM)meType);
	}
	


done:
	SafeRelease(&pEvent);
	return S_OK;
}

HRESULT CCMediaPlayer::HandleEvent(UINT_PTR pEventPtr)
{
	HRESULT hrStatus = S_OK;
	MediaEventType meType = MEUnknown;

	IMFMediaEvent *pEvent = (IMFMediaEvent*)pEventPtr;

	if (pEvent == NULL)
	{
		return E_POINTER;
	}

	// Get the event type.
	HRESULT hr = pEvent->GetType(&meType);
	if (FAILED(hr))
	{
		goto done;
	}

	// Get the event status. If the operation that triggered the event 
	// did not succeed, the status is a failure code.
	hr = pEvent->GetStatus(&hrStatus);

	// Check if the async operation succeeded.
	if (SUCCEEDED(hr) && FAILED(hrStatus))
	{
		hr = hrStatus;
	}
	if (FAILED(hr))
	{
		goto done;
	}

	switch (meType)
	{
	case MESessionTopologyStatus:
		hr = OnTopologyStatus(pEvent);
		break;

	case MEEndOfPresentation:
		hr = OnPresentationEnded(pEvent);
		break;

	case MENewPresentation:
		hr = OnNewPresentation(pEvent);
		break;

	default:
		hr = OnSessionEvent(pEvent, meType);
		break;
	}

done:
	SafeRelease(&pEvent);
	return hr;
}

//  Release all resources held by this object.
HRESULT CCMediaPlayer::Shutdown()
{
	// Close the session
	HRESULT hr = CloseSession();

	// Shutdown the Media Foundation platform
	MFShutdown();

	if (m_hCloseEvent)
	{
		CloseHandle(m_hCloseEvent);
		m_hCloseEvent = NULL;
	}

	return hr;
}

/// Protected methods

HRESULT CCMediaPlayer::OnTopologyStatus(IMFMediaEvent *pEvent)
{
	UINT32 status;

	HRESULT hr = pEvent->GetUINT32(MF_EVENT_TOPOLOGY_STATUS, &status);
	if (SUCCEEDED(hr) && (status == MF_TOPOSTATUS_READY))
	{
		SafeRelease(&m_pVideoDisplay);

		// Get the IMFVideoDisplayControl interface from EVR. This call is
		// expected to fail if the media file does not have a video stream.

		(void)MFGetService(m_pSession, MR_VIDEO_RENDER_SERVICE,
			IID_PPV_ARGS(&m_pVideoDisplay));

		hr = StartPlayback();
	}
	return hr;
}


//  Handler for MEEndOfPresentation event.
HRESULT CCMediaPlayer::OnPresentationEnded(IMFMediaEvent *pEvent)
{
	// The session puts itself into the stopped state automatically.
	CloseSession();
	m_state = finishedPlayback;

	MediaEventType meType = MEUnknown;  // Event type

	pEvent->AddRef();
	PostMessage(m_hwndEvent, WM_APP_PLAYER_FINISHED_EVENT,
		(WPARAM)pEvent, (LPARAM)meType);

	PostMessage(m_hwndEvent, WM_CLOSE,
		(WPARAM)pEvent, (LPARAM)meType);

	return S_OK;
}

//  Handler for MENewPresentation event.
//
//  This event is sent if the media source has a new presentation, which 
//  requires a new topology. 

HRESULT CCMediaPlayer::OnNewPresentation(IMFMediaEvent *pEvent)
{
	IMFPresentationDescriptor *pPD = NULL;
	IMFTopology *pTopology = NULL;

	// Get the presentation descriptor from the event.
	HRESULT hr = GetEventObject(pEvent, &pPD);
	if (FAILED(hr))
	{
		goto done;
	}

	// Create a partial topology.
	hr = CreatePlaybackTopology(m_pSource, pPD, m_hwndVideo, &pTopology);
	if (FAILED(hr))
	{
		goto done;
	}

	// Set the topology on the media session.
	hr = m_pSession->SetTopology(0, pTopology);
	if (FAILED(hr))
	{
		goto done;
	}

	m_state = OpenPending;

done:
	SafeRelease(&pTopology);
	SafeRelease(&pPD);
	return S_OK;
}

//  Create a new instance of the media session.
HRESULT CCMediaPlayer::CreateSession()
{
	// Close the old session, if any.
	HRESULT hr = CloseSession();
	if (FAILED(hr))
	{
		goto done;
	}

	assert(m_state == Closed);

	// Create the media session.
	hr = MFCreateMediaSession(NULL, &m_pSession);
	if (FAILED(hr))
	{
		goto done;
	}

	// Start pulling events from the media session
	hr = m_pSession->BeginGetEvent((IMFAsyncCallback*)this, NULL);
	if (FAILED(hr))
	{
		goto done;
	}

	m_state = Ready;

done:
	return hr;
}

//  Close the media session. 
HRESULT CCMediaPlayer::CloseSession()
{
	//  The IMFMediaSession::Close method is asynchronous, but the 
	//  CPlayer::CloseSession method waits on the MESessionClosed event.
	//  
	//  MESessionClosed is guaranteed to be the last event that the 
	//  media session fires.

	HRESULT hr = S_OK;

	SafeRelease(&m_pVideoDisplay);

	// First close the media session.
	if (m_pSession)
	{
		DWORD dwWaitResult = 0;

		m_state = Closing;

		hr = m_pSession->Close();
		// Wait for the close operation to complete
		if (SUCCEEDED(hr))
		{
			dwWaitResult = WaitForSingleObject(m_hCloseEvent, 5000);
			if (dwWaitResult == WAIT_TIMEOUT)
			{
				assert(FALSE);
			}
			// Now there will be no more events from this session.
		}
	}

	// Complete shutdown operations.
	if (SUCCEEDED(hr))
	{
		// Shut down the media source. (Synchronous operation, no events.)
		if (m_pSource)
		{
			(void)m_pSource->Shutdown();
		}
		// Shut down the media session. (Synchronous operation, no events.)
		if (m_pSession)
		{
			(void)m_pSession->Shutdown();
		}
	}

	SafeRelease(&m_pSource);
	SafeRelease(&m_pSession);
	m_state = Closed;
	return hr;
}

//  Start playback from the current position. 
HRESULT CCMediaPlayer::StartPlayback()
{
	assert(m_pSession != NULL);

	PROPVARIANT varStart;
	PropVariantInit(&varStart);

	HRESULT hr = m_pSession->Start(&GUID_NULL, &varStart);
	if (SUCCEEDED(hr))
	{
		// Note: Start is an asynchronous operation. However, we
		// can treat our state as being already started. If Start
		// fails later, we'll get an MESessionStarted event with
		// an error code, and we will update our state then.
		m_state = Started;
	}
	PropVariantClear(&varStart);
	return hr;
}

//  Start playback from paused or stopped.
HRESULT CCMediaPlayer::Play()
{
	if (m_state != Paused && m_state != Stopped)
	{
		return MF_E_INVALIDREQUEST;
	}
	if (m_pSession == NULL || m_pSource == NULL)
	{
		return E_UNEXPECTED;
	}
	return StartPlayback();
}


//  Create a media source from a URL.
HRESULT CreateMediaSource(PCWSTR sURL, IMFMediaSource **ppSource)
{
	MF_OBJECT_TYPE ObjectType = MF_OBJECT_INVALID;

	IMFSourceResolver* pSourceResolver = NULL;
	IUnknown* pSource = NULL;

	// Create the source resolver.
	HRESULT hr = MFCreateSourceResolver(&pSourceResolver);
	if (FAILED(hr))
	{
		goto done;
	}

	// Use the source resolver to create the media source.

	// Note: For simplicity this sample uses the synchronous method to create 
	// the media source. However, creating a media source can take a noticeable
	// amount of time, especially for a network source. For a more responsive 
	// UI, use the asynchronous BeginCreateObjectFromURL method.

	hr = pSourceResolver->CreateObjectFromURL(
		sURL,                       // URL of the source.
		MF_RESOLUTION_MEDIASOURCE,  // Create a source object.
		NULL,                       // Optional property store.
		&ObjectType,        // Receives the created object type. 
		&pSource            // Receives a pointer to the media source.
		);
	if (FAILED(hr))
	{
		goto done;
	}

	// Get the IMFMediaSource interface from the media source.
	hr = pSource->QueryInterface(IID_PPV_ARGS(ppSource));

done:
	SafeRelease(&pSourceResolver);
	SafeRelease(&pSource);
	return hr;
}

//  Create an activation object for a renderer, based on the stream media type.

HRESULT CreateMediaSinkActivate(
	IMFStreamDescriptor *pSourceSD,     // Pointer to the stream descriptor.
	HWND hVideoWindow,                  // Handle to the video clipping window.
	IMFActivate **ppActivate
	)
{
	IMFMediaTypeHandler *pHandler = NULL;
	IMFActivate *pActivate = NULL;

	// Get the media type handler for the stream.
	HRESULT hr = pSourceSD->GetMediaTypeHandler(&pHandler);
	if (FAILED(hr))
	{
		goto done;
	}

	// Get the major media type.
	GUID guidMajorType;
	hr = pHandler->GetMajorType(&guidMajorType);
	if (FAILED(hr))
	{
		goto done;
	}

	// Create an IMFActivate object for the renderer, based on the media type.
	if (MFMediaType_Audio == guidMajorType)
	{
		// Create the audio renderer.
		hr = MFCreateAudioRendererActivate(&pActivate);
	}
	else if (MFMediaType_Video == guidMajorType)
	{
		// Create the video renderer.
		hr = MFCreateVideoRendererActivate(hVideoWindow, &pActivate);
	}
	else
	{
		// Unknown stream type. 
		hr = E_FAIL;
		// Optionally, you could deselect this stream instead of failing.
	}
	if (FAILED(hr))
	{
		goto done;
	}

	// Return IMFActivate pointer to caller.
	*ppActivate = pActivate;
	(*ppActivate)->AddRef();

done:
	SafeRelease(&pHandler);
	SafeRelease(&pActivate);
	return hr;
}

// Add a source node to a topology.
HRESULT AddSourceNode(
	IMFTopology *pTopology,           // Topology.
	IMFMediaSource *pSource,          // Media source.
	IMFPresentationDescriptor *pPD,   // Presentation descriptor.
	IMFStreamDescriptor *pSD,         // Stream descriptor.
	IMFTopologyNode **ppNode)         // Receives the node pointer.
{
	IMFTopologyNode *pNode = NULL;

	// Create the node.
	HRESULT hr = MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &pNode);
	if (FAILED(hr))
	{
		goto done;
	}

	// Set the attributes.
	hr = pNode->SetUnknown(MF_TOPONODE_SOURCE, pSource);
	if (FAILED(hr))
	{
		goto done;
	}

	hr = pNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, pPD);
	if (FAILED(hr))
	{
		goto done;
	}

	hr = pNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, pSD);
	if (FAILED(hr))
	{
		goto done;
	}

	// Add the node to the topology.
	hr = pTopology->AddNode(pNode);
	if (FAILED(hr))
	{
		goto done;
	}

	// Return the pointer to the caller.
	*ppNode = pNode;
	(*ppNode)->AddRef();

done:
	SafeRelease(&pNode);
	return hr;
}

// Add an output node to a topology.
HRESULT AddOutputNode(
	IMFTopology *pTopology,     // Topology.
	IMFActivate *pActivate,     // Media sink activation object.
	DWORD dwId,                 // Identifier of the stream sink.
	IMFTopologyNode **ppNode)   // Receives the node pointer.
{
	IMFTopologyNode *pNode = NULL;

	// Create the node.
	HRESULT hr = MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &pNode);
	if (FAILED(hr))
	{
		goto done;
	}

	// Set the object pointer.
	hr = pNode->SetObject(pActivate);
	if (FAILED(hr))
	{
		goto done;
	}

	// Set the stream sink ID attribute.
	hr = pNode->SetUINT32(MF_TOPONODE_STREAMID, dwId);
	if (FAILED(hr))
	{
		goto done;
	}

	hr = pNode->SetUINT32(MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, FALSE);
	if (FAILED(hr))
	{
		goto done;
	}

	// Add the node to the topology.
	hr = pTopology->AddNode(pNode);
	if (FAILED(hr))
	{
		goto done;
	}

	// Return the pointer to the caller.
	*ppNode = pNode;
	(*ppNode)->AddRef();

done:
	SafeRelease(&pNode);
	return hr;
}
//</SnippetPlayer.cpp>

//  Add a topology branch for one stream.
//
//  For each stream, this function does the following:
//
//    1. Creates a source node associated with the stream. 
//    2. Creates an output node for the renderer. 
//    3. Connects the two nodes.
//
//  The media session will add any decoders that are needed.

HRESULT AddBranchToPartialTopology(
	IMFTopology *pTopology,         // Topology.
	IMFMediaSource *pSource,        // Media source.
	IMFPresentationDescriptor *pPD, // Presentation descriptor.
	DWORD iStream,                  // Stream index.
	HWND hVideoWnd)                 // Window for video playback.
{
	IMFStreamDescriptor *pSD = NULL;
	IMFActivate         *pSinkActivate = NULL;
	IMFTopologyNode     *pSourceNode = NULL;
	IMFTopologyNode     *pOutputNode = NULL;

	BOOL fSelected = FALSE;

	HRESULT hr = pPD->GetStreamDescriptorByIndex(iStream, &fSelected, &pSD);
	if (FAILED(hr))
	{
		goto done;
	}

	if (fSelected)
	{
		// Create the media sink activation object.
		hr = CreateMediaSinkActivate(pSD, hVideoWnd, &pSinkActivate);
		if (FAILED(hr))
		{
			goto done;
		}

		// Add a source node for this stream.
		hr = AddSourceNode(pTopology, pSource, pPD, pSD, &pSourceNode);
		if (FAILED(hr))
		{
			goto done;
		}

		// Create the output node for the renderer.
		hr = AddOutputNode(pTopology, pSinkActivate, 0, &pOutputNode);
		if (FAILED(hr))
		{
			goto done;
		}

		// Connect the source node to the output node.
		hr = pSourceNode->ConnectOutput(0, pOutputNode, 0);
	}
	// else: If not selected, don't add the branch. 

done:
	SafeRelease(&pSD);
	SafeRelease(&pSinkActivate);
	SafeRelease(&pSourceNode);
	SafeRelease(&pOutputNode);
	return hr;
}

//  Create a playback topology from a media source.
HRESULT CreatePlaybackTopology(
	IMFMediaSource *pSource,          // Media source.
	IMFPresentationDescriptor *pPD,   // Presentation descriptor.
	HWND hVideoWnd,                   // Video window.
	IMFTopology **ppTopology)         // Receives a pointer to the topology.
{
	IMFTopology *pTopology = NULL;
	DWORD cSourceStreams = 0;

	// Create a new topology.
	HRESULT hr = MFCreateTopology(&pTopology);
	if (FAILED(hr))
	{
		goto done;
	}




	// Get the number of streams in the media source.
	hr = pPD->GetStreamDescriptorCount(&cSourceStreams);
	if (FAILED(hr))
	{
		goto done;
	}

	// For each stream, create the topology nodes and add them to the topology.
	for (DWORD i = 0; i < cSourceStreams; i++)
	{
		hr = AddBranchToPartialTopology(pTopology, pSource, pPD, i, hVideoWnd);
		if (FAILED(hr))
		{
			goto done;
		}
	}

	// Return the IMFTopology pointer to the caller.
	*ppTopology = pTopology;
	(*ppTopology)->AddRef();

done:
	SafeRelease(&pTopology);
	return hr;
}

#elif (CC_TARGET_PLATFORM == CC_PLATFORM_WINRT)
using namespace CocosAppWinRT;
using namespace Windows::UI::Xaml::Controls;

CCMediaPlayerClient::CCMediaPlayerClient(Platform::String^ fileName)
{
	PlayMedia(fileName);
};


void CCMediaPlayerClient::PlayMedia(Platform::String^ fileName)
{
	Platform::String ^ msAppResourceDirectory = "ms-appx:///Assets/Resources/";
	Platform::String ^ fullPath = msAppResourceDirectory + fileName;

	Platform::String^ url = fullPath;



	auto dispatcher = cocos2d::GLViewImpl::sharedOpenGLView()->getDispatcher();

	if (dispatcher->GetType() && cocos2d::GLViewImpl::sharedOpenGLView()->getPanel()->GetType())
	{

		dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([this, url]()
		{

			auto mediaElement = ref new Windows::UI::Xaml::Controls::MediaElement();
			Windows::Foundation::Uri^ uri = ref new Windows::Foundation::Uri(url);
			mediaElement->Source = uri;

			mediaElement->AutoPlay = true;
			mediaElement->IsFullWindow = true;
			mediaElement->Name = "Intro";
			mediaElement->IsMuted = false;
			cocos2d::GLViewImpl::sharedOpenGLView()->getPanel()->Children->Append(mediaElement);

			mediaElement->MediaEnded += ref new Windows::UI::Xaml::RoutedEventHandler(this, &CCMediaPlayerClient::OnMediaEnded);

			uri = nullptr;

		}));

	}
}



void CCMediaPlayerClient::OnMediaEnded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	
	cocos2d::GLViewImpl::sharedOpenGLView()->getPanel()->Children->Clear();
}


#endif /*Platform specific*/