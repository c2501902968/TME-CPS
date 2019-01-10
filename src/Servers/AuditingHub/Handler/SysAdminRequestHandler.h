#ifndef _sysadminrequesthandler_h_
#define _sysadminrequesthandler_h_

#include <iostream>
#include "../AuditingHub.h"
using namespace std;

class SysAdminRequestHandler {
    //TODO this class needs to be checked better
    AuditingHub* auditingHub;
    string adminUsername;
    string remoteHost;
    int pidAuditingHubToNodeSessionProcess;
    int adminToHubSocket;
    int processWrite[2];
    int processRead[2];

    private: 
    void launchSessionProcess();
    void launchLogger();
    bool setNodeUntrusted();
    bool purgeMinion();
    bool launchManagementSession();
    void processAttestation(int adminSocket);

   public:
    SysAdminRequestHandler();
    SysAdminRequestHandler(AuditingHub* auditingHub);
    static void startSysAdminRequestHandler(SysAdminRequestHandler sysAdminRequestHandler, int adminToHubSocket);
};

#endif