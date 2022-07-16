#include "mueiocp.h"

#pragma comment(lib, "wsock32.lib")
#pragma comment(lib, "ws2_32.lib")

void __stdcall pAPC(ULONG_PTR param);
char g_fakebuf[1] = { 0 };

mueiocp::mueiocp()
{
	this->m_fnAcceptEx = NULL;
	this->m_fnConnectEx = NULL;
	this->m_fnGetAcceptExAddr = NULL;
	this->m_listensocket = SOCKET_ERROR;
	this->m_completionport = NULL;
	this->m_acceptctx = NULL;
	this->m_acceptcb = NULL;
	this->m_mueventmaps.clear();
	this->m_workers = 0;
	this->m_acceptarg = NULL;
	this->m_event = NULL;
	this->m_listenport = 0;
	this->m_eventid = 0;
	this->m_activeworkers = 0;
	this->m_listenip = 0;

	this->m_initcltextbuffsize = MUE_CLT_MAX_IO_BUFFER_SIZE;
	this->m_initsvrextbuffsize = MUE_SVR_MAX_IO_BUFFER_SIZE;
	this->m_logverboseflags = (DWORD)emuelogtype::eINFO | (DWORD)emuelogtype::eERROR;
	this->fnc_loghandler = NULL;


	InitializeCriticalSection(&this->m_crit);
	for (int i = 0; i < MUE_MAX_IOCP_WORKERS; i++) {
		this->m_t[i] = nullptr;
		this->m_terrcounts[i] = 0;
	}
}

mueiocp::~mueiocp()
{
	this->addlog(emuelogtype::eDEBUG, "%s(), deallocator called.", __func__);
#if _MUE_GCQSEX_ == 1
	for (int i = 0; i < MUE_MAX_IOCP_WORKERS; i++) {
		if (this->m_t[i] != nullptr) {
			QueueUserAPC(&pAPC, this->m_t[i]->native_handle(), (ULONG_PTR)this);
			this->addlog(emuelogtype::eDEBUG, "~mueiocp(), QueueUserAPC sent to thread %d.", i);
		}
	}
#else
	int activeworkers = this->m_activeworkers;
	for (int i = 0; i < activeworkers; i++) {
		this->postqueued();
	}
#endif
	if (this->m_completionport != NULL)
		CloseHandle(this->m_completionport);
	this->addlog(emuelogtype::eDEBUG, "%s(), m_completionport handle closed.", __func__);
	for (int i = 0; i < MUE_MAX_IOCP_WORKERS; i++) {
		if (this->m_t[i] != nullptr) {
			this->m_t[i]->join();
			delete this->m_t[i];
			this->addlog(emuelogtype::eDEBUG, "%s(), worker %d thread deleted.", __func__, i);
		}
	}
	this->clear();
	DeleteCriticalSection(&this->m_crit);
	this->addlog(emuelogtype::eDEBUG, "%s(), critical section deleted.", __func__);
	WSACleanup();
}

void mueiocp::clear()
{
	LPMUE_PS_CTX _ctx = NULL;
	std::map <int, LPMUE_PS_CTX>::iterator iterq;
	this->lock();
	for (iterq = this->m_mueventmaps.begin(); iterq != this->m_mueventmaps.end(); iterq++)
	{
		_ctx = iterq->second;
		if (_ctx != NULL) {
			this->addlog(emuelogtype::eDEBUG, "%s(), closing event id %d.", __func__, _ctx->m_eventid);
			if (_ctx->m_socket != INVALID_SOCKET)
				::closesocket(_ctx->m_socket);
			if (_ctx->IOContext[1].pBuffer != NULL) {
				::free(_ctx->IOContext[1].pBuffer);
			}
			delete _ctx;
		}
	}
	this->m_mueventmaps.clear();
	this->unlock();
}

void mueiocp::lock()
{
	EnterCriticalSection(&this->m_crit);
}

void mueiocp::unlock()
{
	LeaveCriticalSection(&this->m_crit);
}

void mueiocp::remove(int event_id)
{
	std::map<int, LPMUE_PS_CTX>::iterator Iter;
	LPMUE_PS_CTX _ctx = NULL;

	this->lock();

	Iter = this->m_mueventmaps.find(event_id);

	if (Iter == this->m_mueventmaps.end()) {
		this->unlock();
		return;
	}

	_ctx = Iter->second;

	if (_ctx->IOContext[1].pBuffer != NULL) {
		::free(_ctx->IOContext[1].pBuffer);
	}

	delete _ctx;

	this->m_mueventmaps.erase(Iter);
	this->addlog(emuelogtype::eDEBUG, "%s(), delete event id %d.", __func__, event_id);

	this->unlock();
}

bool mueiocp::iseventidvalid(int event_id)
{
	bool bret = false;
	if (event_id == -1)
		return bret;
	if (this->getctx(event_id) != NULL)
		bret = true;
	return bret;
}

int mueiocp::geteventid()
{
	std::map<int, LPMUE_PS_CTX>::iterator Iter;
	int event_id = -1;
	int ctr = 0;
	this->lock();
	while (true) {

		if (this->m_eventid >= MUE_MAX_EVENT_ID) {
			this->m_eventid = 0;
		}

		if (ctr >= MUE_MAX_EVENT_ID) {
			break;
		}

		Iter = this->m_mueventmaps.find(this->m_eventid);
		if (Iter == this->m_mueventmaps.end()) {
			event_id = this->m_eventid;
			this->m_eventid++;
			break;
		}

		this->m_eventid++;
		ctr++;
	}
	this->unlock();
	return event_id;
}

LPMUE_PS_CTX mueiocp::getctx(int event_id)
{
	std::map<int, LPMUE_PS_CTX>::iterator Iter;
	if (event_id == -1)
		return NULL;
	this->lock();
	Iter = this->m_mueventmaps.find(event_id);
	if (Iter == this->m_mueventmaps.end()) {
		this->unlock();
		return NULL;
	}
	this->unlock();
	return Iter->second;
}

void mueiocp::postqueued()
{
	DWORD dwBytes = 0;
	this->lock();
	LPMUE_PS_CTX postctx = new MUE_PS_CTX;
	postctx->clear();
	int event_id = this->geteventid();
	this->m_mueventmaps.insert(std::pair<int, LPMUE_PS_CTX>(event_id, postctx));
	postctx->IOContext[0].wsabuf.buf = this->m_acceptctx->IOContext[0].Buffer;
	postctx->IOContext[0].wsabuf.len = MUE_CLT_MAX_IO_BUFFER_SIZE;
	postctx->IOContext[0].IOOperation = emueiotype::eEXIT_IO;
	postctx->m_eventid = event_id;
	if (PostQueuedCompletionStatus(this->m_completionport, dwBytes, (ULONG_PTR)event_id, (LPOVERLAPPED) & (postctx->IOContext[0].Overlapped)) == 0)
	{
		this->addlog(emuelogtype::eERROR, "%s(), event id %d PostQueuedCompletionStatus Error: %d", __func__, GetLastError(), event_id);
		this->unlock();
		return;
	}
	this->addlog(emuelogtype::eDEBUG, "%s(), event id %d PostQueuedCompletionStatus succeeded.", __func__, event_id);
	this->unlock();
}

void mueiocp::init(int cpucorenum, mue_loghandler loghandler, DWORD logverboseflags, size_t initclt2ndbufsize, size_t initsvr2ndbufsize)
{
	SYSTEM_INFO SystemInfo;
	HANDLE tmphandle = NULL;

	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	wVersionRequested = MAKEWORD(2, 2);

	this->m_initcltextbuffsize = (initclt2ndbufsize != NULL) ? initclt2ndbufsize : this->m_initcltextbuffsize;
	this->m_initsvrextbuffsize = (initsvr2ndbufsize != NULL) ? initsvr2ndbufsize : this->m_initsvrextbuffsize;
	this->m_logverboseflags = (logverboseflags != (DWORD)-1) ? logverboseflags : this->m_logverboseflags;
	this->fnc_loghandler = (loghandler != NULL) ? loghandler : this->fnc_loghandler;

	this->addlog(emuelogtype::eDEBUG, "mueiocp version %02d.%02d.%02d", _MUEIOCP_MAJOR_VER_, _MUEIOCP_MINOR_VER_, _MUEIOCP_PATCH_VER_);

	err = WSAStartup(wVersionRequested, &wsaData);

	if (err != 0)
	{
		this->addlog(emuelogtype::eERROR, "%s(), WSAStartup failed. with error: %d.", __func__, GetLastError());
		return;
	}

	if (LOBYTE(wsaData.wVersion) != 2 ||
		HIBYTE(wsaData.wVersion) != 2) {
		WSACleanup();
		this->addlog(emuelogtype::eERROR, "%s(), WINSOCK wVersion not matched.", __func__);
		return;
	}

	if (cpucorenum < 1) {
		GetSystemInfo(&SystemInfo);
		cpucorenum = SystemInfo.dwNumberOfProcessors;
	}

	this->m_workers = cpucorenum * 2;

	if (this->m_workers > MUE_MAX_IOCP_WORKERS)
		this->m_workers = MUE_MAX_IOCP_WORKERS;

	this->m_completionport = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (this->m_completionport == NULL)
	{
		this->addlog(emuelogtype::eERROR, "%s(), CreateIoCompletionPort failed with error: %d.", __func__, GetLastError());
		return;
	}

	this->m_listensocket = createsocket();

	if (!this->inithelperfunc()) {
		return;
	}

	this->addlog(emuelogtype::eDEBUG, "%s() succeeded, iocp workers set to %d.", __func__, this->m_workers);
}

void mueiocp::dispatch(bool wait)
{
	this->m_tindex = 0;
	for (int n = 0; n < this->m_workers; n++) {
		this->m_tindex = static_cast<intptr_t>(n);
		this->m_t[n] = new std::thread([this]() {IOCPServerWorker((LPVOID)this->m_tindex); });
	}

	if (wait == true) {
		this->m_event = CreateEventA(0, TRUE, FALSE, 0);
		if (this->m_event != NULL) {
			WaitForSingleObject(this->m_event, INFINITE);
			CloseHandle(this->m_event);
			this->m_event = NULL;
		}
	}
}

void mueiocp::dispatchbreak()
{
	if (this->m_event != NULL) {
		SetEvent(this->m_event);
	}
}

void mueiocp::setacceptcbargument(LPVOID arg)
{
	this->m_acceptarg = arg;
}

void mueiocp::setreadeventcbargument(int event_id, LPVOID arg)
{
	this->lock();
	LPMUE_PS_CTX _ctx = this->getctx(event_id);
	if (_ctx == NULL) {
		this->addlog(emuelogtype::eERROR, "%s(), event id %d ctx is NULL.", __func__, event_id);
		this->unlock();
		return;
	}
	_ctx->arg = arg;
	this->unlock();
}

void mueiocp::listen(int listenport, mueventacceptcb acceptcb, LPVOID arg, char* listenip)
{
	this->m_acceptctx = new MUE_PS_CTX;
	this->m_acceptctx->clear();

	this->m_acceptcb = acceptcb;
	this->m_acceptarg = arg;
	this->m_listenport = listenport;

	if (listenip != NULL) {
		struct hostent* h = gethostbyname(listenip);
		this->m_listenip = (h != NULL) ? ntohl(*(DWORD*)h->h_addr) : 0;
	}

	if (!this->createlistensocket(listenport)) {
		delete this->m_acceptctx;
		return;
	}

	this->m_accepteventid = this->geteventid();

	if (this->m_accepteventid == -1) {
		this->addlog(emuelogtype::eERROR, "%s(), geteventid failed.", __func__);
		delete this->m_acceptctx;
		return;
	}

	this->updatecompletionport(this->m_listensocket, this->m_accepteventid);

	this->m_acceptctx->IOContext[0].wsabuf.buf = this->m_acceptctx->IOContext[0].Buffer;
	this->m_acceptctx->IOContext[0].wsabuf.len = MUE_CLT_MAX_IO_BUFFER_SIZE;
	this->m_acceptctx->IOContext[0].IOOperation = emueiotype::eACCEPT_IO;
	this->m_acceptctx->m_socket = this->m_listensocket;
	this->m_acceptctx->m_eventid = this->m_accepteventid;

	if (!this->do_acceptex()) {
		delete this->m_acceptctx;
		this->addlog(emuelogtype::eERROR, "%s(), do_acceptex failed.", __func__);
		return;
	}

	this->m_mueventmaps.insert(std::pair<int, LPMUE_PS_CTX>(this->m_accepteventid, this->m_acceptctx));
	this->addlog(emuelogtype::eDEBUG, "%s() succeeded, listen port is %d.", __func__, this->m_listenport);
}

void mueiocp::setconnectcb(int event_id, mueventreadcb readcb, mueventeventcb eventcb, LPVOID arg)
{
	this->lock();
	
	LPMUE_PS_CTX _ctx = this->getctx(event_id);

	if (_ctx == NULL) {
		this->addlog(emuelogtype::eERROR, "%s(), event id %d ctx is NULL.", __func__, event_id);
		this->unlock();
		return;
	}
	_ctx->recvcb = readcb;
	_ctx->eventcb = eventcb;
	_ctx->arg = arg;
	this->unlock();
}

bool mueiocp::do_acceptex()
{
	DWORD dwRecvNumBytes;
	this->m_acceptctx->IOContext[0].Accept = this->createsocket();

	if (this->m_acceptctx->IOContext[0].Accept == INVALID_SOCKET) {
		this->addlog(emuelogtype::eERROR, "%s(), createsocket() failed.", __func__);
		return false;
	}

	int nRet = this->m_fnAcceptEx(
		this->m_listensocket,
		this->m_acceptctx->IOContext[0].Accept,
		this->m_acceptctx->AddrBuf,
		0,
		sizeof(sockaddr_in) + 16,
		sizeof(sockaddr_in) + 16,
		&dwRecvNumBytes,
		(LPOVERLAPPED) &(this->m_acceptctx->IOContext[0].Overlapped));

	if (nRet == FALSE && (ERROR_IO_PENDING != WSAGetLastError())) {
		this->addlog(emuelogtype::eERROR, "%s(), AcceptEx() failed: %d.", __func__, WSAGetLastError());
		return(FALSE);
	}
	return true;
}

bool mueiocp::redo_acceptex()
{
	this->lock();

	if (this->m_listensocket != INVALID_SOCKET)
		::closesocket(this->m_listensocket);

	this->m_listensocket = createsocket();

	if (!this->createlistensocket(this->m_listenport)) {
		this->addlog(emuelogtype::eERROR, "%s(), createlistensocket failed.", __func__);
		this->unlock();
		return false;
	}
	if (!this->updatecompletionport(this->m_listensocket, this->m_accepteventid)) {
		this->addlog(emuelogtype::eERROR, "%s(), updatecompletionport faield.", __func__);
		this->unlock();
		return false;
	}
	this->m_acceptctx->m_socket = this->m_listensocket;
	if (!this->do_acceptex()) {
		this->addlog(emuelogtype::eERROR, "%s(), do_acceptex failed.", __func__);
		this->unlock();
		return false;
	}
	this->addlog(emuelogtype::eDEBUG, "%s(), listen update successfull.", __func__);
	this->unlock();
	return true;
}

int mueiocp::createlistensocket(WORD port)
{
	sockaddr_in InternetAddr;
	int nRet;

	if (port == 0) {
		return 1;
	}

	if (this->m_listensocket == SOCKET_ERROR)
	{
		this->addlog(emuelogtype::eERROR, "%s(), WSASocket() failed with error %d.", __func__, WSAGetLastError());
		return 0;
	}
	else
	{
		InternetAddr.sin_family = AF_INET;
		InternetAddr.sin_addr.S_un.S_addr = htonl(this->m_listenip);
		InternetAddr.sin_port = htons(port);
		nRet = ::bind(this->m_listensocket, (sockaddr*)&InternetAddr, 16);

		if (nRet == SOCKET_ERROR)
		{
			this->addlog(emuelogtype::eERROR, "%s(), bind() failed with error %d.", __func__, WSAGetLastError());
			return 0;
		}
		else
		{
			nRet = ::listen(this->m_listensocket, 5);
			if (nRet == SOCKET_ERROR)
			{
				this->addlog(emuelogtype::eERROR, "%s(), listen() failed with error %d.", __func__, WSAGetLastError());
				return 0;
			}
			else
			{
				return 1;
			}
		}
	}
}

SOCKET mueiocp::createsocket()
{
	SOCKET s = INVALID_SOCKET;
	s = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (s == INVALID_SOCKET) {
		this->addlog(emuelogtype::eERROR, "%s(), WSASocket (sdSocket) failed: %d.", __func__, WSAGetLastError());
		return s;
	}
	return s;
}

bool mueiocp::updatecompletionport(SOCKET socket, int event_id)
{
	if (socket == INVALID_SOCKET) {
		this->addlog(emuelogtype::eERROR, "%s(), socket is invalid.", __func__);
		return false;
	}

	if (!CreateIoCompletionPort((HANDLE)socket, this->m_completionport, (ULONG_PTR)event_id, 0))
	{
		this->addlog(emuelogtype::eERROR, "%s(), CreateIoCompletionPort failed: %d.", __func__, GetLastError());
		return false;
	}
	return true;
}

bool mueiocp::inithelperfunc()
{
	GUID acceptex_guid = WSAID_ACCEPTEX;
	GUID getacceptex_guid = WSAID_GETACCEPTEXSOCKADDRS;
	GUID connectex_guid = WSAID_CONNECTEX;
	DWORD bytes;

	int nRet = WSAIoctl(
		this->m_listensocket,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&acceptex_guid,
		sizeof(acceptex_guid),
		&this->m_fnAcceptEx,
		sizeof(this->m_fnAcceptEx),
		&bytes,
		NULL,
		NULL
	);

	if (nRet == SOCKET_ERROR)
	{
		this->addlog(emuelogtype::eERROR, "%s(), Failed to load AcceptEx: %d.", __func__, WSAGetLastError());
		return false;
	}

	nRet = WSAIoctl(
		this->m_listensocket,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&getacceptex_guid,
		sizeof(getacceptex_guid),
		&this->m_fnGetAcceptExAddr,
		sizeof(this->m_fnGetAcceptExAddr),
		&bytes,
		NULL,
		NULL
	);

	if (nRet == SOCKET_ERROR)
	{
		this->addlog(emuelogtype::eERROR, "%s(), Failed to load GetAcceptExSocketAddr: %d.", __func__, WSAGetLastError());
		return false;
	}

	nRet = WSAIoctl(
		this->m_listensocket,
		SIO_GET_EXTENSION_FUNCTION_POINTER,
		&connectex_guid,
		sizeof(connectex_guid),
		&this->m_fnConnectEx,
		sizeof(this->m_fnConnectEx),
		&bytes,
		NULL,
		NULL
	);

	if (nRet == SOCKET_ERROR)
	{
		this->addlog(emuelogtype::eERROR, "%s(), Failed to load ConnectEx: %d.", __func__, WSAGetLastError());
		return false;
	}

	return true;
}

bool mueiocp::sendbuffer(int event_id, LPBYTE lpMsg, DWORD dwSize)
{
	DWORD SendBytes;

	this->lock();

	LPMUE_PS_CTX lpPerSocketContext = this->getctx(event_id);

	if (lpPerSocketContext == NULL) {
		this->addlog(emuelogtype::eERROR, "%s(), lpPerSocketContext is NULL.", __func__);
		this->unlock();
		return false;
	}

	if (lpPerSocketContext->m_socket == INVALID_SOCKET) {
		this->addlog(emuelogtype::eERROR, "%s(), m_socket is INVALID_SOCKET.", __func__);
		this->unlock();
		return false;
	}

	LPMUE_PIO_CTX	lpIoCtxt = (LPMUE_PIO_CTX)&lpPerSocketContext->IOContext[1];

	if (lpIoCtxt->pBuffer == NULL) {
		this->addlog(emuelogtype::eERROR, "%s(), pBuffer is NULL.", __func__);
		this->unlock();
		return false;
	}

	if (lpIoCtxt->nWaitIO > 0)
	{
		if ((lpIoCtxt->nSecondOfs + dwSize) > lpIoCtxt->pBufferLen)
		{
			if (lpIoCtxt->pReallocCounts >= MUE_MAX_CONT_REALLOC_REQ) {
				this->addlog(emuelogtype::eERROR, "%s(), event id %d pReallocCounts (1) is max out, 0x%x", __func__, event_id, MUE_MAX_CONT_REALLOC_REQ);
				::free(lpIoCtxt->pBuffer);
				lpIoCtxt->pBufferLen = MUE_CLT_MAX_IO_BUFFER_SIZE;
				lpIoCtxt->pBuffer = (CHAR*)calloc(lpIoCtxt->pBufferLen, sizeof(CHAR));
				lpIoCtxt->pReallocCounts = 0;
				this->close(event_id);
				this->unlock();
				return false;
			}

			while (true) {
				lpIoCtxt->pBufferLen *= 2;
				if (lpIoCtxt->pBufferLen > (lpIoCtxt->nSecondOfs + dwSize))
					break;
			}

			CHAR* tmpBuffer = (CHAR*)realloc(lpIoCtxt->pBuffer, lpIoCtxt->pBufferLen);

			if (tmpBuffer == NULL) {
				this->addlog(emuelogtype::eERROR, "%s(), event id %d realloc (1) failed, requested size is %d.", __func__, event_id, lpIoCtxt->pBufferLen);
				this->close(event_id);
				this->unlock();
				return false;
			}

			lpIoCtxt->pBuffer = tmpBuffer;
			lpIoCtxt->pReallocCounts++;

			this->addlog(emuelogtype::eDEBUG, "%s(), event id %d realloc (1) succeeded, requested size is %d.", __func__, event_id, lpIoCtxt->pBufferLen);
		}

		this->addlog(emuelogtype::eTEST, "%s(), event id %d copied %d bytes to pBuffer, pbuffer size now is %d.", __func__,
			event_id, dwSize, lpIoCtxt->nSecondOfs + dwSize);

		memcpy(&lpIoCtxt->pBuffer[lpIoCtxt->nSecondOfs], lpMsg, dwSize);
		lpIoCtxt->nSecondOfs += dwSize;


		this->unlock();
		return true;
	}

	lpIoCtxt->nTotalBytes = 0;

	if (lpIoCtxt->nSecondOfs > 0)
	{
		if (lpIoCtxt->nSecondOfs <= MUE_CLT_MAX_IO_BUFFER_SIZE) {
			memcpy(lpIoCtxt->Buffer, lpIoCtxt->pBuffer, lpIoCtxt->nSecondOfs);
			lpIoCtxt->nTotalBytes = lpIoCtxt->nSecondOfs;
			lpIoCtxt->nSecondOfs = 0;
		}
		else {
			memcpy(lpIoCtxt->Buffer, lpIoCtxt->pBuffer, MUE_CLT_MAX_IO_BUFFER_SIZE);
			lpIoCtxt->nTotalBytes = MUE_CLT_MAX_IO_BUFFER_SIZE;
			lpIoCtxt->nSecondOfs -= MUE_CLT_MAX_IO_BUFFER_SIZE;
			memcpy(lpIoCtxt->pBuffer, &lpIoCtxt->pBuffer[MUE_CLT_MAX_IO_BUFFER_SIZE], lpIoCtxt->nSecondOfs);
		}
	}

	if ((lpIoCtxt->nTotalBytes + dwSize) > MUE_CLT_MAX_IO_BUFFER_SIZE)
	{
		int bufflen = (lpIoCtxt->nTotalBytes + dwSize) - MUE_CLT_MAX_IO_BUFFER_SIZE; // 808
		int difflen = MUE_CLT_MAX_IO_BUFFER_SIZE - lpIoCtxt->nTotalBytes; // 192

		if ((lpIoCtxt->nSecondOfs + bufflen) > lpIoCtxt->pBufferLen) {

			if (lpIoCtxt->pReallocCounts >= MUE_MAX_CONT_REALLOC_REQ) {
				this->addlog(emuelogtype::eERROR, "%s(), event id %d pReallocCounts (2) is max out, %d", __func__, event_id, MUE_MAX_CONT_REALLOC_REQ);
				::free(lpIoCtxt->pBuffer);
				lpIoCtxt->pBufferLen = MUE_CLT_MAX_IO_BUFFER_SIZE;
				lpIoCtxt->pBuffer = (CHAR*)calloc(lpIoCtxt->pBufferLen, sizeof(CHAR));
				lpIoCtxt->pReallocCounts = 0;
				this->close(event_id);
				this->unlock();
				return false;
			}

			while (true) {
				lpIoCtxt->pBufferLen *= 2;
				if (lpIoCtxt->pBufferLen > (lpIoCtxt->nSecondOfs + dwSize))
					break;
			}

			CHAR* tmpBuffer = (CHAR*)realloc(lpIoCtxt->pBuffer, lpIoCtxt->pBufferLen);

			if (tmpBuffer == NULL) {
				this->addlog(emuelogtype::eERROR, "%s(), event id %d realloc (2) failed, requested size is %d.", __func__, event_id, lpIoCtxt->pBufferLen);
				this->close(event_id);
				this->unlock();
				return false;
			}

			lpIoCtxt->pBuffer = tmpBuffer;
			lpIoCtxt->pReallocCounts++;

			this->addlog(emuelogtype::eDEBUG, "%s(), event id %d realloc (2) succeeded, requested size is %d.", __func__, event_id, lpIoCtxt->pBufferLen);
		}

		memcpy(&lpIoCtxt->pBuffer[lpIoCtxt->nSecondOfs], &lpMsg[difflen], bufflen);
		lpIoCtxt->nSecondOfs += bufflen;

		if (difflen > 0) {
			memcpy(&lpIoCtxt->Buffer[lpIoCtxt->nTotalBytes], lpMsg, difflen);
			lpIoCtxt->nTotalBytes += difflen;
		}
	}
	else {
		memcpy(&lpIoCtxt->Buffer[lpIoCtxt->nTotalBytes], lpMsg, dwSize);
		lpIoCtxt->nTotalBytes += dwSize;
	}

	lpIoCtxt->wsabuf.buf = (char*)&lpIoCtxt->Buffer;
	lpIoCtxt->wsabuf.len = lpIoCtxt->nTotalBytes;
	lpIoCtxt->nSentBytes = 0;
	lpIoCtxt->IOOperation = emueiotype::eSEND_IO;


	if (WSASend(lpPerSocketContext->m_socket, &lpIoCtxt->wsabuf, 1, &SendBytes, 0, &lpIoCtxt->Overlapped, NULL) == -1)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			lpIoCtxt->nWaitIO = 0;

			this->addlog(emuelogtype::eERROR, "%s(), event id %d WSASend failed with error %d.", __func__, event_id, WSAGetLastError());
			this->close(event_id, emuestatus::eSOCKERROR);
			this->unlock();
			return false;
		}
	}

	lpIoCtxt->nWaitIO = 1;
	this->unlock();
	return true;
}

bool mueiocp::connect(int event_id, char* initData, int initLen)
{
	struct sockaddr_in remote_address;
	DWORD dwSentBytes = 0;
	this->lock();
	LPMUE_PS_CTX pSocketContext = this->getctx(event_id);

	if (pSocketContext == NULL) {
		this->addlog(emuelogtype::eERROR, "%s(), pSocketContext is NULL.", __func__);
		this->unlock();
		return NULL;
	}

	remote_address.sin_family = AF_INET;
	remote_address.sin_addr.s_addr = htonl(pSocketContext->m_conipaddr);
	remote_address.sin_port = htons(pSocketContext->m_conport);

	pSocketContext->m_initbuflen = initLen;

	int nRet = this->m_fnConnectEx(
		pSocketContext->m_socket,
		(struct sockaddr*)&remote_address,
		sizeof(remote_address),
		initData,
		pSocketContext->m_initbuflen,
		&dwSentBytes,
		(LPOVERLAPPED) & (pSocketContext->IOContext[0].Overlapped));

	if (nRet == FALSE && (ERROR_IO_PENDING != WSAGetLastError())) {
		this->addlog(emuelogtype::eERROR, "%s(), ConnectEx() failed: %d.", __func__, WSAGetLastError());
		this->close(event_id, emuestatus::eSOCKERROR);
		this->unlock();
		return NULL;
	}

	this->addlog(emuelogtype::eDEBUG, "%s(), event id %d Port:%d succeeded.", __func__, event_id, pSocketContext->m_conport);
	pSocketContext->m_lastconnecttick = GetTickCount();
	this->unlock();
	return true;
}

int mueiocp::makeconnect(const char* ipaddr, WORD port, intptr_t index, mue_datahandler datahandler)
{
	int nRet = 0;
	DWORD bytes = 0;
	DWORD Flags = 0;
	DWORD initBufLen = 0;
	struct sockaddr_in addr;
	struct sockaddr_in remote_address;

	this->addlog(emuelogtype::eDEBUG, "%s(), Index %d Port:%d...", __func__, (int)index, port);
	this->lock();
	LPMUE_PS_CTX pSocketContext = new MUE_PS_CTX;
	pSocketContext->clear();
	SOCKET s = this->createsocket();

	if (s == INVALID_SOCKET) {
		this->addlog(emuelogtype::eERROR, "%s(), createsocket failed.", __func__);
		delete pSocketContext;
		this->unlock();
		return -1;
	}

	int event_id = this->geteventid();

	if (event_id == -1) {
		this->addlog(emuelogtype::eERROR, "%s(), geteventid failed.", __func__);
		delete pSocketContext;
		this->unlock();
		return -1;
	}

	if (!this->updatecompletionport(s, event_id)) {
		::closesocket(s);
		delete pSocketContext;
		this->unlock();
		return NULL;
	}

	pSocketContext->IOContext[0].wsabuf.buf = pSocketContext->IOContext[0].Buffer;
	pSocketContext->IOContext[0].wsabuf.len = MUE_CLT_MAX_IO_BUFFER_SIZE;
	pSocketContext->IOContext[0].IOOperation = emueiotype::eCONNECT_IO;
	pSocketContext->IOContext[0].fnc_datahandler = datahandler;

	pSocketContext->IOContext[1].wsabuf.buf = pSocketContext->IOContext[0].Buffer;
	pSocketContext->IOContext[1].wsabuf.len = MUE_CLT_MAX_IO_BUFFER_SIZE;
	pSocketContext->IOContext[1].IOOperation = emueiotype::eSEND_IO;
	pSocketContext->m_index = index;
	pSocketContext->m_socket = s;
	pSocketContext->m_conport = port;
	pSocketContext->m_eventid = event_id;

	struct hostent* h = gethostbyname(ipaddr);
	pSocketContext->m_conipaddr = (h != NULL) ? ntohl(*(DWORD*)h->h_addr) : 0;

	pSocketContext->IOContext[1].pBuffer = (CHAR*)calloc(this->m_initsvrextbuffsize, sizeof(CHAR));
	pSocketContext->IOContext[1].pBufferLen = this->m_initsvrextbuffsize;

	ZeroMemory(&addr, sizeof(addr));
	ZeroMemory(&remote_address, sizeof(remote_address));

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = 0;

	if (bind(s, (SOCKADDR*)&addr, sizeof(addr))) {
		this->addlog(emuelogtype::eERROR, "%s(), bind() failed: %d.", __func__, WSAGetLastError());
		::free(pSocketContext->IOContext[1].pBuffer);
		::closesocket(s);
		delete pSocketContext;
		this->unlock();
		return -1;
	}

	this->addlog(emuelogtype::eDEBUG, "%s(), Index %d Port:%d succeeded.", __func__, (int)index, port);
	this->m_mueventmaps.insert(std::pair<int, LPMUE_PS_CTX>(event_id, pSocketContext));
	this->unlock();
	return event_id;
}

bool mueiocp::handleaccept(LPMUE_PS_CTX ctx)
{
	sockaddr* pLocal = NULL, * pRemote = NULL;
	int nLocal = 0, nRemote = 0;
	char strIP[16] = { 0 };
	DWORD RecvBytes = 0;
	DWORD Flags = 0;
	this->lock();


	int nRet = setsockopt(
		this->m_acceptctx->IOContext[0].Accept,
		SOL_SOCKET,
		SO_UPDATE_ACCEPT_CONTEXT,
		(char*)&this->m_listensocket,
		sizeof(this->m_listensocket)
	);

	if (nRet == SOCKET_ERROR) {
		this->addlog(emuelogtype::eERROR, "%s(), setsockopt failed with error %d.", __func__, WSAGetLastError());
		this->unlock();
		return false;
	}

	this->m_fnGetAcceptExAddr(
		this->m_acceptctx->AddrBuf,
		0,
		sizeof(sockaddr_in) + 16,
		sizeof(sockaddr_in) + 16,
		&pLocal,
		&nLocal,
		&pRemote,
		&nRemote);

	if (pRemote != NULL) {
		inet_ntop(AF_INET, &(((struct sockaddr_in*)pRemote)->sin_addr),
			strIP, sizeof(strIP));
	}


	LPMUE_PS_CTX lpAcceptSocketContext = new MUE_PS_CTX;
	lpAcceptSocketContext->clear();

	memcpy(lpAcceptSocketContext->m_ipaddr, strIP, sizeof(lpAcceptSocketContext->m_ipaddr));

	lpAcceptSocketContext->IOContext[0].wsabuf.buf = lpAcceptSocketContext->IOContext[0].Buffer;
	lpAcceptSocketContext->IOContext[0].wsabuf.len = MUE_CLT_MAX_IO_BUFFER_SIZE;
	lpAcceptSocketContext->IOContext[0].IOOperation = emueiotype::eRECV_IO;

	lpAcceptSocketContext->IOContext[1].wsabuf.buf = lpAcceptSocketContext->IOContext[0].Buffer;
	lpAcceptSocketContext->IOContext[1].wsabuf.len = MUE_CLT_MAX_IO_BUFFER_SIZE;
	lpAcceptSocketContext->IOContext[1].IOOperation = emueiotype::eSEND_IO;

	lpAcceptSocketContext->IOContext[1].pBuffer = (CHAR*)calloc(this->m_initcltextbuffsize, sizeof(CHAR));
	lpAcceptSocketContext->IOContext[1].pBufferLen = this->m_initcltextbuffsize;

	lpAcceptSocketContext->m_socket = ctx->IOContext[0].Accept;
	lpAcceptSocketContext->m_type = 0;

	lpAcceptSocketContext->_this = this;

	int event_id = this->geteventid();
	if (event_id == -1) {
		this->addlog(emuelogtype::eWARNING, "%s(), no available event id.", __func__);
		::free(lpAcceptSocketContext->IOContext[1].pBuffer);
		::closesocket(lpAcceptSocketContext->m_socket);
		delete lpAcceptSocketContext;
		this->unlock();
		return false;
	}

	if (!this->updatecompletionport(this->m_acceptctx->IOContext[0].Accept, event_id)) {
		::free(lpAcceptSocketContext->IOContext[1].pBuffer);
		::closesocket(lpAcceptSocketContext->m_socket);
		delete lpAcceptSocketContext;
		this->unlock();
		return false;
	}

	lpAcceptSocketContext->m_eventid = event_id;
	this->m_mueventmaps.insert(std::pair<int, LPMUE_PS_CTX>(event_id, lpAcceptSocketContext));

	if (!this->m_acceptcb(event_id, this->m_acceptarg)) {
		this->addlog(emuelogtype::eWARNING, "%s(), Accept Callback returned false.", __func__);
		this->close(event_id);
		this->unlock();
		return false;
	}

	nRet = WSARecv(lpAcceptSocketContext->m_socket, &(lpAcceptSocketContext->IOContext[0].wsabuf), 1, &RecvBytes, &Flags,
		&(lpAcceptSocketContext->IOContext[0].Overlapped), NULL);

	if (nRet == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
	{
		this->addlog(emuelogtype::eERROR, "%s(), WSARecv() failed with event id %d Error:%d", __func__, event_id, WSAGetLastError());
		this->close(event_id, emuestatus::eSOCKERROR);
		this->unlock();
		return false;
	}

	if(lpAcceptSocketContext->eventcb != NULL)
		lpAcceptSocketContext->eventcb(event_id, emuestatus::eCONNECTED, lpAcceptSocketContext->arg);
	this->unlock();
	return true;
}

bool mueiocp::handleconnect(LPMUE_PS_CTX ctx, DWORD dwIoSize)
{
	DWORD RecvBytes;
	DWORD Flags = 0;
	this->lock();

	int event_id = ctx->m_eventid;
	LPMUE_PIO_CTX lpIOContext = (LPMUE_PIO_CTX)&ctx->IOContext[0];

	if (lpIOContext == NULL) {
		this->addlog(emuelogtype::eERROR, "%s(), lpIOContext is NULL.", __func__);
		this->unlock();
		return false;
	}

	lpIOContext->IOOperation = emueiotype::eRECV_IO;

	int nRet = setsockopt(ctx->m_socket, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);

	if (nRet == SOCKET_ERROR) {
		this->addlog(emuelogtype::eERROR, "%s(), setsockopt failed with error %d.", __func__, WSAGetLastError());
		this->unlock();
		return false;
	}

	if (ctx->m_initbuflen == dwIoSize) {

		int nRet = WSARecv(ctx->m_socket, &(ctx->IOContext[0].wsabuf), 1, &RecvBytes, &Flags,
			&(ctx->IOContext[0].Overlapped), NULL);

		if (nRet == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
		{
			this->addlog(emuelogtype::eERROR, "%s(), WSARecv() failed with event id %d Error:%d", __func__, event_id, WSAGetLastError());
			this->close(event_id, emuestatus::eSOCKERROR);
			this->unlock();
			return false;
		}

		ctx->m_connected = true;
		if (ctx->eventcb != NULL) {
			ctx->eventcb(event_id, emuestatus::eCONNECTED, ctx->arg);
			if (!this->iseventidvalid(event_id)) {
				this->unlock();
				return false;
			}
		}
	}
	else {
		this->addlog(emuelogtype::eERROR, "%s(), initbuflen:%d != dwIoSize:%d.", __func__, ctx->m_initbuflen, dwIoSize);
		ctx->m_connected = false;
		this->close(event_id, emuestatus::eSOCKERROR);
	}

	this->addlog(emuelogtype::eDEBUG, "%s(), event id %d Socket:%d connected.", __func__, event_id, (int)ctx->m_socket);
	this->unlock();
	return true;
}

bool mueiocp::IoSendSecond(LPMUE_PS_CTX ctx)
{
	DWORD SendBytes;

	LPMUE_PIO_CTX	lpIoCtxt = (LPMUE_PIO_CTX)&ctx->IOContext[1];

	if (lpIoCtxt->nWaitIO > 0)
	{
		return false;
	}

	lpIoCtxt->nTotalBytes = 0;
	if (lpIoCtxt->nSecondOfs > 0)
	{
		if (lpIoCtxt->nSecondOfs <= MUE_CLT_MAX_IO_BUFFER_SIZE) {
			memcpy(lpIoCtxt->Buffer, lpIoCtxt->pBuffer, lpIoCtxt->nSecondOfs);
			lpIoCtxt->nTotalBytes = lpIoCtxt->nSecondOfs;
			lpIoCtxt->nSecondOfs = 0;
		}
		else {
			memcpy(lpIoCtxt->Buffer, lpIoCtxt->pBuffer, MUE_CLT_MAX_IO_BUFFER_SIZE);
			lpIoCtxt->nTotalBytes = MUE_CLT_MAX_IO_BUFFER_SIZE;
			lpIoCtxt->nSecondOfs = lpIoCtxt->nSecondOfs - MUE_CLT_MAX_IO_BUFFER_SIZE;
			memcpy(lpIoCtxt->pBuffer, &lpIoCtxt->pBuffer[MUE_CLT_MAX_IO_BUFFER_SIZE], lpIoCtxt->nSecondOfs);
		}
	}
	else
	{
		return false;
	}

	lpIoCtxt->wsabuf.buf = (char*)&lpIoCtxt->Buffer;
	lpIoCtxt->wsabuf.len = lpIoCtxt->nTotalBytes;
	lpIoCtxt->nSentBytes = 0;
	lpIoCtxt->IOOperation = emueiotype::eSEND_IO;

	if (WSASend(ctx->m_socket, &lpIoCtxt->wsabuf, 1, &SendBytes, 0, &lpIoCtxt->Overlapped, NULL) == -1)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			this->addlog(emuelogtype::eERROR, "%s(), WSASend failed with error %d.", __func__, WSAGetLastError());
			this->close(ctx->m_eventid, emuestatus::eSOCKERROR);
			this->unlock();
			return false;
		}
	}

	lpIoCtxt->nWaitIO = 1;
	return true;
}

bool mueiocp::IoMoreSend(LPMUE_PS_CTX ctx)
{
	DWORD SendBytes;

	LPMUE_PIO_CTX	lpIoCtxt = (LPMUE_PIO_CTX)&ctx->IOContext[1];

	if ((lpIoCtxt->nTotalBytes - lpIoCtxt->nSentBytes) < 0)
	{
		return false;
	}

	lpIoCtxt->wsabuf.buf = (char*)&lpIoCtxt->Buffer[lpIoCtxt->nSentBytes];
	lpIoCtxt->wsabuf.len = lpIoCtxt->nTotalBytes - lpIoCtxt->nSentBytes;
	lpIoCtxt->IOOperation = emueiotype::eSEND_IO;

	if (WSASend(ctx->m_socket, &lpIoCtxt->wsabuf, 1, &SendBytes, 0, &lpIoCtxt->Overlapped, NULL) == -1)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			this->addlog(emuelogtype::eERROR, "%s(), WSASend failed with error %d.", __func__, WSAGetLastError());
			this->close(ctx->m_eventid, emuestatus::eSOCKERROR);
			return false;
		}
	}
	lpIoCtxt->nWaitIO = 1;
	return true;
}

bool mueiocp::handlesend(LPMUE_PS_CTX ctx, DWORD dwIoSize)
{
	this->lock();

	int event_id = ctx->m_eventid;
	LPMUE_PIO_CTX	lpIoCtxt = (LPMUE_PIO_CTX)&ctx->IOContext[1];

	lpIoCtxt->nSentBytes += dwIoSize;
	lpIoCtxt->pReallocCounts = 0;
	if (lpIoCtxt->nSentBytes >= lpIoCtxt->nTotalBytes)
	{
		lpIoCtxt->nWaitIO = 0;
		if (lpIoCtxt->nSecondOfs > 0)
		{
			if (!this->IoSendSecond(ctx)) {
				this->unlock();
				return false;
			}
		}
	}
	else
	{
		if (!this->IoMoreSend(ctx)) {
			this->unlock();
			return false;
		}
	}
	this->unlock();
	return true;
}

bool mueiocp::handlereceive(LPMUE_PS_CTX ctx, DWORD dwIoSize)
{
	DWORD RecvBytes = 0;
	DWORD Flags = 0;

	this->lock();


	LPMUE_PIO_CTX	lpIOContext = (LPMUE_PIO_CTX)&ctx->IOContext[0];
	int eventid = ctx->m_eventid;

	lpIOContext->nSentBytes += dwIoSize;
	if (ctx->recvcb != NULL && !ctx->recvcb(eventid, ctx->arg)) { // receive callback
		this->addlog(emuelogtype::eERROR, "%s(), Socket Header error %d", __func__, WSAGetLastError());
		this->close(eventid);
		this->unlock();
		return false;
	}

	if (!this->iseventidvalid(eventid)) {
		this->addlog(emuelogtype::eWARNING, "%s(), event id %d is invalid.", __func__, eventid);
		this->unlock();
		return false;
	}

	lpIOContext->nWaitIO = 0;
	Flags = 0;
	ZeroMemory(&(lpIOContext->Overlapped), sizeof(OVERLAPPED));

	lpIOContext->wsabuf.len = MUE_CLT_MAX_IO_BUFFER_SIZE - lpIOContext->nSentBytes;
	lpIOContext->wsabuf.buf = lpIOContext->Buffer + lpIOContext->nSentBytes;
	lpIOContext->IOOperation = emueiotype::eRECV_IO;

	int nRet = WSARecv(ctx->m_socket, &(lpIOContext->wsabuf), 1, &RecvBytes, &Flags,
		&(lpIOContext->Overlapped), NULL);

	if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
	{
		this->addlog(emuelogtype::eERROR, "%s(), WSARecv error event id %d Error:%d.", __func__, eventid, WSAGetLastError());
		this->close(eventid, emuestatus::eSOCKERROR);
		this->unlock();
		return false;
	}

	lpIOContext->nWaitIO = 1;
	this->unlock();
	return true;
}

size_t mueiocp::readbuffer(int event_id, char* buffer, size_t buffersize)
{
	size_t readbytes = 0;

	LPMUE_PS_CTX ctx = this->getctx(event_id);

	if (ctx == NULL) {
		this->addlog(emuelogtype::eERROR, "%s(), ctx is NULL.", __func__);
		return 0;
	}

	LPMUE_PIO_CTX	lpIOContext = (LPMUE_PIO_CTX)&ctx->IOContext[0];

	if (lpIOContext->nSentBytes <= buffersize) {
		memcpy(buffer, lpIOContext->Buffer, lpIOContext->nSentBytes);
		memset(lpIOContext->Buffer, 0, sizeof(lpIOContext->Buffer));
		readbytes = lpIOContext->nSentBytes;
		lpIOContext->nSentBytes = 0;
	}
	else {
		memcpy(buffer, lpIOContext->Buffer, buffersize);
		readbytes = buffersize;
		lpIOContext->nSentBytes -= readbytes;
		memcpy(lpIOContext->Buffer, &lpIOContext->Buffer[buffersize], lpIOContext->nSentBytes);
		memset(&lpIOContext->Buffer[buffersize], 0, readbytes);
	}

	return readbytes;
}

void mueiocp::closesocket(int event_id)
{
	this->lock();

	LPMUE_PS_CTX ctx = this->getctx(event_id);

	if (ctx == NULL) {
		this->addlog(emuelogtype::eDEBUG, "%s(), event id %d is invalid.", __func__, event_id);
		this->unlock();
		return;
	}

	if (ctx->m_socket != INVALID_SOCKET) {
		::closesocket(ctx->m_socket);
		ctx->m_socket = INVALID_SOCKET;

		if (ctx->eventcb != NULL)
			ctx->eventcb(event_id, emuestatus::eCLOSED, ctx->arg);
	}

	this->unlock();
}

void mueiocp::close(int event_id, emuestatus flag)
{
	if (flag == emuestatus::eCLOSED) {
		this->closesocket(event_id);
		return;
	}

	this->lock();

	LPMUE_PS_CTX ctx = this->getctx(event_id);

	if (ctx == NULL) {
		this->addlog(emuelogtype::eDEBUG, "%s(), event id %d is invalid.", __func__, event_id);
		this->unlock();
		return;
	}

	if (ctx->m_socket != INVALID_SOCKET) {
		::closesocket(ctx->m_socket);
		ctx->m_socket = INVALID_SOCKET;
	}

	mueventeventcb eventcb = ctx->eventcb;
	LPVOID eventcb_arg = ctx->arg;

	this->remove(event_id);

	if (flag != emuestatus::eNOEVENCB && eventcb != NULL)
		eventcb(event_id, flag, eventcb_arg);

	this->unlock();
}

void mueiocp::IOCPServerWorker(LPVOID arg)
{
	intptr_t _t_index = (intptr_t)arg;
	int t_index = static_cast<int>(_t_index);
	DWORD	dwIoSize;
	DWORD	Flags = 0;
	DWORD	dwSendNumBytes = 0;
	BOOL	bSuccess = FALSE;
	int event_id = -1;
	LPMUE_PS_CTX	lpPerSocketContext = NULL;
	LPMUE_PS_CTX	lpAcceptSocketContext = NULL;
	LPOVERLAPPED			lpOverlapped = NULL;
	LPMUE_PIO_CTX		lpIOContext = NULL;
#if _MUE_GCQSEX_ == 1
	OVERLAPPED_ENTRY		CPEntry[64];
	ULONG					MaxEntries = 64;
	ULONG					NumEntriesRemoved;
#endif

	this->m_activeworkers += 1;

	while (true)
	{
#if _MUE_GCQSEX_ == 1
		bSuccess = GetQueuedCompletionStatusEx(
			CompletionPort,
			CPEntry,
			MaxEntries,
			&NumEntriesRemoved,
			INFINITE,
			TRUE
		);

		if (!bSuccess)
		{
			DWORD dwError = ::GetLastError();
			if (dwError == WAIT_IO_COMPLETION)
			{
				this->addlog(emuelogtype::eDEBUG, "%s(), GetQueueCompletionStatusEx Terminated (APC).", __func__);
				break;
			}
			else {
				this->addlog(emuelogtype::eERROR, "%s(), GetQueueCompletionStatusEx : ErrorCode: %d", __func__, dwError);
			}
		}
#else
		bSuccess = GetQueuedCompletionStatus(
			this->m_completionport,
			&dwIoSize,
			(PULONG_PTR)&event_id,
			&lpOverlapped,
			INFINITE
		);

		if (!bSuccess)
		{
			if (lpOverlapped != NULL)
			{
				int aError = GetLastError();

				if ((aError != ERROR_NETNAME_DELETED) && (aError != ERROR_CONNECTION_ABORTED) &&
					(aError != ERROR_OPERATION_ABORTED) && (aError != ERROR_SEM_TIMEOUT) && (aError != ERROR_HOST_UNREACHABLE) && (aError != ERROR_CONNECTION_REFUSED)) // Patch
				{
					this->addlog(emuelogtype::eERROR, "%s(), GetQueueCompletionStatus Error: %d", __func__, GetLastError());
					this->m_activeworkers -= 1;
					return;
				}
			}
		}
#endif
#if _MUE_GCQSEX_ == 1
		for (int n = 0; n < NumEntriesRemoved; n++)
#endif
		{
			this->lock();

#if _MUE_GCQSEX_ == 1
			event_id = (int)CPEntry[n].lpCompletionKey;
			lpOverlapped = CPEntry[n].lpOverlapped;
			dwIoSize = CPEntry[n].dwNumberOfBytesTransferred;
#endif

			lpPerSocketContext = this->getctx(event_id);

			if (lpPerSocketContext == NULL)
			{
				this->addlog(emuelogtype::eWARNING, "%s(), event id %d is invalid.", __func__, event_id);
				this->unlock();
				continue;
			}

			lpIOContext = (LPMUE_PIO_CTX)lpOverlapped;

			if (lpIOContext == NULL)
			{
				this->addlog(emuelogtype::eERROR, "%s(), event id %d lpOverlapped is NULL, Error:%d IO:%d", __func__, 
					event_id, GetLastError(), lpPerSocketContext->IOContext[0].IOOperation);

				if (this->m_terrcounts[t_index] >= 10) {
					this->m_activeworkers -= 1;
					this->unlock();
					return;
				}

				this->m_terrcounts[t_index] += 1;
				this->unlock();
				continue;
			}

			this->addlog(emuelogtype::eTEST, "%s(), lpIOContext->IOOperation : %d", __func__, lpIOContext->IOOperation);

			if (lpPerSocketContext->m_type == 0 && (bSuccess == FALSE || (bSuccess == TRUE && dwIoSize == 0)))
			{
				this->addlog(emuelogtype::eWARNING, "%s(),  Event id %d connection Closed, Size == 0 IO:%d", __func__, 
					event_id, lpIOContext->IOOperation);
				this->close(event_id, emuestatus::eSOCKERROR);
				this->unlock();
				continue;
			}

			switch (lpIOContext->IOOperation)
			{
			case emueiotype::eACCEPT_IO:
				this->handleaccept(lpPerSocketContext);
				if (!this->do_acceptex()) {
					this->redo_acceptex();
				}
				break;
			case emueiotype::eCONNECT_IO:
				this->handleconnect(lpPerSocketContext, dwIoSize);
				break;
			case emueiotype::eSEND_IO:
				this->handlesend(lpPerSocketContext, dwIoSize);
				break;
			case emueiotype::eRECV_IO:
				this->handlereceive(lpPerSocketContext, dwIoSize);
				break;
#if _MUE_GCQSEX_ == 0
			case emueiotype::eEXIT_IO:
				this->addlog(emuelogtype::eDEBUG, "%s(), event id %d GetQueueCompletionStatus Terminated (PostQueued).", __func__, event_id);
				this->m_activeworkers -= 1;
				this->unlock();
				return;
#endif
			}

			this->unlock();
		}
	}

	this->m_activeworkers -= 1;
}

void mueiocp::addlog(emuelogtype type, const char* msg, ...)
{
	char szBuffer[1024] = { 0 };
	va_list pArguments;

	if (((DWORD)type & this->m_logverboseflags) != (DWORD)type)
		return;

	if (this->fnc_loghandler == NULL) {
		sprintf(szBuffer, "[MUEIOCP] (%d) ", type);
		va_start(pArguments, msg);
		size_t iSize = strlen(szBuffer);
		vsprintf_s(&szBuffer[iSize], 1024 - iSize, msg, pArguments);
		va_end(pArguments);
		OutputDebugStringA(szBuffer);
	}
	else {
		va_start(pArguments, msg);
		vsprintf_s(szBuffer, 1024, msg, pArguments);
		va_end(pArguments);
		this->fnc_loghandler(type, szBuffer);
	}
}

SOCKET mueiocp::getsocket(int event_id) {
	LPMUE_PS_CTX _ctx = this->getctx(event_id); 
	if (_ctx == NULL)
		return -1;
	return _ctx->m_socket;
}

char* mueiocp::getipaddr(int event_id) {
	LPMUE_PS_CTX _ctx = this->getctx(event_id);
	if (_ctx == NULL)
		return &g_fakebuf[0];
	return &_ctx->m_ipaddr[0];
}

void mueiocp::setindex(int event_id, intptr_t index) {
	LPMUE_PS_CTX _ctx = this->getctx(event_id);
	if (_ctx == NULL)
		return;
	_ctx->m_index = index;
}

intptr_t mueiocp::getindex(int event_id) {
	LPMUE_PS_CTX _ctx = this->getctx(event_id);
	if (_ctx == NULL)
		return -1;
	return _ctx->m_index;
}

void __stdcall pAPC(ULONG_PTR param)
{
	mueiocp* _this = (mueiocp*)param;
	//_this->addlog(emuelogtype::eDEBUG, "APC Called.");
}

