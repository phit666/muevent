// proxy-server.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <muevent.h>
#include <conio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sstream>


static void logger(emuelogtype type, const char* msg);
static bool acceptcb(int eventid, LPVOID arg);
static bool readcb(int eventid, LPVOID arg);
static bool remote_readcb(int eventid, LPVOID arg);
static BOOL WINAPI signalhandler(DWORD signum);
static mueventbase* base = NULL;
static int portProxy;
static int portServer;
static char svrip[16] = { 0 };

int main(int argc, char* argv[])
{
	printf("MuEngine Proxy-Server Version 1.00.00\n");

	if (argc < 3) {
		std::cout << std::endl;
		std::cout << "Usage:" << std::endl;
		std::cout << "proxy-server.exe <listen port> <remote ip> <remote port>" << std::endl;
		std::cout << std::endl;
		std::cout << "Example:" << std::endl;
		std::cout << "proxy-server.exe 3384 10.0.0.1 3389" << std::endl;
		std::cout << std::endl;
		system("pause");
		return -1;
	}

    SetConsoleCtrlHandler(signalhandler, TRUE);

    std::stringstream s;
    s << argv[1]; s >> portProxy; s.clear();
    s << argv[3]; s >> portServer;
    memcpy(&svrip, argv[2], 15);

    base = mueventnewbase(1, logger);
    mueventlisten(base, portProxy, acceptcb, base);
    mueventdispatch(base, true);
    mueventbasedelete(base);
    return _getch();
}

static bool acceptcb(int eventid, LPVOID arg)
{
    mueventlock(arg);

    int event_id = mueventmakeconnect(arg, svrip, portServer); // create the event id of remote connection

    intptr_t _event_id = static_cast<intptr_t>(event_id);
    mueventsetcb(arg, eventid, readcb, NULL, (LPVOID)_event_id); // pass remote event id to client callback

    intptr_t _eventid = static_cast<intptr_t>(eventid);
    mueventsetcb(arg, event_id, remote_readcb, NULL, (LPVOID)_eventid); // pass proxy event id to remote callback

    mueventunlock(arg);
    return true;
}

static bool readcb(int eventid, LPVOID arg)
{
    intptr_t ptr = (intptr_t)arg;
    int remote_eventid = static_cast<int>(ptr);
    char buf[MUE_CLT_MAX_IO_BUFFER_SIZE] = { 0 };

    size_t size = mueventread(base, eventid, buf, MUE_CLT_MAX_IO_BUFFER_SIZE);

    if (size == 0)
        return false;

    if (!mueventisconnected(base, remote_eventid)) {
        return mueventconnect(base, remote_eventid, buf, size); // connect to remote and send initial buffer
    }
    else {
        return mueventwrite(base, remote_eventid, (LPBYTE)buf, size);
    }
}

static bool remote_readcb(int eventid, LPVOID arg)
{
    intptr_t ptr = (intptr_t)arg;
    int local_eventid = static_cast<int>(ptr);
    char buf[MUE_CLT_MAX_IO_BUFFER_SIZE] = { 0 };

    size_t size = mueventread(base, eventid, buf, MUE_CLT_MAX_IO_BUFFER_SIZE);

    if (size == 0)
        return false;

    return mueventwrite(base, local_eventid, (LPBYTE)buf, size);
}

static BOOL WINAPI signalhandler(DWORD signum)
{
    switch (signum)
    {
    case CTRL_C_EVENT:
        mueventdispatchbreak(base); /**we will return dispatch upon Ctrl-C*/
        break;
    }
    return TRUE;
}

static void logger(emuelogtype type, const char* msg)
{
    switch (type) {
    case emuelogtype::eINFO:
        std::cout << "[INFO] " << msg << "\n";
        break;
    case emuelogtype::eDEBUG:
        std::cout << "[DEBUG] " << msg << "\n";
        break;
    case emuelogtype::eERROR:
        std::cout << "[ERROR] " << msg << "\n";
        break;
    case emuelogtype::eWARNING:
        std::cout << "[WARNING] " << msg << "\n";
        break;
    }
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
