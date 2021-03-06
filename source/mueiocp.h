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
#include "mue.h"
#include "mueconfig.h"
/**
	@version mueiocp 3.x.x
*/
#define _MUEIOCP_MAJOR_VER_ 0x03
#define _MUEIOCP_MINOR_VER_ 0x06
#define _MUEIOCP_PATCH_VER_ 0x05


class mueiocp
{
public:
	mueiocp();
	~mueiocp();
	void init(int cpucorenum, mue_loghandler loghandler=NULL, DWORD logverboseflags = -1,
		size_t initclt2ndbufsize = NULL, size_t initsvr2ndbufsize = NULL);
	void dispatch(bool wait = true);
	void dispatchbreak();
	void listen(int listenport, mueventacceptcb acceptcb, LPVOID arg, char* listenip=NULL);
	void setacceptcbargument(LPVOID arg);
	void setreadeventcbargument(int event_id, LPVOID arg);
	void setconnectcb(int event_id, mueventreadcb readcb, mueventeventcb eventcb, LPVOID arg = NULL);
	int makeconnect(const char* ipaddr, WORD port, intptr_t index, LPMUE_PS_CTX ctx=NULL);
	bool connect(int event_id, char* initData, int initLen);
	bool sendbuffer(int event_id, LPBYTE lpMsg, DWORD dwSize);
	size_t readbuffer(int event_id, char* buffer, size_t buffersize);
	void closesocket(int event_id);
	SOCKET getsocket(int event_id);
	char* getipaddr(int event_id);
	void setindex(int event_id, intptr_t index);
	intptr_t getindex(int event_id);
	void lock();
	void unlock();
	bool iseventidvalid(int event_id);
	void addlog(emuelogtype type, const char* msg, ...);

	bool setctx(int event_id, LPMUE_PS_CTX ctx);
	void enablecustomctx() { this->m_customctx = true; }

	LPMUE_PS_CTX getctx(int event_id);
	void deletectx(LPMUE_PS_CTX ctx);
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
	void close(int event_id, emuestatus flag = emuestatus::eCLOSED);
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
	
	DWORD m_listenip;
	WORD m_listenport;
	int m_eventid;

	int m_activeworkers;

	bool m_customctx;
};



