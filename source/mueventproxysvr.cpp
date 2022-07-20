#include "muevent.h"

static bool _proxy_acceptcb(mueventbase* base, int eventid, LPVOID arg);
static bool _proxy_readcb(mueventbase* base, int eventid, LPVOID arg);
static bool _proxy_remote_readcb(mueventbase* base, int eventid, LPVOID arg);


bool mueventrunproxyserver(LpstmueventProxyInfo proxyinfo) {
	if (proxyinfo->base == NULL)
		return false;
	if (proxyinfo->proxyport < 0 || proxyinfo->proxyport > 65535)
		return false;
	if (proxyinfo->remoteport < 0 || proxyinfo->remoteport > 65535)
		return false;
	if (strlen(proxyinfo->proxyhost) < 4 || strlen(proxyinfo->remotehost) < 4 || strcmp(proxyinfo->proxyhost, proxyinfo->remotehost) == 0)
		return false;
	mueventlisten(proxyinfo->base, proxyinfo->proxyport, _proxy_acceptcb, (LPVOID)proxyinfo, proxyinfo->proxyhost);
	return true;
}

static bool _proxy_acceptcb(mueventbase* base, int eventid, LPVOID arg)
{
	LpstmueventProxyInfo proxyinfo = (LpstmueventProxyInfo)arg;

	int event_id = mueventmakeconnect(base, proxyinfo->remotehost, proxyinfo->remoteport); // create the event id of remote connection
	intptr_t _event_id = static_cast<intptr_t>(event_id);
	mueventsetcb(base, eventid, _proxy_readcb, NULL, (LPVOID)_event_id); // pass remote event id to client callback
	intptr_t _eventid = static_cast<intptr_t>(eventid);
	mueventsetcb(base, event_id, _proxy_remote_readcb, NULL, (LPVOID)_eventid); // pass proxy event id to remote callback

	if(proxyinfo->remotereadcb != NULL)
		mueventsetcustomarg(base, event_id, (LPVOID)proxyinfo->remotereadcb);
	if(proxyinfo->proxyreadcb != NULL)
		mueventsetcustomarg(base, eventid, (LPVOID)proxyinfo->proxyreadcb);

	return true;
}

static bool _proxy_readcb(mueventbase* base, int eventid, LPVOID arg)
{
	intptr_t _ptr = (intptr_t)arg;
	int remote_eventid = static_cast<int>(_ptr);
	char buf[MUE_CLT_MAX_IO_BUFFER_SIZE] = { 0 };

	size_t size = mueventread(base, eventid, buf, MUE_CLT_MAX_IO_BUFFER_SIZE);

	if (size == 0)
		return false;

	mueventproxyreadcb readcb = (mueventproxyreadcb)mueventgetcustomarg(base, eventid);

	if (readcb != NULL) {
		readcb(base, buf, size);
	}

	if (!mueventisconnected(base, remote_eventid)) {
		return mueventconnect(base, remote_eventid, buf, size); // connect to remote and send initial buffer
	}
	else {
		return mueventwrite(base, remote_eventid, (LPBYTE)buf, size);
	}
}

static bool _proxy_remote_readcb(mueventbase* base, int eventid, LPVOID arg)
{
	intptr_t _ptr = (intptr_t)arg;
	int local_eventid = static_cast<int>(_ptr);
	char buf[MUE_CLT_MAX_IO_BUFFER_SIZE] = { 0 };

	size_t size = mueventread(base, eventid, buf, MUE_CLT_MAX_IO_BUFFER_SIZE);

	if (size == 0)
		return false;

	mueventproxyreadcb readcb = (mueventproxyreadcb)mueventgetcustomarg(base, eventid);

	if (readcb != NULL) {
		readcb(base, buf, size);
	}

	return mueventwrite(base, local_eventid, (LPBYTE)buf, size);
}