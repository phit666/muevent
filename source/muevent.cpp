#include "muevent.h"

char g_dummybuf[16] = { 0 };

mueventbase* mueventnewbase(int cpucorenum, mue_loghandler loghandler, DWORD logverboseflags) {
	mueiocp* muebase = new mueiocp;
	muebase->init(cpucorenum, loghandler, logverboseflags);
	return (mueventbase*)muebase;
}

void mueventlisten(mueventbase* base, WORD port, mueventacceptcb acceptcb) {
	mueiocp* iocp = (mueiocp*)base;
	iocp->listen(port, acceptcb, NULL);
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

void mueventsetcb(mueventbase* base, int event_id, mueventreadcb readcb, mueventeventcb eventcb, LPVOID arg) {
	mueiocp* iocp = (mueiocp*)base;
	iocp->setconnectcb(event_id, readcb, eventcb, arg);
}

bool mueventwrite(mueventbase* base, int event_id, LPBYTE lpMsg, DWORD dwSize) {
	mueiocp* iocp = (mueiocp*)base;
	iocp->lock();
	if (!iocp->iseventidvalid(event_id))
		return false;
	bool bret = iocp->sendbuffer(event_id, lpMsg, dwSize);
	iocp->unlock();
	return bret;
}

size_t mueventread(mueventbase* base, int event_id, char* buffer, size_t buffersize) {
	mueiocp* iocp = (mueiocp*)base;
	iocp->lock();
	if (!iocp->iseventidvalid(event_id))
		return 0;
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

void mueventclose(mueventbase* base, int event_id) {
	mueiocp* iocp = (mueiocp*)base;
	iocp->lock();
	iocp->close(event_id);
	iocp->unlock();
}

char* mueventgetipaddr(mueventbase* base, int event_id) {
	mueiocp* iocp = (mueiocp*)base;
	if (!iocp->iseventidvalid(event_id)) {
		return &g_dummybuf[0];
	}
	return iocp->getipaddr(event_id);
}

SOCKET mueventgetsocket(mueventbase* base, int event_id) {
	mueiocp* iocp = (mueiocp*)base;
	if (!iocp->iseventidvalid(event_id)) {
		return INVALID_SOCKET;
	}	return iocp->getsocket(event_id);
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





