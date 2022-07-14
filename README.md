# MUEVENT
MuEngine IOCP C++ library with Accept/Read/Event callback interface.

Reference link: http://www.muengine.org/libmuevent/globals_func.html

	muevent is a wrapper to mueiocp class in procedural style, designed to simplify 
	the implementation of AcceptEx, ConnectEx and GetQueuedCompletionStatusEx windows API.

	Interface Callbacks:
		mueventacceptcb - Accept/Listen callback to process new connection
		mueventreadcb - Read callback to process received data
		mueventeventcb - Event callback to process connection status

	Echo-Server in few lines.

	static bool acceptcb(muevent* mue, LPVOID arg);
	static bool readcb(muevent* mue, LPVOID arg);
	static void eventcb(muevent* mue, emuestatus type);

	void main()
	{
		mueventbase* base = mueventnewbase();
		mueventlisten(base, 3000, acceptcb);

		mueventdispatch(base, true); // true is blocking call, need to call mueventdispatchbreak somewhere to return this call
		
		mueventbasedelete(base);
	}

	static bool acceptcb(muevent* mue, LPVOID arg)
	{
		intptr_t userunique_index_arg;
		mueventsetcb(mue, readcb, eventcb, userunique_index_arg++); // setup callbacks and pass argument to callbacks
		return true;
	}

	static bool readcb(muevent* mue, LPVOID arg)
	{
		intptr_t index = (intptr_t)arg;
		char buff[100] = { 0 };
		int readsize = mueventread(mue, buff, sizeof(buff));
		mueventwrite(mue, (LPBYTE)buff, readsize); // simply echo the read data
		return true;
	}

	static void eventcb(muevent* mue, emuestatus type, LPVOID arg)
	{
		intptr_t index = (intptr_t)arg;

		switch (type) {
		case emuestatus::eCONNECTED:
			break;
		case emuestatus::eCLOSED:
			break;
		case emuestatus::eSOCKERROR:
			mueventdelete(mue);
			break;
		}
	}


