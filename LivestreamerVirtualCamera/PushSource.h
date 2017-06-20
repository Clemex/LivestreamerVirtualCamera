//------------------------------------------------------------------------------
// File: PushSource.H
//
// Desc: DirectShow sample code - In-memory push mode source filter
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#include "Livestreamer.h"

// Filter name strings
#define g_wszPushDesktop    L"PushSource Desktop Filter"

class CPushPinDesktop : public CSourceStream, public IAMStreamConfig, public IKsPropertySet
{

protected:
	int m_iFrameNumber;
   REFERENCE_TIME m_rtFrameLength; // also used to get the fps
	REFERENCE_TIME previousFrameEndTime;
   Livestreamer* livestreamer;
	HDC hScrDc;

	int m_millisToSleepBeforePollForChanges;
	void CopyImageToDataBlock(HDC hScrDc, BYTE *pData, BITMAPINFO *pHeader, IMediaSample *pSample);

public:

	//CSourceStream
	HRESULT OnThreadCreate(void);

    //////////////////////////////////////////////////////////////////////////
    //  IUnknown
    //////////////////////////////////////////////////////////////////////////
    STDMETHODIMP QueryInterface(REFIID riid, void **ppv); 
    STDMETHODIMP_(ULONG) AddRef() { return GetOwner()->AddRef(); } // gets called often...
    STDMETHODIMP_(ULONG) Release() { return GetOwner()->Release(); }


     //////////////////////////////////////////////////////////////////////////
    //  IAMStreamConfig
    //////////////////////////////////////////////////////////////////////////
    HRESULT STDMETHODCALLTYPE SetFormat(AM_MEDIA_TYPE *pmt);
    HRESULT STDMETHODCALLTYPE GetFormat(AM_MEDIA_TYPE **ppmt);
    HRESULT STDMETHODCALLTYPE GetNumberOfCapabilities(int *piCount, int *piSize);
    HRESULT STDMETHODCALLTYPE GetStreamCaps(int iIndex, AM_MEDIA_TYPE **pmt, BYTE *pSCC);

    CPushPinDesktop(HRESULT *phr, CSource *pFilter);
    ~CPushPinDesktop();

    // Override the version that offers exactly one media type
    HRESULT DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pRequest);
    HRESULT FillBuffer(IMediaSample *pSample);
    
    // Set the agreed media type and set up the necessary parameters
    HRESULT GetMediaType(CMediaType *pmt);

    // IQualityControl
	// Not implemented because we aren't going in real time.
	// If the file-writing filter slows the graph down, we just do nothing, which means
	// wait until we're unblocked. No frames are ever dropped.
    STDMETHODIMP Notify(IBaseFilter *pSelf, Quality q)
    {
        return E_FAIL;
    }

	
    //////////////////////////////////////////////////////////////////////////
    //  IKsPropertySet
    //////////////////////////////////////////////////////////////////////////
    HRESULT STDMETHODCALLTYPE Set(REFGUID guidPropSet, DWORD dwID, void *pInstanceData, DWORD cbInstanceData, void *pPropData, DWORD cbPropData);
    HRESULT STDMETHODCALLTYPE Get(REFGUID guidPropSet, DWORD dwPropID, void *pInstanceData,DWORD cbInstanceData, void *pPropData, DWORD cbPropData, DWORD *pcbReturned);
    HRESULT STDMETHODCALLTYPE QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD *pTypeSupport);

};

class CPushSourceDesktop : public CSource
{

private:
	CPushSourceDesktop(IUnknown *pUnk, HRESULT *phr);
	~CPushSourceDesktop();

	CPushPinDesktop *m_pPin;
public:
	static CUnknown * WINAPI CreateInstance(IUnknown *pUnk, HRESULT *phr);
};

