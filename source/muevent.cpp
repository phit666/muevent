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

muevent* mueventconnect(mueventbase* base, const char* ipaddr, WORD port, char* initbuf, int initlen) {
	mueiocp* iocp = (mueiocp*)base;
	MUE* mue = iocp->makeconnect(ipaddr, port, 0);
	LPMUE_PS_CTX ctx = (LPMUE_PS_CTX)mue->_ctx;
	if (ctx == NULL) {
		iocp->addlog(emuelogtype::eWARNING, "%s(), ctx is NULL.", __func__);
		return NULL;
	}
	if (initbuf == NULL) {
		iocp->addlog(emuelogtype::eWARNING, "%s(), initbuf is NULL.", __func__);
		return NULL;
	}
	ctx->_this = (LPVOID)iocp;
	iocp->connect(mue, initbuf, initlen);
	return (muevent*)mue;
}

void mueventsetcb(muevent* mue, mueventreadcb readcb, mueventeventcb eventcb, LPVOID arg) {
	MUE* _mue = (MUE*)mue;
	LPMUE_PS_CTX ctx = (LPMUE_PS_CTX)_mue->_ctx;
	mueiocp* iocp = (mueiocp*)ctx->_this;
	iocp->setconnectcb(_mue, readcb, eventcb, arg);
}

bool mueventwrite(muevent* mue, LPBYTE lpMsg, DWORD dwSize) {
	MUE* _mue = (MUE*)mue;
	LPMUE_PS_CTX ctx = (LPMUE_PS_CTX)_mue->_ctx;
	if (ctx == NULL)
		return false;
	mueiocp* iocp = (mueiocp*)ctx->_this;
	iocp->lock();
	bool bret = iocp->sendbuffer(_mue, lpMsg, dwSize);
	iocp->unlock();
	return bret;
}

size_t mueventread(muevent* mue, char* buffer, size_t buffersize) {
	MUE* _mue = (MUE*)mue;
	LPMUE_PS_CTX ctx = (LPMUE_PS_CTX)_mue->_ctx;
	if (ctx == NULL)
		return 0;
	mueiocp* iocp = (mueiocp*)ctx->_this;
	iocp->lock();
	size_t size = iocp->readbuffer(_mue, buffer, buffersize);
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

void mueventclose(muevent* mue) {
	MUE* _mue = (MUE*)mue;
	LPMUE_PS_CTX ctx = (LPMUE_PS_CTX)_mue->_ctx;
	if (ctx == NULL)
		return;
	mueiocp* iocp = (mueiocp*)ctx->_this;
	iocp->lock();
	iocp->close(_mue);
	iocp->unlock();
}

void mueventdelete(muevent* mue) {
	MUE* _mue = (MUE*)mue;
	LPMUE_PS_CTX ctx = (LPMUE_PS_CTX)_mue->_ctx;
	if (ctx == NULL)
		return;
	mueiocp* iocp = (mueiocp*)ctx->_this;
	iocp->lock();
	iocp->close(_mue, emuestatus::eNOEVENCB);
	iocp->free(_mue);
	iocp->unlock();
}

char* mueventgetipaddr(muevent* mue) {
	MUE* _mue = (MUE*)mue;
	LPMUE_PS_CTX ctx = (LPMUE_PS_CTX)_mue->_ctx;
	if (ctx == NULL) {
		return &g_dummybuf[0];
	}
	mueiocp* iocp = (mueiocp*)ctx->_this;
	return iocp->getipaddr(_mue);
}

SOCKET mueventgetsocket(muevent* mue) {
	MUE* _mue = (MUE*)mue;
	LPMUE_PS_CTX ctx = (LPMUE_PS_CTX)_mue->_ctx;
	if (ctx == NULL)
		return INVALID_SOCKET;
	mueiocp* iocp = (mueiocp*)ctx->_this;
	return iocp->getsocket(_mue);
}

mueventbase* mueventgetbase(muevent* mue) {
	MUE* _mue = (MUE*)mue;
	LPMUE_PS_CTX ctx = (LPMUE_PS_CTX)_mue->_ctx;
	if (ctx == NULL)
		return 0;
	return (mueiocp*)ctx->_this;
}

void mueventaddlog(muevent* mue, emuelogtype type, const char* msg, ...) {
	MUE* _mue = (MUE*)mue;
	char szBuffer[1024] = { 0 };
	va_list pArguments;

	LPMUE_PS_CTX ctx = (LPMUE_PS_CTX)_mue->_ctx;
	if (ctx == NULL) {
		return;
	}

	va_start(pArguments, msg);
	vsprintf_s(szBuffer, 1024, msg, pArguments);
	va_end(pArguments);
	mueiocp* iocp = (mueiocp*)ctx->_this;
	iocp->addlog(type, szBuffer);
}





