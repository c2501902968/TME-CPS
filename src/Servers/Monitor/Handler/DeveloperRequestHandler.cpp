#include "DeveloperRequestHandler.h"
using namespace std;

// const int MESSAGE_BYTES = 2048;

DeveloperRequestHandler::DeveloperRequestHandler() {
    std::cout << "DeveloperRequestHandler created\n";
}

DeveloperRequestHandler::DeveloperRequestHandler(Monitor* monitor) : monitor{monitor} {
    std::cout << "DeveloperRequestHandler created with the name " << monitor->getHostname() << "\n";
}

bool DeveloperRequestHandler::sendApp(Minion* minion, string appDir){
    //TODO
}

bool DeveloperRequestHandler::deployApp(string appID, int instances){
    vector<Minion*> trustedMinions = monitor->pickNTrustedMinions(instances);
    monitor->addApplication(appID, trustedMinions);

    bool copyResult = true;
    for_each(trustedMinions.begin(), trustedMinions.end(),
             [&](Minion* minion) {
                 if(DebugFlags::debugMonitor)
                    cout << "Uploading app to minion";
                copyResult &= sendApp(minion, appID);
                if (!copyResult)
                    return false;

                sockaddr_in minionAddress;
                minionAddress = SocketUtils::createServerAddress(Ports::MINION_MONITOR_PORT);
                minionAddress.sin_addr.s_addr = inet_addr(minion->getIpAddress().c_str());  // minion.getIpAddress();
                int minionSocket = socket(AF_INET, SOCK_STREAM, 0);
                SocketUtils::connectToServerSocket(minionSocket, minionAddress);

                char buffer[SocketUtils::MESSAGE_BYTES];
                string message = Messages::DEPLOY + " " + appID;
                General::stringToCharArray(message, buffer, SocketUtils::MESSAGE_BYTES);
                SocketUtils::sendBuffer(minionSocket, buffer, strlen(buffer), 0);
                if (DebugFlags::debugMonitor)
                    cout << "Wrote: " << buffer << " to Minion\n";

                bzero(buffer, SocketUtils::MESSAGE_BYTES);
                SocketUtils::receiveBuffer(minionSocket, buffer, SocketUtils::MESSAGE_BYTES - 1, 0);
                if (DebugFlags::debugMonitor)
                    cout << "Recieved: " << buffer << " from Minion\n";

                string result(buffer);
                vector<string> resultSplit = General::splitString(result);

                if (resultSplit[0] == Messages::OK){
                    // continue;
                } else if (resultSplit[0] == Messages::NOT_OK){
                    return false;
                }
             });
    return copyResult;
}

bool DeveloperRequestHandler::deleteApp(string appID){
    vector<Minion*> appHosts = monitor->getHosts(appID);
    monitor->deleteApplication(appID);

    bool result = true;
    for_each(appHosts.begin(), appHosts.end(),
             [&](Minion* minion) {
                 if (DebugFlags::debugMonitor)
                     cout << "Deleting app to minion";

                 sockaddr_in minionAddress;
                 minionAddress = SocketUtils::createServerAddress(Ports::MINION_MONITOR_PORT);
                 minionAddress.sin_addr.s_addr = inet_addr(minion->getIpAddress().c_str());  // minion.getIpAddress();
                 int minionSocket = socket(AF_INET, SOCK_STREAM, 0);
                 SocketUtils::connectToServerSocket(minionSocket, minionAddress);

                 char buffer[SocketUtils::MESSAGE_BYTES];
                 string message = Messages::DELETE_APP + " " + appID;
                 General::stringToCharArray(message, buffer, SocketUtils::MESSAGE_BYTES);
                 SocketUtils::sendBuffer(minionSocket, buffer, strlen(buffer), 0);
                 if (DebugFlags::debugMonitor)
                     cout << "Wrote: " << buffer << " to Minion\n";

                 bzero(buffer, SocketUtils::MESSAGE_BYTES);
                 SocketUtils::receiveBuffer(minionSocket, buffer, SocketUtils::MESSAGE_BYTES - 1, 0);
                 if (DebugFlags::debugMonitor)
                     cout << "Recieved: " << buffer << " from Minion\n";

                 string result(buffer);
                 vector<string> resultSplit = General::splitString(result);

                 if (resultSplit[0] == Messages::OK) {
                    //  continue;
                 } else if (resultSplit[0] == Messages::NOT_OK) {
                     return false;
                 }
             });
    return result;
}

void DeveloperRequestHandler::processAttestation(int developerSocket) {
    char buffer[SocketUtils::MESSAGE_BYTES];
    bzero(buffer, SocketUtils::MESSAGE_BYTES);
    SocketUtils::receiveBuffer(developerSocket, buffer, SocketUtils::MESSAGE_BYTES - 1, 0);
    if (DebugFlags::debugMonitor)
        cout << "Recieved: " << buffer << "\n";

    string request(buffer);
    vector<string> requestSplit = General::splitString(request);

    if (requestSplit[0] == Messages::ATTEST) {
        bzero(buffer, SocketUtils::MESSAGE_BYTES);
        string configuration = Messages::QUOTE + " " + AttestationConstants::QUOTE + " " + monitor->getApprovedSHA1() + " " + string((char*)monitor->getApprovedConfiguration());
        General::stringToCharArray(configuration, buffer, SocketUtils::MESSAGE_BYTES);
        // cout << "Buffer: " << buffer;
        SocketUtils::sendBuffer(developerSocket, buffer, strlen(buffer), 0);
        if (DebugFlags::debugMonitor)
            cout << "Wrote: " << buffer << " to client\n";
    }

    bzero(buffer, SocketUtils::MESSAGE_BYTES);
    SocketUtils::receiveBuffer(developerSocket, buffer, SocketUtils::MESSAGE_BYTES - 1, 0);
    if (DebugFlags::debugMonitor)
        cout << "Recieved: " << buffer << "\n";

    string response(buffer);
    vector<string> responseSplit = General::splitString(response);
    if (requestSplit[0] == Messages::NOT_OK) {
        if (DebugFlags::debugMonitor)
            cout << "Developer rejected platform attestation.\n";
    }
}

void DeveloperRequestHandler::startDeveloperRequestHandler(DeveloperRequestHandler developerRequestHandler) {
    std::cout << "DeveloperRequestHandler running with Monitor with the name " << developerRequestHandler.monitor->getHostname() << "\n";

    sockaddr_in developerAddress;
    developerAddress = SocketUtils::createServerAddress(Ports::MONITOR_DEVELOPER_PORT);

    int serverSocket;
    serverSocket = SocketUtils::createServerSocket(developerAddress);

    int developerSocket;
    while (true) {
        developerSocket = SocketUtils::acceptClientSocket(serverSocket);
        cout << "Got connection from Developer\n";

        char buffer[SocketUtils::MESSAGE_BYTES];
        bzero(buffer, SocketUtils::MESSAGE_BYTES);
        SocketUtils::receiveBuffer(developerSocket, buffer, SocketUtils::MESSAGE_BYTES - 1, 0);
        if (DebugFlags::debugMonitor)
            cout << "Recieved: " << buffer << "\n";
        // cout << buffer;
        string command(buffer);
        vector<string> commandSplit = General::splitString(command);

        bool requestResult = false;
        if (commandSplit[0] == Messages::NEW_APP) {
            if (DebugFlags::debugMonitor)
                cout << "Deploying: " << commandSplit[2] << "\n";
            requestResult = developerRequestHandler.deployApp(commandSplit[2], atoi(commandSplit[3].c_str()));
        } else if (commandSplit[0] == Messages::DELETE_APP) {
            if (DebugFlags::debugMonitor)
                cout << "Deleting: " << commandSplit[2] << "\n";
            requestResult = developerRequestHandler.deleteApp(commandSplit[2]);
        }

        if (requestResult){
            bzero(buffer, SocketUtils::MESSAGE_BYTES);
            std::string result = Messages::OK;
            General::stringToCharArray(result, buffer, SocketUtils::MESSAGE_BYTES);
            SocketUtils::sendBuffer(developerSocket, buffer, strlen(buffer), 0);
            if (DebugFlags::debugMonitor)
                cout << "Success \n";
        } else {
            bzero(buffer, SocketUtils::MESSAGE_BYTES);
            std::string result = Messages::NOT_OK;
            General::stringToCharArray(result, buffer, SocketUtils::MESSAGE_BYTES);
            SocketUtils::sendBuffer(developerSocket, buffer, strlen(buffer), 0);
            if (DebugFlags::debugMonitor)
                cout << "Failed \n";
        }
    }

    close(developerSocket);
    close(serverSocket);
}


// int main(int argc, char* argv[]) {
//     std::cout << "Will try to create DeveloperRequestHandler\n";

//     DeveloperRequestHandler* developerRequestHandler;
//     developerRequestHandler = new DeveloperRequestHandler();

//     std::cout << "Bubye\n";

//     return 0;
// }