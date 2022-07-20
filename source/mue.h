/*
 * MIT License
 *
 * Copyright (c) 2022 phit666
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#pragma once
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define NO_WARN_MBCS_MFC_DEPRECATION
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#pragma warning(disable : 4244)
#pragma warning(disable : 4018)
#pragma warning(disable : 4267) // SIZE_T to INT
#pragma warning(disable : 4091) // ENUM

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <map>
#include <vector>
#include <mswsock.h>
#include <thread>

#define MUE_CLT_MAX_IO_BUFFER_SIZE		8192

#ifndef MUE_SVR_MAX_IO_BUFFER_SIZE
#define MUE_SVR_MAX_IO_BUFFER_SIZE		8192
#endif

#define MUE_MAX_CONT_REALLOC_REQ		100			
#define MUE_MAX_IOCP_WORKERS		32			
#define MUE_MAX_EVENT_ID			10000

enum class emueiotype
{
	eRECV_IO,
	eSEND_IO,
	eACCEPT_IO,
	eCONNECT_IO,
	eEXIT_IO
};

/**
	Enum class type of mueventaddlog function.
*/
enum class emuelogtype
{
	eINFO = 1,
	eWARNING = 2,
	eERROR = 4,
	eDEBUG = 8,
	eTEST = 16,
	eALL = eINFO | eWARNING | eERROR | eDEBUG,
	eSILENT = 32
};

/**
	Enum class type of Event callback.
*/
enum class emuestatus
{
	eCONNECTED,
	eCLOSED,
	eSOCKERROR,
	eNOEVENCB
};

typedef void muevent;
typedef void mueventbase;

typedef void (*mueventproxyreadcb)(mueventbase* base, char* lpmsg, int len);

typedef void (*mue_loghandler)(emuelogtype logtype, LPCSTR message);

/**
	muevent Read callback typedef.
*/
typedef bool (*mueventreadcb)(mueventbase* base, int event_id, LPVOID argument);

/**
	muevent Event callback typedef.
*/
typedef void (*mueventeventcb)(mueventbase* base, int event_id, emuestatus eventype, LPVOID argument);

/**
	muevent Accept callback typedef.
*/
typedef bool (*mueventacceptcb)(mueventbase* base, int event_id, LPVOID argument);


typedef struct _MUE_PIO_CTX
{
	_MUE_PIO_CTX()
	{
		clear();
	}
	void clear()
	{
		pBuffer = NULL;
		pBufferLen = 0;
		pReallocCounts = 0;
		nSecondOfs = 0;
		nTotalBytes = 0;
		nSentBytes = 0;
		nWaitIO = 0;
		Accept = INVALID_SOCKET;
		memset(&Overlapped, 0, sizeof(OVERLAPPED));
		memset(Buffer, 0, MUE_CLT_MAX_IO_BUFFER_SIZE);
	}
	void clear2()
	{
		pReallocCounts = 0;
		nSecondOfs = 0;
		nTotalBytes = 0;
		nSentBytes = 0;
		nWaitIO = 0;
		Accept = INVALID_SOCKET;
	}
	OVERLAPPED Overlapped;
	WSABUF wsabuf;
	CHAR Buffer[MUE_CLT_MAX_IO_BUFFER_SIZE];
	CHAR* pBuffer;
	int pBufferLen;
	int pReallocCounts;
	int nSecondOfs;
	int nTotalBytes;
	int nSentBytes;
	emueiotype IOOperation;
	int nWaitIO;
	SOCKET Accept;
} MUE_PIO_CTX, * LPMUE_PIO_CTX;

typedef struct _MUE_PS_CTX
{
	_MUE_PS_CTX()
	{
		clear();
	}

	void clear()
	{
		m_socket = INVALID_SOCKET;
		m_index = -1;
		m_eventid = -1;
		memset(AddrBuf, 0, sizeof(AddrBuf));
		m_type = -1;
		m_initbuflen = 0;
		m_connected = false;
		memset(m_ipaddr, 0, sizeof(m_ipaddr));
		recvcb = NULL;
		eventcb = NULL;
		m_conipaddr = 0;
		m_conport = 0;
		m_lastconnecttick = 0;
		IOContext[0].clear();
		IOContext[1].clear();
		arg = NULL;
		arg2 = NULL;
		_this = NULL;
	}

	void clear2()
	{
		m_socket = INVALID_SOCKET;
		m_index = -1;
		m_eventid = -1;
		memset(AddrBuf, 0, sizeof(AddrBuf));
		m_type = -1;
		m_initbuflen = 0;
		m_connected = false;
		memset(m_ipaddr, 0, sizeof(m_ipaddr));
		recvcb = NULL;
		eventcb = NULL;
		m_conipaddr = 0;
		m_conport = 0;
		m_lastconnecttick = 0;
		IOContext[0].clear2();
		IOContext[1].clear2();
		arg = NULL;
		arg2 = NULL;
		_this = NULL;
	}


	intptr_t m_index;
	SOCKET m_socket;
	int m_eventid;
	_MUE_PIO_CTX IOContext[2];
	CHAR AddrBuf[((sizeof(sockaddr_in) + 16) * 2)];
	char m_type;
	char m_ipaddr[16];
	bool m_connected;
	DWORD m_conipaddr;
	DWORD m_conport;
	DWORD m_lastconnecttick;
	DWORD m_initbuflen;
	mueventreadcb recvcb;
	mueventeventcb eventcb;
	LPVOID arg;
	LPVOID arg2;
	LPVOID _this;
} MUE_PS_CTX, * LPMUE_PS_CTX;

