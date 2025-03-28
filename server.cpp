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

/**client information structure**/

struct ClientInfo{
    string username;
    int socket_fd;
    sockaddr_in address;
    bool is_online;

    //constructors
    ClientInfo() : username(""), socket_fd(-1), address{}, is_online(false) {}

    ClientInfo(const string& user, int fd) : username(user), socket_fd(fd), address{}, is_online(true) {}

    ClientInfo(const string& user, int fd, const sockaddr_in& addr) : username(user), socket_fd(fd), address(addr), is_online(true) {}

    ClientInfo(const ClientInfo& other) : username(other.username), socket_fd(other.socket_fd), address(other.address), is_online(other.is_online) {}

    ClientInfo& operator=(const ClientInfo& other){
        if(this != &other){
            username = other.username;
            socket_fd = other.socket_fd;
            address = other.address;
            is_online = other.is_online;
        }
        return *this;
    }
};

/**whiteboard unit to store in the vector**/

struct whiteboardUnit{
    string sender;
    string receiver;
    string message;
    time_t timestamp;//the sending time
};

/******username -> ClientInfo map******/
map<string, ClientInfo> clients;

/**whiteboard vector**/
vector<whiteboardUnit> whiteboard;

/**pthread of clients and whiteboard**/
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t whiteboard_mutex = PTHREAD_MUTEX_INITIALIZER;

/**transform time t to [yyyy month dd, hh mm ss] as a string**/
string getFormattedTimestamp(time_t t) {
    char buffer[80];
    struct tm* timeinfo = localtime(&t);
    strftime(buffer, sizeof(buffer), "%Y %B %d, %H:%M:%S", timeinfo);
    return string(buffer);
}

/**to print all the whiteboard**/
void printWhiteboard(){
    if(pthread_mutex_trylock(&whiteboard_mutex) != 0){//檢查是否遇到deadlock
        //cout << "DEBUG: mutex locked, deadlock" << endl;
    }else{
        //cout << "DEBUG: locked whiteboard_mutex in addWhiteboardList()" << endl;
        pthread_mutex_unlock(&whiteboard_mutex);
    }
    pthread_mutex_lock(&whiteboard_mutex);
	//cout << "DEBUG: Entering printWhiteboard()" << endl;
    for(const auto& entry : whiteboard){
        cout << "$ chat " << entry.receiver 
            << " \"" << entry.message << "\"\n";
        cout << "[" << getFormattedTimestamp(entry.timestamp) 
            << "] " << entry.sender << " is using the whiteboard.\n";
	    cout << "<To " << entry.receiver << "> " << entry.message << "\n"; 
    }
    pthread_mutex_unlock(&whiteboard_mutex);
}

/**to add entries in the whiteboard list**/
void addWhiteboardList(const string& sender, const string& receiver, const string& message){
    pthread_mutex_lock(&whiteboard_mutex);
	/*
    cout << "DEBUG: adding whiteboard entry" << endl;
    cout << "Sender: " << sender << endl;
    cout << "Receiver: " << receiver << endl;
    cout << "Message: " << message << endl;
	*/
    try{
        whiteboardUnit entry{sender, receiver, message, time(nullptr)};
        whiteboard.push_back(entry);
        //cout << "calling printWhileboard()\n";
        pthread_mutex_unlock(&whiteboard_mutex);
        printWhiteboard(); 
        //cout << "printWhileboard()\n";
    }catch(const std::exception& e){
        cerr << "Error while adding whiteboard entries: " << endl;
        pthread_mutex_unlock(&whiteboard_mutex);
    }
}


/**warning when someone is trying to send message to someone was online but now is off-line**/
void systemMessage(const string& message, int sender_fd){
	pthread_mutex_lock(&clients_mutex);
    for(auto& pair: clients){
        if(pair.second.socket_fd != sender_fd && pair.second.is_online){
            send(pair.second.socket_fd, message.c_str(), message.length(), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

/**handle input strings**/
void* clientHandler(void* clientSocket){
    int socket_fd = *(int*)clientSocket;
    delete (int*)clientSocket;
    char buffer[1024];

    memset(buffer, 0, sizeof(buffer));
    int byteReceived = recv(socket_fd, buffer, sizeof(buffer) - 1, 0);

    if(byteReceived <= 0){
        if(byteReceived == 0){
            cout << "Client disconnected normally.\n";
        }else{
            perror("recv failed");
        }
        close(socket_fd);
        return nullptr;
    }
	//cout << "Received message: " << buffer << endl; 

    string receivedMessage(buffer);
    size_t pos = receivedMessage.find(" ");
    if(pos == string::npos){
		cout << "Invalid message format\n";
        close(socket_fd);
        return nullptr;
    }

    /**split the string into command and username parts**/
    string command = receivedMessage.substr(0, pos);
    string username = receivedMessage.substr(pos + 1);

    if(command != "connect"){//the connect command
		cout << "Invalid command: " << command << "\n";
        close(socket_fd);
        return nullptr;
    }

    pthread_mutex_lock(&clients_mutex);
    clients[username] = ClientInfo(username, socket_fd);
    pthread_mutex_unlock(&clients_mutex);
	sockaddr_in clientAddress;
    socklen_t addressLength = sizeof(clientAddress);
    getpeername(socket_fd, (struct sockaddr*)&clientAddress, &addressLength);

	char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddress.sin_addr, clientIP, INET_ADDRSTRLEN);
    int clientPort = ntohs(clientAddress.sin_port);

    /**announce when somebody else is online**/
    stringstream onlineMsg;
    onlineMsg << "<User " << username << " is on-line, socket address: " << clientIP << "/" << clientPort << ".>";

    pthread_mutex_lock(&clients_mutex);
    clients[username] = ClientInfo(username, socket_fd, clientAddress);
    pthread_mutex_unlock(&clients_mutex);

    systemMessage(onlineMsg.str(), socket_fd);

    cout << onlineMsg.str() << endl;

    //cout << "<User " << username << " is online.>\n";

    while(true){
        memset(buffer, 0, sizeof(buffer));
        byteReceived = recv(socket_fd, buffer, sizeof(buffer) - 1, 0);
        if(byteReceived <= 0){
			cout << "Disconnected or error in receiving data.\n";
            break;
        }

        string msg(buffer);
        if(msg == "bye"){//quit the connection
            stringstream offlineMsg;
            offlineMsg << "<User " << username << " is off-line.>\n";

            systemMessage(offlineMsg.str(), socket_fd);

            cout << offlineMsg.str() << endl;
            break;
        }
        
        pos = msg.find(" ");
        if(pos != string::npos && msg.substr(0, pos) == "chat"){//the chat command
            size_t pos2 = msg.find(" ", pos + 1);//the message
            if(pos2 != string::npos){
                string targetUser = msg.substr(pos + 1, pos2 - pos - 1);
                string message = msg.substr(pos2 + 1);

				message.erase(0, message.find_first_not_of(" \""));//handle redundant endline
                message.erase(message.find_last_not_of(" \"\n") + 1);

                pthread_mutex_lock(&clients_mutex);
				auto it = clients.find(targetUser);
                if(it != clients.end()){
                    if(it->second.is_online){
                        //target user is online, send the message
                        int targetSocket = it->second.socket_fd;
                        string fullMessage = "<From " + username + "> " + message;
                        send(targetSocket, fullMessage.c_str(), fullMessage.length(), 0);
						addWhiteboardList(username, targetUser, message);
                    }else{
                        //target user is offline
                        string offlineMsg = "<User " + targetUser + " is off-line.>";
                        send(socket_fd, offlineMsg.c_str(), offlineMsg.length(), 0);
            			addWhiteboardList(username, targetUser, message);
                    }
                }else{//user trying to send to someone not connected ever
                    string errorMsg = "<User " + targetUser + " does not exist.>";
                    send(socket_fd, errorMsg.c_str(), errorMsg.length(), 0);
					//cout << "DEBUG: calling addWhiteboardEntry()" << endl;
					addWhiteboardList(username, targetUser, message);
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
