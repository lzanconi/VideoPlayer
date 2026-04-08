#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <thread>
#include <atomic>
#include <string>
#include <chrono>
#include <iostream>

class NetworkManager
{
private:
    std::string serverIP;
    int port;
    std::thread workerThread;
    std::atomic<bool> running;

public:
    NetworkManager(const std::string& serverIP, int port)
    {
        this->serverIP = serverIP;
        this->port = port;
    }

    ~NetworkManager()
    {
        Stop();
    }

    void Start()
    {
        if (running) 
            return;
        running = true;

        workerThread = std::thread(&NetworkManager::Run, this);
        std::cout << "NetworkManager: Background thread started." << std::endl;
    }

    void Stop()
    {
        running = false;
        if (workerThread.joinable()) 
        {
            workerThread.join();
        }
    }
private:
    void Run()
    {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) 
            return;

        while (running) 
        {
            SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (clientSocket == INVALID_SOCKET) 
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }

            sockaddr_in serverAddr;
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(port);
            inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);

            // Attempt connection
            std::cout << "[Network] Attempting to connect to server at " << serverIP << ":" << port << "..." << std::endl;
            if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) 
            {
                closesocket(clientSocket);
                std::this_thread::sleep_for(std::chrono::seconds(2)); // Retry interval
                continue;
            }

            std::cout << "NetworkManager: Connected to Position Server." << std::endl;
            HandleCommunication(clientSocket);
            closesocket(clientSocket);
        }

        WSACleanup();
    }

    void HandlePositionSend(SOCKET socket)
    {
        // 25 FPS = 1 message every 40ms
        const std::chrono::milliseconds frameDuration(40);
        auto period_duration = frameDuration;
        float last_known_position = 0.0f;

        auto next_frame = std::chrono::steady_clock::now() + period_duration;
        float previousSentPosition = 0.0;
        double current_speed = 0.0;

        while (running)
        {
            auto now = std::chrono::steady_clock::now();
            auto time_left = std::chrono::duration_cast<std::chrono::milliseconds>(next_frame - now);
            if (time_left.count() > 2)
                std::this_thread::sleep_for(time_left - std::chrono::milliseconds(2));

            while (std::chrono::steady_clock::now() < next_frame)
            {
                /* spin until it's time to send */
            }

            auto trigger_time = std::chrono::steady_clock::now();
            int64_t trigger_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(trigger_time.time_since_epoch()).count();

            float pos_value = 0.0f;
            double progress_pct = 0.0;


        }
    }

    void HandleCommunication(SOCKET socket) 
    {
        // 25 FPS = 1 message every 40ms
        const std::chrono::milliseconds frameDuration(40);

        while (running) 
        {
            auto startTime = std::chrono::steady_clock::now();

            const char* msg = "FRAME_SYNC";
            if (send(socket, msg, (int)strlen(msg), 0) == SOCKET_ERROR) 
            {
                break; // Connection lost, return to NetworkLoop to reconnect
            }

            auto endTime = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

            if (elapsed < frameDuration) 
            {
                std::this_thread::sleep_for(frameDuration - elapsed);
            }
        }
    }
};