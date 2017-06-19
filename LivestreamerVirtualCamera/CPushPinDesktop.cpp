#include "stdafx.h"

#include "PushSource.h"
#include "PushGuids.h"
#include "Livestreamer.h"

#define MIN(a,b)  ((a) < (b) ? (a) : (b))  // danger! can evaluate "a" twice.

  bool launchDebugger()
  {
	  // Get System directory, typically c:\windows\system32
	  std::wstring systemDir(MAX_PATH + 1, '\0');
	  UINT nChars = GetSystemDirectoryW(&systemDir[0], systemDir.length());
	  if (nChars == 0) return false; // failed to get system directory
	  systemDir.resize(nChars);

	  // Get process ID and create the command line
	  DWORD pid = GetCurrentProcessId();
	  std::wostringstream s;
	  s << systemDir << L"\\vsjitdebugger.exe -p " << pid;
	  std::wstring cmdLine = s.str();

	  // Start debugger process
	  STARTUPINFOW si;
	  ZeroMemory(&si, sizeof(si));
	  si.cb = sizeof(si);

	  PROCESS_INFORMATION pi;
	  ZeroMemory(&pi, sizeof(pi));

	  if (!CreateProcessW(NULL, &cmdLine[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) return false;

	  // Close debugger process handles to eliminate resource leak
	  CloseHandle(pi.hThread);
	  CloseHandle(pi.hProcess);

	  // Wait for the debugger to attach
	  while (!IsDebuggerPresent()) Sleep(100);

	  // Stop execution so the debugger can take over
	  DebugBreak();
	  return true;
  }

// the default child constructor...
CPushPinDesktop::CPushPinDesktop(HRESULT *phr, CSource *pFilter)
        : CSourceStream(NAME("Push Source CPushPinDesktop child/pin"), phr, pFilter, L"Capture"),
		pOldData(NULL),
		hRawBitmap(NULL),
		previousFrameEndTime(0),
      livestreamer()
{
	//launchDebugger();
	hScrDc = GetDC(NULL);
	
	int config_width = 900; // read_config_setting(TEXT("capture_width"), 0, false);
	int config_height = 600; // read_config_setting(TEXT("capture_height"), 0, false);

	// default 30 fps...hmm...
	int config_max_fps = 30;

	// m_rtFrameLength is also re-negotiated later...
  	m_rtFrameLength = UNITS / config_max_fps; 
}

HRESULT CPushPinDesktop::FillBuffer(IMediaSample *pSample)
{
	BYTE *pData;

    CheckPointer(pSample, E_POINTER);

	// Access the sample's data buffer
    pSample->GetPointer(&pData);

    VIDEOINFOHEADER *pVih = (VIDEOINFOHEADER*) m_mt.pbFormat;

    CopyScreenToDataBlock(hScrDc, pData, (BITMAPINFO *) &(pVih->bmiHeader), pSample);

	// Set the timestamps that will govern playback frame rate.
	// If this file is getting written out as an AVI,
	// then you'll also need to configure the AVI Mux filter to 
	// set the Average Time Per Frame for the AVI Header.
	// The current time is the sample's start
	REFERENCE_TIME rtStart = m_iFrameNumber * m_rtFrameLength;
	REFERENCE_TIME rtStop = rtStart + m_rtFrameLength;

	pSample->SetTime(&rtStart, &rtStop);
	m_iFrameNumber++;

	// Set TRUE on every sample for uncompressed frames
	pSample->SetSyncPoint(TRUE);

    return S_OK;
}

CPushPinDesktop::~CPushPinDesktop()
{   
	// They *should* call this...VLC does at least, correctly.

    // Release the device context stuff
	::ReleaseDC(NULL, hScrDc);
    ::DeleteDC(hScrDc);

    if (hRawBitmap)
      DeleteObject(hRawBitmap); // don't need those bytes anymore -- I think we are supposed to delete just this and not hOldBitmap

    if(pOldData) {
		free(pOldData);
		pOldData = NULL;
	}
}

void CPushPinDesktop::CopyScreenToDataBlock(HDC hScrDC, BYTE *pData, BITMAPINFO *pHeader, IMediaSample *pSample)
{
    HDC hMemDC;
    int iFinalStretchHeight = livestreamer.GetHeight();
    int iFinalStretchWidth = livestreamer.GetWidth();
	
    hMemDC = CreateCompatibleDC(hScrDC); //  0.02ms Anything else to reuse, this one's pretty fast...?

	BITMAPINFO tweakableHeader;
	memcpy(&tweakableHeader, pHeader, sizeof(BITMAPINFO));
	
   auto x = livestreamer.GetNextFrame();
	doDIBits(hScrDC, livestreamer.GetNextFrame(), iFinalStretchHeight, pData, &tweakableHeader);

   // clean up
    DeleteDC(hMemDC);
}

void CPushPinDesktop::doDIBits(HDC hScrDC, HBITMAP hRawBitmap, int nHeightScanLines, BYTE *pData, BITMAPINFO *pHeader) {
    GetDIBits(hScrDC, hRawBitmap, 0, nHeightScanLines, pData, pHeader, DIB_RGB_COLORS);
	
}


//
// DecideBufferSize
//
// This will always be called after the format has been sucessfully
// negotiated (this is negotiatebuffersize). So we have a look at m_mt to see what size image we agreed.
// Then we can ask for buffers of the correct size to contain them.
//
HRESULT CPushPinDesktop::DecideBufferSize(IMemAllocator *pAlloc,
                                      ALLOCATOR_PROPERTIES *pProperties)
{
    CheckPointer(pAlloc,E_POINTER);
    CheckPointer(pProperties,E_POINTER);

    CAutoLock cAutoLock(m_pFilter->pStateLock());
    HRESULT hr = NOERROR;

    VIDEOINFO *pvi = (VIDEOINFO *) m_mt.Format();
	BITMAPINFOHEADER header = pvi->bmiHeader;
	
	int bytesPerLine;
	// there may be a windows method that would do this for us...GetBitmapSize(&header); but might be too small for VLC? LODO try it :)
	// some pasted code...
	int bytesPerPixel = (header.biBitCount/8);

    bytesPerLine = header.biWidth * bytesPerPixel;
    /* round up to a dword boundary for stride */
    if (bytesPerLine & 0x0003) 
    {
      bytesPerLine |= 0x0003;
      ++bytesPerLine;
    }

	// NB that we are adding in space for a final "pixel array" (http://en.wikipedia.org/wiki/BMP_file_format#DIB_Header_.28Bitmap_Information_Header.29) even though we typically don't need it, this seems to fix the segfaults
	// maybe somehow down the line some VLC thing thinks it might be there...weirder than weird.. LODO debug it LOL.
	int bitmapSize = 14 + header.biSize + (long)(bytesPerLine)*(header.biHeight) + bytesPerLine*header.biHeight;
	pProperties->cbBuffer = bitmapSize;

    pProperties->cBuffers = 1; // 2 here doesn't seem to help the crashes...

    // Ask the allocator to reserve us some sample memory. NOTE: the function
    // can succeed (return NOERROR) but still not have allocated the
    // memory that we requested, so we must check we got whatever we wanted.
    ALLOCATOR_PROPERTIES Actual;
    hr = pAlloc->SetProperties(pProperties,&Actual);
    if(FAILED(hr))
    {
        return hr;
    }

    // Is this allocator unsuitable?
    if(Actual.cbBuffer < pProperties->cbBuffer)
    {
        return E_FAIL;
    }
	
	if(pOldData) {
		free(pOldData);
		pOldData = NULL;
	}
    pOldData = (BYTE *) malloc(max(pProperties->cbBuffer*pProperties->cBuffers, bitmapSize)); // we convert from a 32 bit to i420, so need more space, hence max
    memset(pOldData, 0, pProperties->cbBuffer*pProperties->cBuffers); // reset it just in case :P	
	
    // create a bitmap compatible with the screen DC
	if(hRawBitmap)
		DeleteObject (hRawBitmap); // delete the old one in case it exists...
	hRawBitmap = CreateCompatibleBitmap(hScrDc, livestreamer.GetWidth(), livestreamer.GetHeight());
	
    return NOERROR;
} // DecideBufferSize


HRESULT CPushPinDesktop::OnThreadCreate() {
	return S_OK;
}
