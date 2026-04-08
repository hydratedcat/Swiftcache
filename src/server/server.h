#pragma once
#include <string>

class Server {
public:
    Server(int port, size_t cache_capacity);
    void start();
    void stop();

private:
    int port_;
    size_t cache_capacity_;
};
