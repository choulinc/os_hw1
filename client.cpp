#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

using namespace std;

#define BUFFER_SIZE 1024

int clientSocket;
bool running = true;


void* receiveMessages(void* arg){
    char buffer[BUFFER_SIZE];
    while(running){
        memset(buffer, 0, BUFFER_SIZE);
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
        if(bytesReceived <= 0){
            cout << "Discoonnected\n";
            running = false;
            break;
        }
        cout << buffer <<  endl;
    }
    return nullptr;
}

int main(){	
    // creating socket
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(clientSocket < 0){
        perror("Socket creation failed");
        return 1;
    }
    // specifying address
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

    // sending connection request
    if(connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress) < 0)){
        perror("Connection failed");
        return 1;
    }
    
    pthread_t receiveThread;
    pthread_create(&receiveThread, nullptr, receiveMessages, nullptr);
    pthread_detach(receiveThread);

    string input;
    while(running){
        getline(cin, input);
        if(input == "bye"){
            send(clientSocket, input.c_str(), input.length(), 0);
            running = false;
            break;
        }
        send(clientSocket, input.c_str(), input.length(), 0);
    }

    // closing socket
    close(clientSocket);

    return 0;
}

