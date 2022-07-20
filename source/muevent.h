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
#include "mueiocp.h"

/**
	@file muevent.h

	muevent is a wrapper to mueiocp class in procedural style, designed to simplify
	the implementation of AcceptEx, ConnectEx and GetQueuedCompletionStatusEx windows API.

	Interface Callbacks:
		mueventacceptcb - Accept/Listen callback to process new connection
		mueventreadcb - Read callback to process received data
		mueventeventcb - Event callback to process connection status
*/

/**
	stProxyInfo struct store proxy info.
	@param proxyport			Proxy server listening port for client connection.
	@param remoteport			Remote server listening port  where the proxy server will connect to.
	@param proxyhost			Proxy server listening host (FQDN) or IP address.
	@param remotehost			Remote server listening host (FQDN) or IP address.
*/
typedef struct _stmueventProxyInfo {
	_stmueventProxyInfo()
	{
		proxyport = -1;
		remoteport = -1;
		memset(proxyhost, 0, 50);
		memset(remotehost, 0, 50);
		base = NULL;
		proxyreadcb = NULL;
		remotereadcb = NULL;
	}
	int proxyport;
	int remoteport;
	char proxyhost[50];
	char remotehost[50];
	mueventbase* base;
	mueventproxyreadcb proxyreadcb;
	mueventproxyreadcb remotereadcb;
} stmueventProxyInfo, * LpstmueventProxyInfo;

/**
	stmueventConnectInfo struct store the newly connected IP address and socket, this is only use when mueventenablecustomcontext is called 
	because when the custom context is provided the IP address and socket info will not available in the custom context inside the 
	accept callback. LpstmueventProxyInfo pointer should be passed as accept callback's argument to mueventlisten call then inside the accept
	callback cast the argument to LpstProxyInfo to fetch the IP address and socket info.

	@param s				Socket info of the newly connected client.
	@param ipaddr			IP address of the newly connected client.
*/
typedef struct _stmueventConnectInfo {
	SOCKET s;
	char ipaddr[16];
} stmueventConnectInfo, * LPstmueventConnectInfo;


/**
		Create a new muevent base.

		@param cpucorenum		Number of CPU, it's multiplied by 2 so 2 will spawn
								4 thread workers, set it to 0 to automatically get
								the machine's CPU count.
		@param loghandler		The log callback function, it will default to
								OutputDebugString when set to NULL.
		@param logverboseflags	Bit flags of enum class emuelogtype for filtering
								type of log to be shown.
		@param connect2ndbuffer	This is a dynamic buffer storage that will expand as required by the data allocation size needed to be sent. 
		@return					muevent base.
*/
mueventbase * mueventnewbase(int cpucorenum=0, mue_loghandler loghandler=0, DWORD logverboseflags=-1, int connect2ndbuffer=0);

/**
	Start accepting connections.
	@param base					muevent base from mueventnewbase call.
	@param port					Listen port of this server.
	@param acceptcb				Pointer to Accept callback function of type mueventacceptcb for
								processing client newly connected.
	@param arg					Variable  you want to pass to Accept callback.
*/
void mueventlisten(mueventbase* base, WORD port, mueventacceptcb acceptcb, LPVOID arg=0, char* listenip=NULL);

/**
	Connect this server to another server.

	@param base					muevent base from mueventnewbase call.
	@param ipaddr				Server IP address to connect to.
	@param port					Server port to connect to.
	@param initbuf				Optional initial data to send to server, this can't be NULL and user is responsible in allocating memory for this variable.
	@param initlen				Size of optional data to send to server, set to 0 if initbuf has no data.
	@param ctx					User provided pointer to MUE_PS_CTX struct, this is required if mueventenablecustomcontext has been called.
	@return						-1 on failure and muevent event id on success.
*/
int mueventconnect(mueventbase* base, const char* ipaddr, WORD port, char *initbuf, int initlen, LPMUE_PS_CTX ctx=NULL);

/**
	Make the connection setup only but don't connect.
	@param base					muevent base from mueventnewbase call.
	@param ipaddr				Server IP address to connect to.
	@param port					Server port to connect to.
	@param ctx					User provided pointer to MUE_PS_CTX struct, this is required if mueventenablecustomcontext has been called.
	@return						-1 on failure and muevent event id on success.
*/
int mueventmakeconnect(mueventbase* base, const char* ipaddr, WORD port, LPMUE_PS_CTX ctx=NULL);

/**
	Connect to another server based on the parameters of mueventmakeconnect call.
	@param base					muevent base from mueventnewbase call.
	@param event_id				muevent event id from mueventmakeconnect call.
	@param initbuf				Optional initial data to send to server, this can't be NULL and user is responsible in allocating memory for this variable.
	@param initlen				Size of optional data to send to server, set to 0 if initbuf has no data.
	@return						true on success and false on failure.
*/
bool mueventconnect(mueventbase* base, int eventid, char* initbuf, int initlen);

/**
	Check if the call to mueventconnect really made, call to mueventconnect will return true immediately if there has no socket error but the IO is still
	pending, calling mueventisconnected will get the real connection status. 
	@param base					muevent base from mueventnewbase call.
	@param event_id				muevent event id from mueventmakeconnect call.
	@return						true if connected and false if not.
*/
bool mueventisconnected(mueventbase* base, int eventid);

/**
	Start mueevent.

	@param base					muevent base from mueventnewbase call.
	@param block				mueventdispatch call will block when true otherwise return
								immediately.
*/
void mueventdispatch(mueventbase* base, bool block);

/**
	Set the read and event callback of muevent object.

	@param base					muevent base from mueventnewbase call.
	@param event_id				muevent event id.
	@param readcb				Read callback function of type mueventreadcb for
								processing received data.
	@param eventcb				Event callback of type mueventeventcb for processing
								connection status.
	@param arg					Variable you want to pass to read and event callback.
*/
void mueventsetcb(mueventbase* base, int event_id, mueventreadcb readcb, mueventeventcb eventcb, LPVOID arg = NULL);

/**
	Set the read and event callback of muevent object.
	@param base					muevent base from mueventnewbase call.
	@param arg					Variable you want to pass to accept callback.
*/
void mueventsetacceptcbargument(mueventbase* base, LPVOID arg);

/**
	Set the read and event callback argument.
	@param base					muevent base from mueventnewbase call.
	@param event_id				muevent event id.
	@param arg					Variable you want to pass to accept callback.
*/
void mueventsetreadeventcbargument(mueventbase* base, int event_id, LPVOID arg);


/**
	Attempt and immediate sending of data, if not possible unsent data will be moved to dynamic buffer
	for later sending.

	@param base					muevent base from mueventnewbase call.
	@param event_id				muevent event id.
	@param lpMsg				Data to send.
	@param dwSize				Size of data to be sent
	@return						false when failed otherwise true if the attempt is successfull
*/
bool mueventwrite(mueventbase* base, int event_id, LPBYTE lpMsg, DWORD dwSize);

/**
	Read received data, call this inside the read callback.

	@param base					muevent base from mueventnewbase call.
	@param event_id				muevent event id.
	@param buffer				Store the data to be read
	@param dwSize				Size of data to be read
	@return						Size of data read otherwise 0 when there is error.
*/
size_t mueventread(mueventbase* base, int event_id, char* buffer, size_t buffersize);

/**
	Return mueventdispatch blocking call.
	@param base					muevent base from mueventnewbase call.
*/
void mueventdispatchbreak(mueventbase* base);

/**
	Log a message and categorize it with enum class emuelogtype.
	@param mue					muevent object.
	@param type					Log category from enum class emuelogtype
	@param msg					Log message.
*/
void mueventaddlog(mueventbase* base, emuelogtype type, const char* msg, ...);


/**
	Stop muevent and clean all the memory used.
	@param base					muevent base from mueventnewbase call.
*/
void mueventbasedelete(mueventbase* base);


/**
	Close muevent object socket connection.
	@param base					muevent base from mueventnewbase call.
	@param event_id				muevent event id.
*/
void mueventclosesocket(mueventbase* base, int event_id);

/**
	Thread safe to muevent, all calls after this is exclussive as the rest of the thread accessing this muevent will be in wait state.
	@param base					muevent object.
*/
void mueventlock(mueventbase* base);

/**
	Release the exclussive access to muevent.
	@param base					muevent base from mueventnewbase call.
*/
void mueventunlock(mueventbase* base);

/**
	Return muevent object's IP address.
	@param base					muevent base from mueventnewbase call.
	@param event_id				muevent event id.
	@return						IP address on success and empty buffer on failure.
*/
char* mueventgetipaddr(mueventbase* base, int event_id);

/**
	Return muevent object's socket.
	@param base					muevent base from mueventnewbase call.
	@param event_id				muevent event id.
	@return						socket on success and -1 on failure.
*/
SOCKET mueventgetsocket(mueventbase* base, int event_id);

/**
	Set a user index to muevent context.
	@param base					muevent base from mueventnewbase call.
	@param event_id				muevent event id.
	@param userindex			User index to set in muevent's context.
*/
void mueventsetindex(mueventbase* base, int event_id, intptr_t userindex);

/**
	Get the user index from muevent context.
	@param base					muevent base from mueventnewbase call.
	@param event_id				muevent event id.
	@return						-1 if not set or the user index when set.
*/
intptr_t mueventgetindex(mueventbase* base, int event_id);

/**
	Check if muevent event id is valid.
	@param base					muevent base from mueventnewbase call.
	@param event_id				muevent event id.
	@return						true if event id is valid otherwise false.
*/
bool mueventisvalid(mueventbase* base, int event_id);

/**
	Get the active IO thread workers.
	@param base					muevent base from mueventnewbase call.
	@return						count of active IO thread workers.
*/
int mueventactiveioworkers(mueventbase* base);

/**
	Set the user custom argument.
	@param base					muevent base from mueventnewbase call.
	@param event_id				muevent event id.
	@param arg					Custom argument to pass on event id.
*/
bool mueventsetcustomarg(mueventbase* base, int event_id, LPVOID arg);

/**
	Get the user custom argument.
	@param base					muevent base from mueventnewbase call.
	@param event_id				muevent event id.
	@return						Pointer to argument on success or 0 on failure.
*/
LPVOID mueventgetcustomarg(mueventbase* base, int event_id);

/**
	Start an instance of proxy server.
	@param proxyinfo			Pointer to struct stmueventProxyInfo.
	@param return				true on success and false on failure.
*/
bool mueventrunproxyserver(LpstmueventProxyInfo proxyinfo);

/**
	Enable the user to provide a pointer to MUE_PS_CTX struct.
	@param base					muevent base from mueventnewbase call.
*/
void mueventenablecustomcontext(mueventbase* base);

/**
	Set the user provided MUE_PS_CTX struct pointer with the event id, this can only be called inside accept callback. 
	@param base					muevent base from mueventnewbase call.
	@param event_id				muevent event id.
	@param ctx					User created pointer to MUE_PS_CTX struct.
	@return						true on success or false on failure.
*/
bool mueventsetcustomcontext(mueventbase* base, int event_id, LPMUE_PS_CTX ctx);

/**
	Delete the user provided MUE_PS_CTX struct pointer, this must be called by the user upon exit. 
	@param base					muevent base from mueventnewbase call.
	@param ctx					User created pointer to MUE_PS_CTX struct.
*/
void mueventdelcustomcontext(mueventbase* base, LPMUE_PS_CTX ctx);
