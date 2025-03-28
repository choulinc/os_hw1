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

struct WhiteboardEntry {
    string sender;
    string receiver;
    string message;
    time_t timestamp;
};

/******username -> ClientInfo******/
map<string, ClientInfo> clients;
vector<WhiteboardEntry> whiteboard;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t whiteboard_mutex = PTHREAD_MUTEX_INITIALIZER;

string getFormattedTimestamp(time_t t) {
    char buffer[80];
    struct tm* timeinfo = localtime(&t);
    strftime(buffer, sizeof(buffer), "%Y %B %d, %H:%M:%S", timeinfo);
    return string(buffer);
}

void printWhiteboard() {
    pthread_mutex_lock(&whiteboard_mutex);
    cout << "\n--- Whiteboard Contents ---\n";
    for (const auto& entry : whiteboard) {
        cout << "$ chat " << entry.receiver 
             << " \"" << entry.message << "\"\n";
        cout << "[" << getFormattedTimestamp(entry.timestamp) 
             << "] " << entry.sender << " is using the whiteboard.\n";
        cout << "Message: " << entry.message << "\n\n";
    }
    cout << "-------------------------\n";
    pthread_mutex_unlock(&whiteboard_mutex);
}

void addWhiteboardEntry(const string& sender, const string& receiver, const string& message) {
    pthread_mutex_lock(&whiteboard_mutex);

	cout << "DEBUG: Adding whiteboard entry" << endl;
    cout << "Sender: " << sender << endl;
    cout << "Receiver: " << receiver << endl;
    cout << "Message: " << message << endl;

	try {
        WhiteboardEntry entry{sender, receiver, message, time(nullptr)};
        whiteboard.push_back(entry);
        printWhiteboard(); 
    } catch (const std::exception& e) {
        cerr << "Error adding whiteboard entry: " << e.what() << endl;
    }


    pthread_mutex_unlock(&whiteboard_mutex);
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

    pthread_mutex_lock(&clients_mutex);
    clients[username] = {username, socket_fd};
    pthread_mutex_unlock(&clients_mutex);

    cout << "<User " << username << " is online.>\n";

    while(true){
        memset(buffer, 0, sizeof(buffer));
        byteReceived = recv(socket_fd, buffer, sizeof(buffer) - 1, 0);
        if(byteReceived <= 0){
			cout << "Disconnected or error in receiving data.\n";
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

				 message.erase(0, message.find_first_not_of(" \""));
                message.erase(message.find_last_not_of(" \"\n") + 1);

                pthread_mutex_lock(&clients_mutex);
                if(clients.find(targetUser) != clients.end()){
                    int targetSocket = clients[targetUser].socket_fd;
					
					 cout << "DEBUG: Attempting to send message" << endl;
    cout << "Target Socket: " << targetSocket << endl;
    cout << "Full Message: [<From " << username << "> " << message << "]" << endl;

                    string fullMessage = "<From " + username + "> " + message;
                    ssize_t bytesSent = send(targetSocket, fullMessage.c_str(), fullMessage.length(), 0);
					
					if(bytesSent < 0){
						perror("Send failed");
						cout << "Error sending message to " << targetUser << endl;
					}else{
						cout << "Successfully sent " << bytesSent << " bytes to " << targetUser << endl;
					}
                }else{
                    string errorMsg = "<User " + targetUser + "does not exist.>";
                    send(socket_fd, errorMsg.c_str(), errorMsg.length(), 0);
					addWhiteboardEntry(username, targetUser, message);
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
