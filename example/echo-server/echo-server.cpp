#include <iostream>
//#include <mueiocp.h>
#include <muevent.h>
#include <conio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>

static bool acceptcb(mueventbase* base, int eventid, LPVOID arg);
static bool readcb(mueventbase* base, int eventid, LPVOID arg);
static void eventcb(mueventbase* base, int eventid, emuestatus type, LPVOID arg);
static void logger(emuelogtype type, const char* msg);

BOOL WINAPI signalhandler(DWORD signum);
int indexctr = 0;
mueventbase* gbase = NULL;

int main()
{
    SetConsoleCtrlHandler(signalhandler, TRUE);

    mueventbase* base = mueventnewbase(2, logger);
    gbase = base;
    mueventlisten(base, 3000, acceptcb, NULL);

    std::cout << "press Ctrl-C to exit.\n";
    mueventdispatch(base, true); /**set true to make this call blocking*/

    std::cout << "dispatchbreak called, cleaning the mess up...\n";
    mueventbasedelete(base);

    std::cout << "cleanup done, press any key to exit.\n";
    int ret = _getch();
    return ret;
}

/**accept callback, returing false in this callback will close the client*/
static bool acceptcb(mueventbase* base, int eventid, LPVOID arg)
{
    /**client connection accepted, we should store the MUE object here to our variable..*/

    /**set read and event callback to newly accepted client*/
    mueventsetcb(base, eventid, readcb, eventcb, NULL);
    
    return true;
}

/**read callback, returing false in this callback will close the client*/
static bool readcb(mueventbase* base, int eventid, LPVOID arg)
{
    char buff[100] = { 0 };

    int readsize = mueventread(base, eventid, buff, sizeof(buff));
    
    std::cout << "Client message : " << buff << "\n";

    mueventwrite(base, eventid, (LPBYTE)buff, readsize); /**echo the received data from client*/


    return true;
}

/**event callback*/
static void eventcb(mueventbase* base, int eventid, emuestatus type, LPVOID arg)
{
    switch (type) {
    case emuestatus::eCONNECTED:
        mueventaddlog(base, emuelogtype::eINFO, "client connected, ip:%s socket:%d",
            mueventgetipaddr(base, eventid), mueventgetsocket(base, eventid));
        break;
    case emuestatus::eCLOSED:
        break;
    case emuestatus::eSOCKERROR:
        break;
    }
}

/**log callback*/
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


BOOL WINAPI signalhandler(DWORD signum)
{
    switch (signum)
    {
    case CTRL_C_EVENT:
        mueventdispatchbreak(gbase); /**we will return dispatch upon Ctrl-C*/
        break;
    }
    return TRUE;
}
