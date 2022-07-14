#pragma once
#include "mue.h"
#include "mueconfig.h"
/**
	@version mueiocp 2.00.00
*/
#define _MUEIOCP_MAJOR_VER_ 0x02
#define _MUEIOCP_MINOR_VER_ 0x00
#define _MUEIOCP_PATCH_VER_ 0x01

/**
	Toggle the use of GetQueuedCompletionStatusEx or GetQueuedCompletionStatus
		1 - GetQueuedCompletionStatusEx
		0 - GetQueuedCompletionStatus
*/
#define _MUE_GCQSEX_ 0

class mueiocp
{
public:
	mueiocp();
	~mueiocp();
	void init(int cpucorenum, mue_loghandler loghandler=NULL, DWORD logverboseflags = -1,
		size_t initclt2ndbufsize = NULL, size_t initsvr2ndbufsize = NULL);
	void dispatch(bool wait = true);
	void dispatchbreak();
	void listen(int listenport, mueventacceptcb acceptcb, LPVOID arg);
	void setconnectcb(MUE* mue, mueventreadcb readcb, mueventeventcb eventcb, LPVOID arg = NULL);
	MUE* makeconnect(const char* ipaddr, WORD port, int index, mue_datahandler datahandler = NULL);
	bool connect(MUE* mue, char* initData, int initLen);
	bool sendbuffer(MUE* mue, LPBYTE lpMsg, DWORD dwSize);
	size_t readbuffer(MUE* mue, char* buffer, size_t buffersize);
	void close(MUE* mue, emuestatus flag = emuestatus::eCLOSED);
	void free(MUE* mue);
	void remove(MUE* mue);
	bool delayconnect(MUE* mue, DWORD mseconds);
	SOCKET getsocket(MUE* mue);
	char* getipaddr(MUE* mue);
	void setindex(MUE* mue, intptr_t index);
	intptr_t getindex(MUE* mue);
	bool isvalid(MUE* mue);
	void lock();
	void unlock();
	void addlog(emuelogtype type, const char* msg, ...);

private:

	std::thread *m_t[MUE_MAX_IOCP_WORKERS];
	void IOCPServerWorker(LPVOID CompletionPortID);
	mue_loghandler fnc_loghandler;

	bool inithelperfunc();
	SOCKET createsocket();
	int createlistensocket(WORD port);
	bool updatecompletionport(SOCKET socket, MUE *mue);
	bool IoSendSecond(MUE* mue);
	bool IoMoreSend(MUE* mue);

	bool do_acceptex();
	bool redo_acceptex();
	bool handleaccept(MUE* mue);
	bool handleconnect(MUE* mue, DWORD dwIoSize);
	bool handlesend(MUE* mue, DWORD dwIoSize);
	bool handlereceive(MUE* mue, DWORD dwIoSize);
	void clear();
	void postqueued();


	mueventacceptcb m_acceptcb;
	LPVOID m_acceptarg;

	LPFN_CONNECTEX m_fnConnectEx;
	LPFN_ACCEPTEX m_fnAcceptEx;
	LPFN_GETACCEPTEXSOCKADDRS m_fnGetAcceptExAddr;
	SOCKET m_listensocket;
	HANDLE m_completionport;

	LPMUE_PS_CTX m_acceptctx;
	MUE* m_acceptmue;

	size_t m_initcltextbuffsize;
	size_t m_initsvrextbuffsize;

	std::vector<MUE*>m_vMUE;

	CRITICAL_SECTION m_crit;

	DWORD m_logverboseflags;
	int m_workers;

	HANDLE m_event;

	WORD m_listenport;
};



