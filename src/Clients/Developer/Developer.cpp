#include "Developer.h"
#include <tss2/tss2_esys.h>
#include "tpm2_options.h"
#include "log.h"
#include "files.h"
#include "tpm2_tool.h"
#include "tpm2_util.h"
#define SUPPORTED_ABI_VERSION \
{ \
    .tssCreator = 1, \
    .tssFamily = 2, \
    .tssLevel = 1, \
    .tssVersion = 108, \
}
using namespace std;

const int REQUEST_FLAG_INDEX = 1;
const int MONITOR_HOST_FLAG_INDEX = 2;
const int USERNAME_FLAG_INDEX = 4;
const int KEY_FLAG_INDEX = 6;
const int APPDIR_FLAG_INDEX = 8;
const int INSTANCES_FLAG_INDEX = 10;

const int ERROR_CODE = 255;

//  Usage:
//  Developer -h: help
//  Developer

Developer::Developer() {
    std::cout << "Developer created\n";
}

Developer::Developer(string monitorHost, string username, string key, string appDir, int instances) : monitorHost{monitorHost}, username{username}, key{key}, appDir{appDir}, instances{instances} {
    std::cout << "Developer created\n";
}

Developer::Developer(string monitorHost, string username, string key, string appDir) : monitorHost{monitorHost}, username{username}, key{key}, appDir{appDir}  {
    std::cout << "Developer created\n";
}

void Developer::attestMonitor(int monitorSocket){
    char buffer[SocketUtils::MESSAGE_BYTES];
    bzero(buffer, SocketUtils::MESSAGE_BYTES);
    string attestationRequestString = Messages::ATTEST + " " + AttestationConstants::NONCE;
    General::stringToCharArray(attestationRequestString, buffer, SocketUtils::MESSAGE_BYTES);
    SocketUtils::sendBuffer(monitorSocket, buffer, strlen(buffer), 0);
    if (DebugFlags::debugDeveloper)
        cout << "Wrote: " << buffer << " to server\n";

    bzero(buffer, SocketUtils::MESSAGE_BYTES);
    SocketUtils::receiveBuffer(monitorSocket, buffer, SocketUtils::MESSAGE_BYTES - 1, 0);
    if (DebugFlags::debugDeveloper)
        cout << "Recieved from server: " << buffer << "\n";

    string quote(buffer);
    vector<string> splittedQuote = General::splitString(quote);

    if (splittedQuote[0] == Messages::QUOTE && splittedQuote[1] == AttestationConstants::QUOTE && splittedQuote[2] == AttestationConstants::PCR_SHA1 && splittedQuote[3] == AttestationConstants::PCR_SHA1) {
        string approvedMessage = Messages::OK_APPROVED;
        General::stringToCharArray(approvedMessage, buffer, SocketUtils::MESSAGE_BYTES);
        SocketUtils::sendBuffer(monitorSocket, buffer, strlen(buffer), 0);
        if (DebugFlags::debugDeveloper)
            cout << "Wrote: " << buffer << " to server\n";
    } else {
        General::stringToCharArray(Messages::NOT_APPROVED, buffer, SocketUtils::MESSAGE_BYTES);
        SocketUtils::sendBuffer(monitorSocket, buffer, strlen(buffer), 0);
        if (DebugFlags::debugDeveloper)
            cout << "Wrote: " << buffer << " to server\n";
    }
}

bool Developer::sendSyncMessageAndGetResponse(string message){
    struct hostent* serverHost;
    serverHost = SocketUtils::getHostByName(monitorHost);

    sockaddr_in serverAddress;
    serverAddress = SocketUtils::createServerAddress(Ports::MONITOR_DEVELOPER_PORT);
    bcopy((char *)serverHost->h_addr, (char *)&serverAddress.sin_addr.s_addr, serverHost->h_length);
    int monitorSocket = socket(AF_INET, SOCK_STREAM, 0);
    SocketUtils::connectToServerSocket(monitorSocket, serverAddress);
    if (DebugFlags::debugDeveloper)
        cout << "Connected to the server\n";

    attestMonitor(monitorSocket);

    bool sendResult = true;
    vector<string> splittedMessage = General::splitString(message);
    if (splittedMessage[0] == Messages::NEW_APP){
        sendResult = Developer::sendApp();
    }

    if (!sendResult)
        return sendResult;

    char buffer[SocketUtils::MESSAGE_BYTES];
    bzero(buffer, SocketUtils::MESSAGE_BYTES);
    General::stringToCharArray(message, buffer, SocketUtils::MESSAGE_BYTES);
    SocketUtils::sendBuffer(monitorSocket, buffer, strlen(buffer), 0);
    if (DebugFlags::debugDeveloper)
        cout << "Wrote: " << buffer << " to server\n";

    bzero(buffer, SocketUtils::MESSAGE_BYTES);
    SocketUtils::receiveBuffer(monitorSocket, buffer, SocketUtils::MESSAGE_BYTES - 1, 0);
    if (DebugFlags::debugDeveloper)
        cout << "Recieved from server: " << buffer << "\n";

    string response(buffer);
    vector<string> splittedResponse = General::splitString(response);

    if (splittedResponse[0] == Messages::OK) {
        close(monitorSocket);
        return true;
    } else if (splittedResponse[0] == Messages::NOT_OK) {
        close(monitorSocket);
        return false;
    }
    close(monitorSocket);
    return false;
}

bool Developer::deleteApp(){
    if (DebugFlags::debugDeveloper)
        cout << "Deleting app on monitor\n";

    string message = Messages::DELETE_APP + " " + Developer::username + " " + appDir.substr(appDir.find("/")+1);
    bool messageResult;
    messageResult = sendSyncMessageAndGetResponse(message);

    if (!messageResult)
        return false;
    return true;
}

bool Developer::sendApp(){
    char* scpArgsStream;
    int size = asprintf(&scpArgsStream, "%s -r -i %s -oStrictHostKeyChecking=no %s %s@%s:%s",
                        ProcessBinaries::SCP.c_str(), key.c_str(), appDir.c_str(), username.c_str(),
                        monitorHost.c_str(), Directories::APPS_DIR_MONITOR.c_str());//, appDir.substr(appDir.find("/")+1).c_str());
    string scpArgsStreamStr(scpArgsStream);
    std::vector<std::string> scpArgsStreamVec = General::splitString(scpArgsStreamStr);
    char* scpArgsStreamCharVec[scpArgsStreamVec.size()];
    int i = 0;
    for (const std::string& str : scpArgsStreamVec) {
        scpArgsStreamCharVec[i] = const_cast<char*>(str.c_str());
        // cout << scpArgsStreamCharVec[i] << "\n";
        i++;
    }
    scpArgsStreamCharVec[i] = NULL;
    if (DebugFlags::debugDeveloper)
        cout << "Executing command: " << scpArgsStream << "\n";
    fflush(NULL);
    pid_t pid = fork();
    if (pid == 0) {
        int result = execvp(ProcessBinaries::SCP.c_str(), scpArgsStreamCharVec);
        if (result == -1) {
            if (DebugFlags::debugDeveloper){
                cout << "Command failed\n";
                cout << "Result: " << result << "\n";
                cout << strerror(errno) << "\n";
            }
            exit(-1);
        }
        cout << "Result: " << result << "\n";
        cout << strerror(errno) << "\n";
        exit(0);
    }

    if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == ERROR_CODE)
            return false;
    }
    return true;
}

bool Developer::deployApp(){
    if (DebugFlags::debugDeveloper)
        cout << "Deploying app on monitor\n";

    string message = Messages::NEW_APP + " " + Developer::username + " " + appDir.substr(appDir.find("/") + 1) + " " + to_string(Developer::instances);
    bool messageResult;
    messageResult = sendSyncMessageAndGetResponse(message);

    if (!messageResult)
        return false;
    return true;
}

void printHelp() {
    std::cout << "Usage to Deploy App: Developer -deploy -m monitorHost -u username -k key -a appDir -i #instances\n";
    std::cout << "Usage to Delete App: Developer -delete -m monitorHost -u username -k key -a appDir \n";
}

static ESYS_CONTEXT* ctx_init(TSS2_TCTI_CONTEXT *tcti_ctx) {

	TSS2_ABI_VERSION abi_version = SUPPORTED_ABI_VERSION;
	ESYS_CONTEXT *esys_ctx;

	TSS2_RC rval = Esys_Initialize(&esys_ctx, tcti_ctx, &abi_version);
	if (rval != TPM2_RC_SUCCESS) {
		return NULL;
	}

	return esys_ctx;
}

typedef struct tpm_random_ctx tpm_random_ctx;
struct tpm_random_ctx {
	bool output_file_specified;
	char *output_file;
	UINT16 num_of_bytes;
};

static tpm_random_ctx ctx;
extern bool output_enabled = true;

int main(int argc, char* argv[]) {
	TSS2_TCTI_CONTEXT *tcti = NULL;
	ESYS_CONTEXT *ectx = NULL;
	ectx  = ctx_init(tcti);
	ctx.num_of_bytes = 5;
	TPM2B_DIGEST *random_bytes;


	TSS2_RC rval = Esys_GetRandom(ectx,
		ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
		ctx.num_of_bytes, &random_bytes);
	cout << "Something rval -> " << rval << " TPM SUCCESS -> " << TPM2_RC_SUCCESS <<"\n";
	if (rval != TPM2_RC_SUCCESS) {
		//LOG_PERR(Esys_GetRandom, rval);
		//return false;
	}

	if (!ctx.output_file_specified) {
		UINT16 i;
		for (i = 0; i < random_bytes->size; i++) {
			tpm2_tool_output("%s0x%2.2X", i ? " " : "", random_bytes->buffer[i]);
		}
		tpm2_tool_output("\n");
		free(random_bytes);;
		//return true;
	}
	//--//
    string monitorHost;
    string username;
    string key;
    string appDir;
    string instances;
    Developer* developer;

    if (argc != 10 && argc != 12) {
        std::cout << "Invalid number of fields\n";
        printHelp();
        return -1;
    }
    string command = argv[REQUEST_FLAG_INDEX];
    bool commandResult = false;
    if(command == "-deploy"){
        monitorHost = argv[MONITOR_HOST_FLAG_INDEX+1];
        username = argv[USERNAME_FLAG_INDEX+1];
        key = argv[KEY_FLAG_INDEX+1];
        appDir = argv[APPDIR_FLAG_INDEX+1];
        instances = argv[INSTANCES_FLAG_INDEX+1];
        developer = new Developer(monitorHost, username, key, appDir, atoi(instances.c_str()));
        commandResult = developer->deployApp();
    } else if (command == "-delete"){
        monitorHost = argv[MONITOR_HOST_FLAG_INDEX+1];
        username = argv[USERNAME_FLAG_INDEX+1];
        key = argv[KEY_FLAG_INDEX+1];
        appDir = argv[APPDIR_FLAG_INDEX+1];
        developer = new Developer(monitorHost, username, key, appDir);
        commandResult = developer->deleteApp();
    } else {
        std::cout << "Unknown Command\n";
        printHelp();
    }

    if(commandResult)
        cout << "Success";
    else 
        cout << "Failed";

    return 0;
}