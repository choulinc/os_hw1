#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>
#include <vector>
#include <map>

using namespace std;

const int MAX_CLIENTS = 5;

struct ClientInfo{
   string username;
   int socket_fd;
};


/******username -> ClientInfo******/
map<string, ClientInfo> clients;
pthread_mutex_t clients_mutex = PTHREADS_MUTEX_INITIALIZER;

void* clientHandler(void* clientSocket){
    int socket_fd = *(int*)clientSocket;
    delete (int*)clientSocket;
    char buffer[1024];

    memset(buffer, 0, sizeof(buffer));
    int byteReceived = recv(socket_fd, buffer, sizeof(buffer) - 1, 0);
    if(byteReceived <= 0){
        close(socket_fd);
        return nullptr;
    }

    string receivedMessage(buffer);
    size_t pos = receivedMessage.find(" ");
    if(pos == string::npos){
        close(socket_fd);
        return nullptr;
    }

    string command = receivedMessage.substr(0, pos);
    string username = receivedMessage.substr(pos + 1);

    if(command != "connect"){
        close(socket_fd);
        return nullptr;
    }

    pthread_mutex_lock(&clients_mutex);
    clients[username] = {username, socket_fd};
    pthread_mutex_unlock(&clients_mutex);

    cout << "<User " << username << "is online.>\n";

    while(true){
        memset(buffer, 0, sizeof(buffer));
        bytesReceived = recv(soclet_fd, buffer, sizeof(buffer) - 1, 0);
        if(byteReceived <= 0){
            break;
        }

        string msg(buffer);
        if(msg == "bye"){
            cout << "<User " << username << " is off-line.>\n";
            break;
        }
        
        pos = msg.find(" ");
        if(pos != string::npos && msg.substr(0, pos) == "chat"){
            size_t pos2 = msg.find(" ", pos + 1);
            if(pos2 != string::npos){
                string targetUser = msg.substr(pos + 1, pos2 - pos - 1);
                string message = msg.substr(pos2 + 1);

                pthread_mutex_lock(&clients_mutex);
                if(clients.find(targetUser) != clients.end()){
                    int targetSocket = clients[targetUser].socket_fd;
                    string fullMessage = "<From " + username + "> " + message;
                    send(targetSocket, fullMessage.c_str(), fullMessage.length(), 0);
                }else{
                    string errorMsg = "<User " + targetUser + "does not exist.>";
                    send(socket_fd, errorMsg.c_str(), errorMsg.length(), 0);
                }
                pthread_mutex_unlock(&clients_mutex);
            }
        }
    }

    pthread_mutex_lock(&clients_mutex);
    clients.erase(username);
    pthread_mutex_unlock(&clients_mutex);

    close(socket_fd);
    return nullptr;
}


    

int main(){
    
    /******socket******/

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(serverSocket < 0){
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    sockaddr_in serverAddress; //store the address
    serverAddress.sin_family = AF_INET;    
    serverAddress.sin_port = htons(8080); // convert unsigned int from machine byte order to network byte order
    serverAddress.sin_addr.s_addr = INADDR_ANY; //make it listen to all the available IPs
    
    /******bind******/

    if(bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress))< 0){
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    /******listen******/

    if(listen(serverSocket, MAX_CLIENTS) < 0){
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    /******receive******/

    while(1){
                
        /******accept******/
        sockaddr_in clientAddress{};
        socklent_t clientSize = sizeof(clientAddress);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &clientSize);
        if(clientSocket < 0){
            perror("Accept failed");
            continue;
        }
        
        pthread_t thread;
        int* socketPtr = new int(clientSocket);
        pthread_create(&thread, nullptr, clientHandler, socketPtr);
        pthread_detach(thread);
    }

    /*******close******/
    
    close(serverSocket);

}
