# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++11 -Wall -pthread

# Targets
all: server client

# Server compilation
server: server.cpp
	$(CXX) $(CXXFLAGS) -o server server.cpp

# Client compilation
client: client.cpp
	$(CXX) $(CXXFLAGS) -o client client.cpp

# Clean up compiled files
clean:
	rm -f server client
