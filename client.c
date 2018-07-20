#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include "common.h"
#include <sys/time.h>


#define PORT "2018"
#define INPUT_SIZE 2048
#define BUFF_SIZE 1024
#define TIMEOUT_IN_SECONDS 10


struct client_state cs;
struct sockaddr_in server_address;
int socketfd;
int debug_flag = 0;
int isTesting = 0;

void handleError(char* msg, cmdLine* cmd) {
    perror(msg);
    freeCmdLines(cmd);
    exit(-1);
}

void printIfDebug(char* msg) {
    if(debug_flag != 0) {
        char msg2print[strlen(cs.server_addr) + strlen("|Log: ") + strlen(msg)];
        strcpy(msg2print, cs.server_addr);
        strcat(msg2print, "|Log: ");
        strcat(msg2print, msg);
        fprintf(stderr, "%s\n", msg2print);
    }
}

void updateDebugFlag(int argc, char* argv[]) {
    if(argc > 1 && strcmp(argv[1], "-d") == 0) {
        debug_flag = 1;
    }
}

void send2Server(char* msg, cmdLine* cmd) {
    if(send(socketfd, msg, strlen(msg), 0) == -1) {
        handleError("Send error", cmd);
    }
}

void initializeClientState() {
    cs.server_addr = "nil";
    cs.conn_state = IDLE;
    cs.client_id = NULL;
    cs.sock_fd = -1;
}

int check_connCmd_isValid(cmdLine* cmd) {
    if(cs.conn_state != IDLE) {
        perror("Tried to connect while <conn_state> isn't IDLE");
        freeCmdLines(cmd);
        return -2;
    }
        
    if(cmd->argCount == 1) {
        perror("Server address is missing");
        return -1;
    }
    
    return 0;
}

void print_nok_error(char* msg) {
    char msg2print[strlen(msg) + strlen("Server Error: ")];
    strcpy(msg2print, "Server Error: ");
    strcat(msg2print, msg);
    fprintf(stderr, "%s\n", msg2print);
}

int execConn(cmdLine* cmd) {
    int isValid;
    if((isValid = check_connCmd_isValid(cmd)) != 0) {
        return isValid;
    }
    
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    if(getaddrinfo(cmd->arguments[1], PORT, &hints, &res) == -1) {
        handleError("getaddrinfo() error", cmd);
    }
    
    if((socketfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
        handleError("New socket error", cmd);
    }
    
    if(connect(socketfd, res->ai_addr, res->ai_addrlen) == -1) {
        handleError("Connect error", cmd);
    }
    
    cs.server_addr = calloc(strlen(cmd->arguments[1]), sizeof(char));
    strcpy(cs.server_addr, cmd->arguments[1]);
    
    send2Server("hello", cmd);
    
    cs.conn_state = CONNECTING;
    
    char serverInput[INPUT_SIZE] = {0};
    memset(serverInput, '\0', INPUT_SIZE);
    if(recv(socketfd, serverInput, INPUT_SIZE, 0) == -1) {
        handleError("Recieve error", cmd);
    }
    printIfDebug(serverInput);
    
    char serverInputCheck[strlen(serverInput)];
    strcpy(serverInputCheck, serverInput);
    serverInputCheck[6] = '\0';
    if(strcmp(serverInputCheck, "hello ") != 0) {
        serverInputCheck[4] = '\0';
        
        if(strcmp(serverInputCheck, "nok ") == 0) {
            print_nok_error(serverInput+4);
        }
        else {
            perror("Handshake error");
        }
        
        initializeClientState();
        return -1;
    }
    
    char tmp[strlen(serverInput+6)];
    strcpy(tmp, serverInput+6);
    cs.client_id = tmp;
    cs.conn_state = CONNECTED;
    cs.sock_fd = socketfd;
    
    if(isTesting) printf("execConn() is finished successfuly\n");
    return 0;
}

int execBye(cmdLine* cmd) {
    if(cs.conn_state != CONNECTED) {
        perror("Tried to disconnect while <conn_state> isn't CONNECTED");
        freeCmdLines(cmd);
        return -2;
    }
    
    send2Server("bye", cmd);
    if(close(socketfd) == -1) {
        handleError("Close error", cmd);
    }
    
    initializeClientState();
    return 0;
}

int execLs(cmdLine* cmd) {
    if(cs.conn_state != CONNECTED) {
        perror("Tried to run <ls> while <conn_state> isn't CONNECTED");
        freeCmdLines(cmd);
        return -2;
    }
    
    send2Server("ls", cmd);
    
    char inputCheck[3] = {0};
    memset(inputCheck, '\0', 3);
    char serverInput[INPUT_SIZE] = {0};
    memset(serverInput, '\0', INPUT_SIZE);
    if(recv(socketfd, inputCheck, 3, 0) == -1) {
        handleError("Recieve error", cmd);
    }
    printIfDebug(inputCheck);
    
    if(strcmp(inputCheck, "nok") == 0) {
        if(recv(socketfd, serverInput, INPUT_SIZE, 0) == -1) {
            handleError("Recieve error", cmd);
        }
        print_nok_error(serverInput+1);
        execBye(cmd);
    }
    else if(strcmp(inputCheck, "ok ") == 0) {
        if(recv(socketfd, serverInput, INPUT_SIZE, 0) == -1) {
            handleError("Recieve error", cmd);
        }
        printf("%s", serverInput);
    }
    printIfDebug(serverInput);
    
    return 0;
}

int execGet(cmdLine* cmd) {
    char* fileName = cmd->arguments[1];
    if(cs.conn_state != CONNECTED) {
        perror("Tried to run <get x> while <conn_state> isn't CONNECTED");
        freeCmdLines(cmd);
        return -2;
    }
    
    char msg2server[4 + strlen(fileName)];
    strcpy(msg2server, "get ");
    strcat(msg2server, fileName);
    send2Server(msg2server, cmd);
    
    char inputfromServer[INPUT_SIZE] = {0};
    memset(inputfromServer, '\0', INPUT_SIZE);
    if(recv(socketfd, inputfromServer, INPUT_SIZE, 0) == -1) {
        handleError("Recieve error", cmd);
    }
    printIfDebug(inputfromServer);
    
    char inputCheck[3];
    strncpy(inputCheck, inputfromServer, 3);
    inputCheck[3] = 0;
    
    if(strcmp(inputCheck, "nok") == 0) {
        print_nok_error(inputfromServer);
        execBye(cmd);
    }
    else if(strcmp(inputCheck, "ok ") == 0) {
        int fileSize = atoi(inputfromServer + 3);
        memset(inputfromServer, '\0', INPUT_SIZE);
        send2Server("send file", cmd);
        
        char fileName_withExtension[strlen(fileName) + 4];
        strcpy(fileName_withExtension, fileName);
        strcat(fileName_withExtension, ".tmp");
        FILE* tmpFile = fopen(fileName_withExtension, "w+");
        cs.conn_state = DOWNLOADING;
        
        int totalBytesReceived = 0;
        char downloadBuffer[BUFF_SIZE] = {0};
        while(totalBytesReceived < fileSize) {
            memset(downloadBuffer, '\0', BUFF_SIZE);
            int bytesRecieved = 0;
            
            if((bytesRecieved = recv(socketfd, downloadBuffer, BUFF_SIZE, 0)) == -1) {
                handleError("Recieve error", cmd);
            }
            
            printIfDebug(downloadBuffer);
            fprintf(tmpFile, "%s", downloadBuffer);
            totalBytesReceived += bytesRecieved;
        }
        fclose(tmpFile);
        send2Server("done", cmd);
        
        // sets a 10 seconds timeout
        struct timeval timeout;
        timeout.tv_sec = TIMEOUT_IN_SECONDS;
        timeout.tv_usec = 0;
        setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof timeout);
        
        memset(inputfromServer, '\0', INPUT_SIZE);
        if(recv(socketfd, inputfromServer, INPUT_SIZE, 0) == -1) {
            handleError("Recieve error", cmd);
        }
        if(strcmp(inputfromServer, "ok") != 0) {
            fprintf(stderr, "Error while downloading file %s\n", fileName);
            remove(fileName_withExtension);
            execBye(cmd);
        }
        else { // "ok" was returned
            rename(fileName_withExtension, fileName);
            cs.conn_state = CONNECTED;
        }
        printIfDebug(inputfromServer);
    }
    
    return 0;
}

int handleCommand(char* input) {
    cmdLine* cmd = parseCmdLines(input);
            
    if     (strcmp(cmd->arguments[0], "quit") == 0) {
        freeCmdLines(cmd);
        return -1;
    }
    else if(strcmp(cmd->arguments[0], "conn") == 0) {
        execConn(cmd);
    }
    else if(strcmp(cmd->arguments[0], "ls") == 0) {
        execLs(cmd);
    }
    else if(strcmp(cmd->arguments[0], "get") == 0 && cmd->argCount == 2) {
        execGet(cmd);
    }
    else if(strcmp(cmd->arguments[0], "bye") == 0) {
        execBye(cmd);
    }
    
    freeCmdLines(cmd);
    return 0;
}

int main (int argc , char* argv[]) {
    updateDebugFlag(argc, argv);
    char input[INPUT_SIZE];
    initializeClientState();
    
    while(1) {
        printf("server:%s>", cs.server_addr);
        fgets(input, sizeof(input), stdin);
        
        if(input[0] != '\n')
            if(handleCommand(input) < 0)
                break;
        
        memset(input, 0, sizeof(input));
    }
    
    return 0;
}
