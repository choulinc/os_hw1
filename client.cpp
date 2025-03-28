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
string username;

void* receiveMessages(void* arg){
    char buffer[BUFFER_SIZE];
    while(running){
        memset(buffer, 0, BUFFER_SIZE);
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
        if(bytesReceived <= 0){
            cout << "Disconnected or error receiving data\n";
            running = false;
            break;
        }
        cout << buffer << endl;
    }
    return nullptr;
}

int main(int argc, char* argv[]){
    if(argc != 4){
        cerr << "Usage: " << argv[0] << " <server_ip> <port> <username>\n";
        return 1;
    }

    const char* serverIP = argv[1];
    int serverPort = atoi(argv[2]);
    username = argv[3];

    // creating socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(clientSocket < 0){
        perror("Socket creation failed");
        return 1;
    }

    // specifying address
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverPort);
    
    // Convert IP address
    if(inet_pton(AF_INET, serverIP, &serverAddress.sin_addr) <= 0){
        perror("Invalid address/ Address not supported");
        return 1;
    }

    // sending connection request
    if(connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0){
        perror("Connection failed");
        return 1;
    }

    // Send connect command with username
    string connectCmd = "connect " + username;
    send(clientSocket, connectCmd.c_str(), connectCmd.length(), 0);

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
