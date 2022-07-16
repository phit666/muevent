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
static BOOL WINAPI signalhandler(DWORD signum);
static mueventbase* base = NULL;

static int portProxy;
static int portServer;
static char proxyip[50] = { 0 };
static char svrip[50] = { 0 };

int main(int argc, char* argv[])
{
    std::cout << "Proxy server powered by muevent." << std::endl;

	if (argc < 4) {
		std::cout << std::endl;
		std::cout << "Usage:" << std::endl;
		std::cout << "proxy-server.exe <listen ip> <listen port> <remote ip> <remote port>" << std::endl;
		std::cout << std::endl;
		system("pause");
		return -1;
	}

    SetConsoleCtrlHandler(signalhandler, TRUE);

    std::stringstream s;
    s << argv[2]; s >> portProxy; s.clear();
    s << argv[4]; s >> portServer;
    memcpy(&proxyip, argv[1], sizeof(proxyip));
    memcpy(&svrip, argv[3], sizeof(svrip));

    base = mueventnewbase(0, logger, (DWORD)emuelogtype::eSILENT);

    LPMuevenProxyInfo proxyinfo = new MuevenProxyInfo;
    proxyinfo->_base = base;
    memcpy(proxyinfo->_remotehost, svrip, sizeof(proxyinfo->_remotehost));
    proxyinfo->_remoteport = portServer;

    if (mueventproxyserver(proxyinfo, proxyip, portProxy)) {
        mueventdispatch(base, true);
    }

    mueventbasedelete(base);
    delete proxyinfo;
    std::cout << "Press any key to exit" << std::endl;
    return _getch();
}

static BOOL WINAPI signalhandler(DWORD signum)
{
    switch (signum)
    {
    case CTRL_C_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_BREAK_EVENT:
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
