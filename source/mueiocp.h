#pragma once
#include "mue.h"
#include "mueconfig.h"
/**
	@version mueiocp 3.00.00
*/
#define _MUEIOCP_MAJOR_VER_ 0x03
#define _MUEIOCP_MINOR_VER_ 0x01
#define _MUEIOCP_PATCH_VER_ 0x03


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
	void setconnectcb(int event_id, mueventreadcb readcb, mueventeventcb eventcb, LPVOID arg = NULL);
	int makeconnect(const char* ipaddr, WORD port, intptr_t index, mue_datahandler datahandler = NULL);
	bool connect(int event_id, char* initData, int initLen);
	bool sendbuffer(int event_id, LPBYTE lpMsg, DWORD dwSize);
	size_t readbuffer(int event_id, char* buffer, size_t buffersize);
	void close(int event_id, emuestatus flag = emuestatus::eCLOSED);
	SOCKET getsocket(int event_id);
	char* getipaddr(int event_id);
	void setindex(int event_id, intptr_t index);
	intptr_t getindex(int event_id);
	void lock();
	void unlock();
	bool iseventidvalid(int event_id);
	void addlog(emuelogtype type, const char* msg, ...);

	LPMUE_PS_CTX getctx(int event_id);

	int getactiveioworkers() { return this->m_activeworkers; }

private:

	std::thread *m_t[MUE_MAX_IOCP_WORKERS];
	int m_terrcounts[MUE_MAX_IOCP_WORKERS];
	intptr_t m_tindex;
	void IOCPServerWorker(LPVOID arg);
	mue_loghandler fnc_loghandler;

	bool inithelperfunc();
	SOCKET createsocket();
	int createlistensocket(WORD port);
	bool updatecompletionport(SOCKET socket, int event_id);
	bool IoSendSecond(LPMUE_PS_CTX ctx);
	bool IoMoreSend(LPMUE_PS_CTX ctx);

	bool do_acceptex();
	bool redo_acceptex();
	bool handleaccept(LPMUE_PS_CTX ctx);
	bool handleconnect(LPMUE_PS_CTX ctx, DWORD dwIoSize);
	bool handlesend(LPMUE_PS_CTX ctx, DWORD dwIoSize);
	bool handlereceive(LPMUE_PS_CTX ctx, DWORD dwIoSize);
	void clear();
	void postqueued();
	void remove(int event_id);

	int geteventid();


	mueventacceptcb m_acceptcb;
	LPVOID m_acceptarg;

	LPFN_CONNECTEX m_fnConnectEx;
	LPFN_ACCEPTEX m_fnAcceptEx;
	LPFN_GETACCEPTEXSOCKADDRS m_fnGetAcceptExAddr;
	SOCKET m_listensocket;
	HANDLE m_completionport;

	LPMUE_PS_CTX m_acceptctx;
	int m_accepteventid;

	size_t m_initcltextbuffsize;
	size_t m_initsvrextbuffsize;

	std::map<int, LPMUE_PS_CTX>m_mueventmaps;

	CRITICAL_SECTION m_crit;

	DWORD m_logverboseflags;
	int m_workers;

	HANDLE m_event;

	WORD m_listenport;
	int m_eventid;

	int m_activeworkers;
};



