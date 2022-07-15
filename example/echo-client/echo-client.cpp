#include <iostream>
#include <muevent.h>
#include <conio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>

static bool acceptcb(int eventid, LPVOID arg);
static bool readcb(int eventid, LPVOID arg);
static void eventcb(int eventid, emuestatus type, LPVOID arg);
static void logger(emuelogtype type, const char* msg);

mueventbase* base = NULL;

int main()
{
    char initbuf[1] = { 0 };
    int initlen = 0;

    base = mueventnewbase(2, logger);

    mueventdispatch(base, false); /**set false to make this call return immediately*/

    for (int n = 0; n < 50; n++) {
        mueventlock(base);
        int eventid = mueventconnect(base, "127.0.0.1", 3000, initbuf, initlen);
        mueventsetcb(base, eventid, readcb, eventcb, (LPVOID)n);
        mueventunlock(base);
    }

    std::cout << "press any key to cleanup and exit.\n";
    int ret = _getch();

    mueventbasedelete(base);
    
    return ret;
}

/**read callback*/
static bool readcb(int eventid, LPVOID arg)
{
    char buff[100] = { 0 };
    int index = (int)arg;

    int readsize = mueventread(base, eventid, buff, sizeof(buff)); /**receive data, read it now...*/
    std::cout << "Server echo to index " << index << ": " << buff << "\n";
    return true;
}

/**event callback*/
static void eventcb(int eventid, emuestatus type, LPVOID arg)
{
    char sbuf[100] = { 0 };
    int index = (int)arg;

    switch (type) {
    case emuestatus::eCONNECTED: /**client is connected to host, send the message now...*/
        sprintf_s(sbuf, 100, "Hello from Index %d.", index);
        mueventwrite(base, eventid, (LPBYTE)sbuf, strlen(sbuf) + 1);
        break;
    case emuestatus::eCLOSED:
        break;
    case emuestatus::eSOCKERROR: /**free part of MUE object's memory resources, we will totally remove it with remove call or upon exit*/
        break;
    }
}

/**assigned log callback*/
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
