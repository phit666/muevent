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
		Create a new muevent base.

		@param cpucorenum		Number of CPU, it's multiplied by 2 so 2 will spawn
								4 thread workers, set it to 0 to automatically get
								the machine's CPU count.
		@param loghandler		The log callback function, it will default to
								OutputDebugString when set to NULL.
		@param logverboseflags	Bit flags of enum class emuelogtype for filtering
								type of log to be shown.
		@return					muevent base.
*/
mueventbase * mueventnewbase(int cpucorenum=0, mue_loghandler loghandler=0, DWORD logverboseflags=-1);

/**
	Start accepting connections.
	@param base					muevent base from mueventnewbase call.
	@param port					Listen port of this server.
	@param acceptcb				Pointer to Accept callback function of type mueventacceptcb for
								processing client newly connected.
	@param arg					Variable  you want to pass to Accept callback.
*/
void mueventlisten(mueventbase* base, WORD port, mueventacceptcb acceptcb, LPVOID arg=0);

/**
	Connect this server to another server.

	@param base					muevent base from mueventnewbase call.
	@param ipaddr				Server IP address to connect to.
	@param port					Server port to connect to.
	@param initbuf				Optional initial data to send to server, this can't be NULL and user is responsible in allocating memory for this variable.
	@param initlen				Size of optional data to send to server, set to 0 if initbuf has no data.
	@return						-1 on failure and muevent event id on success.
*/
int mueventconnect(mueventbase* base, const char* ipaddr, WORD port, char *initbuf, int initlen);


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
	Close muevent object connection.
	@param base					muevent base from mueventnewbase call.
	@param event_id				muevent event id.
*/
void mueventclose(mueventbase* base, int event_id);

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
