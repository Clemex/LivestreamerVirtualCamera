#include "stdafx.h"

#include "PushSource.h"
#include "PushGuids.h"
#include "Livestreamer.h"

#include "debug_helpers.h"

#define MIN(a,b)  ((a) < (b) ? (a) : (b))  // danger! can evaluate "a" twice.

// the default child constructor...
CPushPinDesktop::CPushPinDesktop(HRESULT *phr, CSource *pFilter)
    : CSourceStream(NAME("Push Source CPushPinDesktop child/pin"), phr, pFilter, L"Capture"),
    previousFrameEndTime(0)
{
    livestreamer = new Livestreamer();
    //launchDebugger();
    hScrDc = GetDC(NULL);

    // m_rtFrameLength is also re-negotiated later...
    m_rtFrameLength = UNITS / 30;
}

CPushPinDesktop::~CPushPinDesktop()
{
    delete livestreamer;
    // Release the device context stuff
    ::ReleaseDC(NULL, hScrDc);
    ::DeleteDC(hScrDc);

}

void CPushPinDesktop::CopyImageToDataBlock(HDC hScrDC, BYTE *pData, BITMAPINFO *pHeader, IMediaSample *pSample)
{
    HDC hMemDC;
    int iFinalStretchHeight = livestreamer->GetHeight();
    int iFinalStretchWidth = livestreamer->GetWidth();

    hMemDC = CreateCompatibleDC(hScrDC); //  0.02ms Anything else to reuse, this one's pretty fast...?

    BITMAPINFO tweakableHeader;
    memcpy(&tweakableHeader, pHeader, sizeof(BITMAPINFO));

    GetDIBits(hScrDC, livestreamer->GetNextFrame(), 0, iFinalStretchHeight, pData, &tweakableHeader, DIB_RGB_COLORS);
    // clean up
    DeleteDC(hMemDC);
}

//
// DecideBufferSize
//
// This will always be called after the format has been sucessfully
// negotiated (this is negotiatebuffersize). So we have a look at m_mt to see what size image we agreed.
// Then we can ask for buffers of the correct size to contain them.
//
HRESULT CPushPinDesktop::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProperties)
{
    CheckPointer(pAlloc, E_POINTER);
    CheckPointer(pProperties, E_POINTER);

    CAutoLock cAutoLock(m_pFilter->pStateLock());
    HRESULT hr = NOERROR;

    VIDEOINFO *pvi = (VIDEOINFO *)m_mt.Format();
    BITMAPINFOHEADER header = pvi->bmiHeader;

    int bytesPerLine;
    // there may be a windows method that would do this for us...GetBitmapSize(&header); but might be too small for VLC? LODO try it :)
    // some pasted code...
    int bytesPerPixel = (header.biBitCount / 8);

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
    hr = pAlloc->SetProperties(pProperties, &Actual);
    if (FAILED(hr))
    {
        return hr;
    }

    // Is this allocator unsuitable?
    if (Actual.cbBuffer < pProperties->cbBuffer)
    {
        return E_FAIL;
    }

    return NOERROR;
} // DecideBufferSize

HRESULT CPushPinDesktop::FillBuffer(IMediaSample *pSample)
{
    BYTE *pData;

    CheckPointer(pSample, E_POINTER);

    // Access the sample's data buffer
    pSample->GetPointer(&pData);

    VIDEOINFOHEADER *pVih = (VIDEOINFOHEADER*)m_mt.pbFormat;

    CopyImageToDataBlock(hScrDc, pData, (BITMAPINFO *) &(pVih->bmiHeader), pSample);

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

// Get: Return the pin category (our only property). 
HRESULT CPushPinDesktop::Get(
    REFGUID guidPropSet,   // Which property set.
    DWORD dwPropID,        // Which property in that set.
    void *pInstanceData,   // Instance data (ignore).
    DWORD cbInstanceData,  // Size of the instance data (ignore).
    void *pPropData,       // Buffer to receive the property data.
    DWORD cbPropData,      // Size of the buffer.
    DWORD *pcbReturned     // Return the size of the property.
)
{
    if (guidPropSet != AMPROPSETID_Pin)             return E_PROP_SET_UNSUPPORTED;
    if (dwPropID != AMPROPERTY_PIN_CATEGORY)        return E_PROP_ID_UNSUPPORTED;
    if (pPropData == NULL && pcbReturned == NULL)   return E_POINTER;

    if (pcbReturned) *pcbReturned = sizeof(GUID);
    if (pPropData == NULL)          return S_OK; // Caller just wants to know the size. 
    if (cbPropData < sizeof(GUID))  return E_UNEXPECTED;// The buffer is too small.

    *(GUID *)pPropData = PIN_CATEGORY_CAPTURE; // PIN_CATEGORY_PREVIEW ?
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CPushPinDesktop::GetFormat(AM_MEDIA_TYPE **ppmt)
{
    CAutoLock cAutoLock(m_pFilter->pStateLock());

    *ppmt = CreateMediaType(&m_mt);
    return S_OK;
}

HRESULT CPushPinDesktop::GetMediaType(CMediaType *pmt) // AM_MEDIA_TYPE basically == CMediaType
{
    CheckPointer(pmt, E_POINTER);
    CAutoLock cAutoLock(m_pFilter->pStateLock());

    VIDEOINFO *pvi = (VIDEOINFO *)pmt->AllocFormatBuffer(sizeof(VIDEOINFO));
    if (NULL == pvi)
        return(E_OUTOFMEMORY);

    // Initialize the VideoInfo structure before configuring its members
    ZeroMemory(pvi, sizeof(VIDEOINFO));

    pvi->bmiHeader.biCompression = BI_RGB;
    pvi->bmiHeader.biBitCount = 24;

    // Now adjust some parameters that are the same for all formats
    pvi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pvi->bmiHeader.biWidth = livestreamer->GetWidth();
    pvi->bmiHeader.biHeight = livestreamer->GetHeight();
    pvi->bmiHeader.biPlanes = 1;
    pvi->bmiHeader.biSizeImage = GetBitmapSize(&pvi->bmiHeader); // calculates the size for us, after we gave it the width and everything else we already chucked into it
    pvi->bmiHeader.biClrImportant = 0;

    pvi->AvgTimePerFrame = m_rtFrameLength; // from our config or default

    SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
    SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

    pmt->SetType(&MEDIATYPE_Video);
    pmt->SetFormatType(&FORMAT_VideoInfo);
    pmt->SetTemporalCompression(FALSE);

    // Work out the GUID for the subtype from the header info.
    if (*pmt->Subtype() == GUID_NULL) {
        const GUID SubTypeGUID = GetBitmapSubtype(&pvi->bmiHeader);
        pmt->SetSubtype(&SubTypeGUID);
    }

    return NOERROR;

} // GetMediaType

HRESULT STDMETHODCALLTYPE CPushPinDesktop::GetNumberOfCapabilities(int *piCount, int *piSize)
{
    *piCount = 1;
    *piSize = sizeof(VIDEO_STREAM_CONFIG_CAPS);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CPushPinDesktop::GetStreamCaps(int iIndex, AM_MEDIA_TYPE **pmt, BYTE *pSCC)
{

    CAutoLock cAutoLock(m_pFilter->pStateLock());
    HRESULT hr = GetMediaType(&m_mt);

    if (FAILED(hr))
    {
        return hr;
    }

    *pmt = CreateMediaType(&m_mt); // a windows lib method, also does a copy for us
    if (*pmt == NULL) return E_OUTOFMEMORY;

    return hr;
}

HRESULT CPushPinDesktop::OnThreadCreate() {
    return S_OK;
}

HRESULT CPushPinDesktop::QueryInterface(REFIID riid, void **ppv)
{
    // Standard OLE stuff, needed for capture source
    if (riid == _uuidof(IAMStreamConfig))
        *ppv = (IAMStreamConfig*)this;
    else if (riid == _uuidof(IKsPropertySet))
        *ppv = (IKsPropertySet*)this;
    else
        return CSourceStream::QueryInterface(riid, ppv);

    AddRef(); // avoid interlocked decrement error... // I think
    return S_OK;
}

// QuerySupported: Query whether the pin supports the specified property.
HRESULT CPushPinDesktop::QuerySupported(REFGUID guidPropSet, DWORD dwPropID, DWORD *pTypeSupport)
{
    if (guidPropSet != AMPROPSETID_Pin)
        return E_PROP_SET_UNSUPPORTED;
    if (dwPropID != AMPROPERTY_PIN_CATEGORY)
        return E_PROP_ID_UNSUPPORTED;
    // We support getting this property, but not setting it.
    if (pTypeSupport)
        *pTypeSupport = KSPROPERTY_SUPPORT_GET;
    return S_OK;
}

HRESULT CPushPinDesktop::Set(REFGUID guidPropSet, DWORD dwID, void *pInstanceData,
    DWORD cbInstanceData, void *pPropData, DWORD cbPropData)
{
    // Set: we don't have any specific properties to set...that we advertise yet anyway, and who would use them anyway?
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE CPushPinDesktop::SetFormat(AM_MEDIA_TYPE *pmt)
{
    CAutoLock cAutoLock(m_pFilter->pStateLock());

    if (pmt != NULL)
    {
        m_mt = *pmt;
    }

    return S_OK;
}

CPushSourceDesktop::CPushSourceDesktop(IUnknown *pUnk, HRESULT *phr)
    : CSource(NAME("Livestreamer Virtual Camera"), pUnk, CLSID_LivestreamerVirtualCamera)
{
    // The pin magically adds itself to our pin array.
    // except its not an array since we just have one [?]
    m_pPin = new CPushPinDesktop(phr, this);

    if (phr)
    {
        if (m_pPin == NULL)
            *phr = E_OUTOFMEMORY;
        else
            *phr = S_OK;
    }
}

CPushSourceDesktop::~CPushSourceDesktop() // parent destructor
{
    // COM should call this when the refcount hits 0...
    // but somebody should make the refcount 0...
    delete m_pPin;
}

CUnknown * WINAPI CPushSourceDesktop::CreateInstance(IUnknown *pUnk, HRESULT *phr)
{
    //launchDebugger();

    // the first entry point
    CPushSourceDesktop *pNewFilter = new CPushSourceDesktop(pUnk, phr);

    if (phr)
    {
        if (pNewFilter == NULL)
            *phr = E_OUTOFMEMORY;
        else
            *phr = S_OK;
    }
    return pNewFilter;
}
