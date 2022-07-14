# MUEVENT
MuEngine IOCP C++ library with Accept/Read/Event callback interface.

Reference link: http://www.muengine.org/libmuevent/globals_func.html

	muevent is a wrapper to mueiocp class in procedural style, designed to simplify 
	the implementation of AcceptEx, ConnectEx and GetQueuedCompletionStatusEx windows API.

	Interface Callbacks:
		mueventacceptcb - Accept/Listen callback to process new connection
		mueventreadcb - Read callback to process received data
		mueventeventcb - Event callback to process connection status



