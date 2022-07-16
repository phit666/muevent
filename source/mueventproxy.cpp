#include "muevent.h"
#include <iostream>
#include <conio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sstream>

static bool _proxy_acceptcb(int eventid, LPVOID arg);
static bool _proxy_readcb(int eventid, LPVOID arg);
static bool _proxy_remote_readcb(int eventid, LPVOID arg);

bool mueventproxyserver(LPMuevenProxyInfo proxyinfo, char* proxyip, int proxyport) {
    if (proxyinfo == NULL)
        return false;
    if (proxyinfo->_base == NULL)
        return false;
    if (proxyport < 0 || proxyport > 65535)
        return false;
    if (proxyinfo->_remoteport < 0 || proxyinfo->_remoteport > 65535)
        return false;
    mueventlisten(proxyinfo->_base, proxyport, _proxy_acceptcb, (LPVOID)proxyinfo, proxyip);
    return true;
}

static bool _proxy_acceptcb(int eventid, LPVOID arg) {
    LPMuevenProxyInfo pProxyInfo = (LPMuevenProxyInfo)arg;
    if (pProxyInfo == NULL)
        return false;
    int event_id = mueventmakeconnect(pProxyInfo->_base, pProxyInfo->_remotehost, pProxyInfo->_remoteport); // create the event id of remote connection
    
    intptr_t _event_id = static_cast<intptr_t>(event_id);
    intptr_t _eventid = static_cast<intptr_t>(eventid);
    
    pProxyInfo->_remoteeventid = _event_id;
    pProxyInfo->_proxyeventid = _eventid;
    
    mueventsetcb(pProxyInfo->_base, eventid, _proxy_readcb, NULL, (LPVOID)pProxyInfo); // pass remote event id to client callback
    mueventsetcb(pProxyInfo->_base, event_id, _proxy_remote_readcb, NULL, (LPVOID)pProxyInfo); // pass proxy event id to remote callback
    return true;
}

static bool _proxy_readcb(int eventid, LPVOID arg) {
    LPMuevenProxyInfo pProxyInfo = (LPMuevenProxyInfo)arg;
    if (pProxyInfo == NULL)
        return false;
    int remote_eventid = static_cast<int>(pProxyInfo->_remoteeventid);
    char buf[MUE_CLT_MAX_IO_BUFFER_SIZE] = { 0 };
    size_t size = mueventread(pProxyInfo->_base, eventid, buf, MUE_CLT_MAX_IO_BUFFER_SIZE);
    if (size == 0)
        return false;
    if (!mueventisconnected(pProxyInfo->_base, remote_eventid)) {
        return mueventconnect(pProxyInfo->_base, remote_eventid, buf, size); // connect to remote and send initial buffer
    }
    else {
        return mueventwrite(pProxyInfo->_base, remote_eventid, (LPBYTE)buf, size);
    }
}

static bool _proxy_remote_readcb(int eventid, LPVOID arg) {
    LPMuevenProxyInfo pProxyInfo = (LPMuevenProxyInfo)arg;
    if (pProxyInfo == NULL)
        return false;
    int local_eventid = static_cast<int>(pProxyInfo->_proxyeventid);
    char buf[MUE_CLT_MAX_IO_BUFFER_SIZE] = { 0 };
    size_t size = mueventread(pProxyInfo->_base, eventid, buf, MUE_CLT_MAX_IO_BUFFER_SIZE);
    if (size == 0)
        return false;
    return mueventwrite(pProxyInfo->_base, local_eventid, (LPBYTE)buf, size);
}



