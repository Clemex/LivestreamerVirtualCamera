//------------------------------------------------------------------------------
// File: PushGuids.h
//
// Desc: DirectShow sample code - GUID definitions for PushSource filter set
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#pragma once

#ifndef __PUSHGUIDS_DEFINED
#define __PUSHGUIDS_DEFINED

#ifdef _WIN64
// {937621CD-DDBC-4E4A-B1E2-56233ECDC680}
DEFINE_GUID(CLSID_LivestreamerVirtualCamera, 
	0x937621cd, 0xddbc, 0x4e4a, 0xb1, 0xe2, 0x56, 0x23, 0x3e, 0xcd, 0xc6, 0x80);

#else
// {937621CD-DDBC-4E4A-B1E2-56233ECDC680}
DEFINE_GUID(CLSID_LivestreamerVirtualCamera,
	0x937621cd, 0xddbc, 0x4e4a, 0xb1, 0xe2, 0x56, 0x23, 0x3e, 0xcd, 0xc6, 0x80);
#endif

#endif
