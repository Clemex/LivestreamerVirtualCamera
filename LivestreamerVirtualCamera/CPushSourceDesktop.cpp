#include "stdafx.h"
#include "PushSource.h"
#include "PushGuids.h"
#include "Livestreamer.h"

HRESULT STDMETHODCALLTYPE CPushPinDesktop::SetFormat(AM_MEDIA_TYPE *pmt)
{
	CAutoLock cAutoLock(m_pFilter->pStateLock());

	if (pmt != NULL)
	{
		m_mt = *pmt;
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CPushPinDesktop::GetFormat(AM_MEDIA_TYPE **ppmt)
{
	CAutoLock cAutoLock(m_pFilter->pStateLock());

	*ppmt = CreateMediaType(&m_mt);
	return S_OK;
}


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

HRESULT CPushPinDesktop::Set(REFGUID guidPropSet, DWORD dwID, void *pInstanceData,
	DWORD cbInstanceData, void *pPropData, DWORD cbPropData)
{
	// Set: we don't have any specific properties to set...that we advertise yet anyway, and who would use them anyway?
	return E_NOTIMPL;
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
   pvi->bmiHeader.biWidth = livestreamer.GetWidth();
   pvi->bmiHeader.biHeight = livestreamer.GetHeight(); 
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


CPushSourceDesktop::CPushSourceDesktop(IUnknown *pUnk, HRESULT *phr)
           : CSource(NAME("PushSourceDesktop Parent"), pUnk, CLSID_PushSourceDesktop)
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
