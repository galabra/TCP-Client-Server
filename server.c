#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/sendfile.h>

#define PORT "2018"
#define INPUT_SIZE 2048
#define CWD_LENGTH 100

struct client_state cs;
char buff[INPUT_SIZE];
int isTesting = 0;
int debug_flag = 0;
int client_num = 0;
char* ip_address;
//char ip_address[INET_ADDRSTRLEN];


void updateDebugFlag(int argc, char* argv[], struct sockaddr_in *res) {
    if(argc > 1 && strcmp(argv[1], "-d") == 0) {
        debug_flag = 1;
    }
    //inet_ntop(AF_INET, &(res->sin_addr), ip_address, INET_ADDRSTRLEN);
    ip_address = inet_ntoa(((struct sockaddr_in*)res)->sin_addr);
}

void printIfDebug(char* msg) {
    if(debug_flag != 0) {
        char msg2print[strlen(cs.client_id) + strlen("|Log: ") + strlen(msg)];
        strcpy(msg2print, cs.client_id);
        strcat(msg2print, "|Log: ");
        strcat(msg2print, msg);
        fprintf(stderr, "%s\n", msg2print);
    }
}

void handleError(char* msg) {
    perror(msg);
    exit(-1);
}

void initializeClientState() {
    char hostName[INPUT_SIZE];
    cs.client_id = NULL;
    cs.conn_state = IDLE;
    cs.sock_fd = -1;
    gethostname(hostName, strlen(hostName));
}

void setClientID() {
    char client_num_generator[1];
    sprintf(client_num_generator, "%d", client_num);
    char new_client_id[strlen("client") + 1];
    strcpy(new_client_id, "client");
    strcat(new_client_id, client_num_generator);
    
    cs.client_id = strdup(new_client_id);
    
    client_num ++;
}

void unknownMsg(char* msg) {
    fprintf(stderr, "%s|ERROR: Unknown message %s\n", cs.client_id, msg);
}

void send2Client(char* msg) {
    if(send(cs.sock_fd, msg, strlen(msg), 0) == -1) {
        handleError("Send error");
    }
}

void send_nok_andDisconnect(char* nokMsg) {
    send2Client(nokMsg);
    close(cs.sock_fd);
    initializeClientState();
}

int execBye() {
    if(cs.conn_state != CONNECTED) {
        send_nok_andDisconnect("nok state");
        return 0;
    }
    else {
        send2Client("bye");
        printf("Client %s disconnected\n", cs.client_id);
        close(cs.sock_fd);
        initializeClientState();
        return -1; // end the client-loop
    }
}

int execLs() {
    if(cs.conn_state != CONNECTED) {
        send_nok_andDisconnect("nok state");
        return 0;
    }
    else {
        char* output = list_dir();
        if(output == NULL) {
            send_nok_andDisconnect("nok filesystem");
            return 0;
        }
        send2Client("ok ");
        send2Client(output);
        char cwd[CWD_LENGTH];
        getcwd(cwd, CWD_LENGTH);
        printf("Listed files at %s\n", cwd);
    }
}

void recieveFromClient() {
    memset(buff, '\0', INPUT_SIZE);
    if(recv(cs.sock_fd, &buff, sizeof(buff), 0) <= 0) {
        handleError("Recieve error");
    }
    printIfDebug(buff);
}

int execGet() {
    if(cs.conn_state != CONNECTED) {
        send_nok_andDisconnect("nok state");
        return 0;
    }
    
    char fileName[strlen(buff + 4)];
    strcpy(fileName, buff + 4);
    int fileSize = file_size(fileName);
    if(fileSize == -1) {
        send_nok_andDisconnect("nok file");
        return 0;
    }
    char fileSizeAsString[CWD_LENGTH];
    sprintf(fileSizeAsString, "%d", fileSize);
    
    send2Client("ok ");
    send2Client(fileSizeAsString);
    
    int file2send_fd = open(fileName, O_RDONLY);
    recieveFromClient();
    if(strcmp(buff, "send file") == 0) {
        cs.conn_state = DOWNLOADING;
        sendfile(cs.sock_fd, file2send_fd, NULL, fileSize);
        
        recieveFromClient();
        if(strcmp(buff, "done") != 0) {
            send_nok_andDisconnect("nok done");
            return 0;
        }
        else {
            cs.conn_state = CONNECTED;
            send2Client("ok");
            printf("Sent file %s\n", fileName);
        }
    }
    
}

int exec(char* command) {    
    if(strcmp(buff, "bye") == 0) {
        return execBye();
    }
    else if(strcmp(buff, "ls") == 0) {
        return execLs();
    }
    else if(strstr(buff, "get") != NULL) {
        return execGet();
    }
    else if(strcmp(buff, "hello") == 0) {
        if(cs.conn_state != IDLE) {
            send_nok_andDisconnect("nok state");
            return -2;
        }
    }
    else {
        unknownMsg(buff);
    }
    
    return 0;
}

int main (int argc , char* argv[]) {
    int socketfd;
    initializeClientState();
    struct addrinfo hints, *res;
    struct sockaddr_storage client_addr;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    if(getaddrinfo(NULL, PORT, &hints, &res) == -1) {
        handleError("getaddrinfo() error");
    }
    updateDebugFlag(argc, argv, (struct sockaddr_in *)res);
    
    // make a socket:
    if((socketfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
        handleError("New socket error");
    }
    
    if(bind(socketfd, res->ai_addr, res->ai_addrlen) == -1) {
        handleError("Bind error");
    }
    
    while(1) {  // the main server loop (waiting for a client)
        
        if(listen(socketfd, 1) == -1) {
            handleError("Listen error");
        }
        
        socklen_t addr_size = sizeof(client_addr);
        if((cs.sock_fd = accept(socketfd, (struct sockaddr *)&client_addr, &addr_size)) == -1) {
            handleError("Accept error");
        }
        
        setClientID();
        recieveFromClient();
        
        if(strcmp("hello", buff) != 0) {
            unknownMsg(buff);
            continue;
        }
        
        if(cs.conn_state != IDLE) {
            send_nok_andDisconnect("nok state");
        }
        
        cs.conn_state = CONNECTED;
        char helloNewClient[strlen("hello ") + strlen(cs.client_id)];
        strcpy(helloNewClient, "hello ");
        strcat(helloNewClient, cs.client_id);
        
        send2Client(helloNewClient);
        printf("Client %s connected\n", cs.client_id);
        
        
        while(1) {  // the client-loop
            recieveFromClient();
            if(exec(buff) == -1) {
                break;
            }
        }
    }
    
    return 0;
}