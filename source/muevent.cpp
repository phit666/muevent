#include "muevent.h"

char g_dummybuf[16] = { 0 };

mueventbase* mueventnewbase(int cpucorenum, mue_loghandler loghandler, DWORD logverboseflags, int connect2ndbuffer) {
	mueiocp* muebase = new mueiocp;
	muebase->init(cpucorenum, loghandler, logverboseflags, 0, connect2ndbuffer);
	return (mueventbase*)muebase;
}

void mueventlisten(mueventbase* base, WORD port, mueventacceptcb acceptcb, LPVOID arg, char* listenip) {
	mueiocp* iocp = (mueiocp*)base;
	iocp->listen(port, acceptcb, arg, listenip);
}

void mueventsetacceptcbargument(mueventbase* base, LPVOID arg) {
	mueiocp* iocp = (mueiocp*)base;
	iocp->setacceptcbargument(arg);
}

void mueventsetreadeventcbargument(mueventbase* base, int event_id, LPVOID arg) {
	mueiocp* iocp = (mueiocp*)base;
	iocp->setreadeventcbargument(event_id, arg);
}

void mueventdispatch(mueventbase* base, bool block) {
	mueiocp* iocp = (mueiocp*)base;
	iocp->dispatch(block);
}

int mueventconnect(mueventbase* base, const char* ipaddr, WORD port, char* initbuf, int initlen) {
	mueiocp* iocp = (mueiocp*)base;
	int eventid = iocp->makeconnect(ipaddr, port, 0);
	LPMUE_PS_CTX ctx = iocp->getctx(eventid);
	if (ctx == NULL) {
		iocp->addlog(emuelogtype::eWARNING, "%s(), makeconnect failed.", __func__);
		return NULL;
	}
	if (initbuf == NULL) {
		iocp->addlog(emuelogtype::eWARNING, "%s(), initbuf is NULL.", __func__);
		return NULL;
	}
	ctx->_this = (LPVOID)iocp;
	iocp->connect(eventid, initbuf, initlen);
	return eventid;
}

int mueventmakeconnect(mueventbase* base, const char* ipaddr, WORD port) {
	mueiocp* iocp = (mueiocp*)base;
	int eventid = iocp->makeconnect(ipaddr, port, 0);
	LPMUE_PS_CTX ctx = iocp->getctx(eventid);
	if (ctx == NULL) {
		iocp->addlog(emuelogtype::eWARNING, "%s(), mueventmakeconnect failed.", __func__);
		return NULL;
	}
	ctx->_this = (LPVOID)iocp;
	return eventid;
}

bool mueventconnect(mueventbase* base, int eventid, char* initbuf, int initlen) {
	mueiocp* iocp = (mueiocp*)base;
	return iocp->connect(eventid, initbuf, initlen);
}

bool mueventisconnected(mueventbase* base, int eventid) {
	mueiocp* iocp = (mueiocp*)base;
	iocp->lock();
	LPMUE_PS_CTX ctx = iocp->getctx(eventid);
	if (ctx == NULL) {
		iocp->unlock();
		return false;
	}
	bool bret = ctx->m_connected;
	iocp->unlock();
	return bret;
}

void mueventsetcb(mueventbase* base, int event_id, mueventreadcb readcb, mueventeventcb eventcb, LPVOID arg) {
	mueiocp* iocp = (mueiocp*)base;
	iocp->setconnectcb(event_id, readcb, eventcb, arg);
}

bool mueventwrite(mueventbase* base, int event_id, LPBYTE lpMsg, DWORD dwSize) {
	mueiocp* iocp = (mueiocp*)base;
	iocp->lock();
	bool bret = iocp->sendbuffer(event_id, lpMsg, dwSize);
	iocp->unlock();
	return bret;
}

size_t mueventread(mueventbase* base, int event_id, char* buffer, size_t buffersize) {
	mueiocp* iocp = (mueiocp*)base;
	iocp->lock();
	size_t size = iocp->readbuffer(event_id, buffer, buffersize);
	iocp->unlock();
	return size;
}

void mueventdispatchbreak(mueventbase* base) {
	mueiocp* iocp = (mueiocp*)base;
	iocp->dispatchbreak();
}

void mueventlock(mueventbase* base) {
	mueiocp* iocp = (mueiocp*)base;
	iocp->lock();
}

void mueventunlock(mueventbase* base) {
	mueiocp* iocp = (mueiocp*)base;
	iocp->unlock();
}

void mueventbasedelete(mueventbase* base) {
	mueiocp* iocp = (mueiocp*)base;
	delete iocp;
}

void mueventclosesocket(mueventbase* base, int event_id) {
	mueiocp* iocp = (mueiocp*)base;
	iocp->lock();
	iocp->closesocket(event_id);
	iocp->unlock();
}

char* mueventgetipaddr(mueventbase* base, int event_id) {
	mueiocp* iocp = (mueiocp*)base;
	return iocp->getipaddr(event_id);
}

SOCKET mueventgetsocket(mueventbase* base, int event_id) {
	mueiocp* iocp = (mueiocp*)base;
	iocp->lock();
	SOCKET s = iocp->getsocket(event_id);
	iocp->unlock();
	return s;
}

void mueventsetindex(mueventbase* base, int event_id, intptr_t userindex){
	mueiocp* iocp = (mueiocp*)base;
	iocp->lock();
	iocp->setindex(event_id, userindex);
	iocp->unlock();
}

intptr_t mueventgetindex(mueventbase* base, int event_id) {
	mueiocp* iocp = (mueiocp*)base;
	iocp->lock();
	intptr_t index = iocp->getindex(event_id);
	iocp->unlock();
	return index;
}

bool mueventisvalid(mueventbase* base, int event_id) {
	mueiocp* iocp = (mueiocp*)base;
	iocp->lock();
	bool bret = iocp->iseventidvalid(event_id);
	iocp->unlock();
	return bret;
}

int mueventactiveioworkers(mueventbase* base) {
	mueiocp* iocp = (mueiocp*)base;
	return iocp->getactiveioworkers();
}


void mueventaddlog(mueventbase* base, emuelogtype type, const char* msg, ...) {
	char szBuffer[1024] = { 0 };
	va_list pArguments;
	mueiocp* iocp = (mueiocp*)base;
	va_start(pArguments, msg);
	vsprintf_s(szBuffer, 1024, msg, pArguments);
	va_end(pArguments);
	iocp->addlog(type, szBuffer);
}





