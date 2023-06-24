#include <iostream>
#include <winsock2.h>
#include <thread>
#include <string>
#include <sstream>
#include "Log.h"
#include "Anisette.h"
#include <map>

void handleClient(SOCKET clientSocket) {
    // Handle the client connection
    // ...
    // Read and write to the clientSocket
    char buffer[4096];
    int bytesRead;

    // Read data from the clientSocket
    bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead == SOCKET_ERROR) {
        std::stringstream ss;
        ss << "Failed to read from client socket: " << WSAGetLastError();
        info(ss.str());

        closesocket(clientSocket);
        return;
    }

    if (bytesRead == 0) {
        info("Client disconnected.");
        closesocket(clientSocket);
        return;
    }
    buffer[bytesRead] = '\0';

    // assign to std::string
    std::string http_res = buffer;

    info("Got Anisette request!");

    std::string header[] = {
        "HTTP/1.1 200 OK\n", 
        "Content-Type: application/json\n",
        "\n"
    };

    std::stringstream ss;
    for (const std::string& str : header) {
        ss << str;
    }

    // Append AnisettaData
    auto anisetteData = fetchAnisette();
    if (!anisetteData) {
        error("Anisette Data failed!");
        return;
    }

    // Since there is only key(str) => val(str)
    // we can just serialize it manually
    std::stringstream json_msg;
    json_msg << "{ ";

    int i = 0;
    for (const auto& elem : *anisetteData)
    {
        // last element
        if (i == anisetteData->size() - 1) {
            json_msg << "\"" << elem.first << "\": \"" << elem.second << "\"";
            break;
        } else {
            json_msg << "\"" << elem.first << "\": \"" << elem.second << "\", ";
        }

        i++;
    }

    json_msg << " }";

    ss << json_msg.str();

    std::string message = ss.str();
    send(clientSocket, message.c_str(), message.size(), 0);

    // Close the client socket
    closesocket(clientSocket);
}

bool setup_http_server() {
    info("Initializing winsock.");

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        error("Could not initialize winsock.");
        return true;
    }

    info("Opening socket.");
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == INVALID_SOCKET) {
        error("Could not create socket.");
        std::cerr << WSAGetLastError();
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY; // Listen on all available interfaces
    serverAddress.sin_port = htons(8080); // Use port 8080

    if (bind(listenSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        error("Could not bind to socket. Make sure port 8080 is available. Windows error:");
        std::cerr << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        error("Could not listen on socket.");
        std::cerr << "Failed to listen: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    while (true) {
        // Accept a client socket
        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Failed to accept client socket: " << WSAGetLastError() << std::endl;
            closesocket(listenSocket);
            WSACleanup();
            return 1;
        }

        info("Got HTTP request.");
        // Create a new thread to handle the client connection
        std::thread clientThread(handleClient, clientSocket);
        clientThread.detach();
    }

    // Close the listen socket
    closesocket(listenSocket);

    // Cleanup Winsock
    WSACleanup();

    return 0;
}