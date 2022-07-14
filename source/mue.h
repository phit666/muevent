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
#include <vector>
#include <mswsock.h>
#include <thread>

#define MUE_CLT_MAX_IO_BUFFER_SIZE		8192
#define MUE_SVR_MAX_IO_BUFFER_SIZE		8192
#define MUE_MAX_CONT_REALLOC_REQ		100			
#define MUE_MAX_IOCP_WORKERS		32			

enum class emueiotype
{
	eRECV_IO,
	eSEND_IO,
	eACCEPT_IO,
	eCONNECT_IO,
	eEXIT_IO
};

enum class emuelogtype
{
	eINFO = 1,
	eWARNING = 2,
	eERROR = 4,
	eDEBUG = 8,
	eTEST = 16,
	eALL = eINFO | eWARNING | eERROR | eDEBUG,
};

enum class emuestatus
{
	eCONNECTED,
	eCLOSED,
	eSOCKERROR,
	eNOEVENCB
};

struct MUE
{
	LPVOID _ctx;
};

typedef void muevent;
typedef void mueventbase;

typedef void (*mue_datahandler)(BYTE index, LPBYTE data, size_t size);
typedef bool (*mue_receivecallback)(MUE* mue, LPVOID argument, LPVOID _this_ptr);
typedef void (*mue_eventcallback)(MUE* mue, emuestatus eventype, LPVOID argument, LPVOID _this_ptr);
typedef bool (*mue_acceptcallback)(MUE* mue, LPVOID argument, LPVOID _this_ptr);
typedef void (*mue_loghandler)(emuelogtype logtype, LPCSTR message);

typedef bool (*mueventreadcb)(muevent* mue, LPVOID argument);
typedef void (*mueventeventcb)(muevent* mue, emuestatus eventype, LPVOID argument);
typedef bool (*mueventacceptcb)(muevent* mue, LPVOID argument);


typedef struct _MUE_PIO_CTX
{
	void clear()
	{
		pBuffer = NULL;
		pBufferLen = 0;
		pReallocCounts = 0;
		nSecondOfs = 0;
		nTotalBytes = 0;
		nSentBytes = 0;
		nWaitIO = 0;
		Accept = -1;
		fnc_datahandler = NULL;
		memset(&Overlapped, 0, sizeof(OVERLAPPED));
		memset(Buffer, 0, MUE_CLT_MAX_IO_BUFFER_SIZE);
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
	mue_datahandler fnc_datahandler;
} MUE_PIO_CTX, * LPMUE_PIO_CTX;

typedef struct _MUE_PS_CTX
{
	void clear()
	{
		m_socket = -1;
		m_index = -1;
		memset(AddrBuf, 0, ((sizeof(sockaddr_in) + 16) * 2));
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
	}

	intptr_t m_index;
	SOCKET m_socket;
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
	LPVOID _this;
} MUE_PS_CTX, * LPMUE_PS_CTX;

