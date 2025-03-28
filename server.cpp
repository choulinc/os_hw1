#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <vector>
#include <map>
#include <sstream>

using namespace std;

const int MAX_CLIENTS = 5;

struct ClientInfo{
   string username;
   int socket_fd;
   sockaddr_in address;
   bool is_online;

   ClientInfo() : username(""), socket_fd(-1), address{}, is_online(false) {}

   // Parameterized constructor
   ClientInfo(const string& user, int fd, const sockaddr_in& addr) 
       : username(user), socket_fd(fd), address(addr), is_online(true) {}

   // Copy constructor
   ClientInfo(const ClientInfo& other) 
       : username(other.username), socket_fd(other.socket_fd), address(other.address), is_online(other.is_online) {}

   // Assignment operator
   ClientInfo& operator=(const ClientInfo& other) {
       if (this != &other) {
           username = other.username;
           socket_fd = other.socket_fd;
           address = other.address;
           is_online = other.is_online;
       }
       return *this;
   }
};


/******username -> ClientInfo******/
map<string, ClientInfo> clients;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void systemMessage(const string& message, int sender_fd){
	pthread_mutex_lock(&clients_mutex);
    for(auto& pair: clients){
        if(pair.second.socket_fd != sender_fd && pair.second.is_online){
            send(pair.second.socket_fd, message.c_str(), message.length(), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void* clientHandler(void* clientSocket){
    int socket_fd = *(int*)clientSocket;
    delete (int*)clientSocket;
    char buffer[1024];

    memset(buffer, 0, sizeof(buffer));
    int byteReceived = recv(socket_fd, buffer, sizeof(buffer) - 1, 0);
    if(byteReceived <= 0){
        if (byteReceived == 0) {
            cout << "Client disconnected normally.\n";
        } else {
            perror("recv failed");
        }
        close(socket_fd);
        return nullptr;
    }
	cout << "Received message: " << buffer << endl; 

    string receivedMessage(buffer);
    size_t pos = receivedMessage.find(" ");
    if(pos == string::npos){
		cout << "Invalid message format\n";
        close(socket_fd);
        return nullptr;
    }

    string command = receivedMessage.substr(0, pos);
    string username = receivedMessage.substr(pos + 1);

    if(command != "connect"){
		cout << "Invalid command: " << command << "\n";
        close(socket_fd);
        return nullptr;
    }
    
    sockaddr_in clientAddress;
    socklen_t addressLength = sizeof(clientAddress);
    getpeername(socket_fd, (struct sockaddr*)&clientAddress, &addressLength);

    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddress.sin_addr, clientIP, INET_ADDRSTRLEN);
    int clientPort = ntohs(clientAddress.sin_port);

    stringstream onlineMsg;
    onlineMsg << "<User " << username << " is on-line, socket address: " << clientIP << "/" << clientPort << ".>";

    pthread_mutex_lock(&clients_mutex);
    clients[username] = {username, socket_fd, clientAddress};
    pthread_mutex_unlock(&clients_mutex);

    systemMessage(onlineMsg.str(), socket_fd);

    cout << onlineMsg.str() << endl;

    while(true){
        memset(buffer, 0, sizeof(buffer));
        byteReceived = recv(socket_fd, buffer, sizeof(buffer) - 1, 0);
        if(byteReceived <= 0){
			cout << "Disconnected or error in receiving data.\n";
            break;
        }

        string msg(buffer);
        if(msg == "bye"){
            stringstream offlineMsg;
            offlineMsg << "<User " << username << " is off-line.>\n";

            systemMessage(offlineMsg.str(), socket_fd);

            cout << offlineMsg.str() << endl;
            break;
        }
        
        pos = msg.find(" ");
        if(pos != string::npos && msg.substr(0, pos) == "chat"){
            size_t pos2 = msg.find(" ", pos + 1);
            if(pos2 != string::npos){
                string targetUser = msg.substr(pos + 1, pos2 - pos - 1);
                string message = msg.substr(pos2 + 1);

                pthread_mutex_lock(&clients_mutex);
                auto it = clients.find(targetUser);
                if(it != clients.end()){
                    if(it->second.is_online){
                        // User is online, send message
                        int targetSocket = it->second.socket_fd;
                        string fullMessage = "<From " + username + "> " + message;
                        send(targetSocket, fullMessage.c_str(), fullMessage.length(), 0);
                    }else{
                        // User is offline
                        string offlineMsg = "<User " + targetUser + " is off-line.>";
                        send(socket_fd, offlineMsg.c_str(), offlineMsg.length(), 0);
                    }
                }else{
                    string errorMsg = "<User " + targetUser + " does not exist.>";
                    send(socket_fd, errorMsg.c_str(), errorMsg.length(), 0);
                }
                pthread_mutex_unlock(&clients_mutex);
            }
        }
    }

    pthread_mutex_lock(&clients_mutex);
    auto it = clients.find(username);
    if (it != clients.end()) {
        it->second.is_online = false;
        it->second.socket_fd = -1;
    }
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

    sockaddr_in serverAddress{}; //store the address
    serverAddress.sin_family = AF_INET;    
    serverAddress.sin_port = htons(8080); // convert unsigned int from machine byte order to network byte order
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY); //make it listen to all the available IPs
    
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
        socklen_t clientSize = sizeof(clientAddress);
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
