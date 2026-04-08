#include "server.h"

Server::Server(int port, size_t cache_capacity)
    : port_(port), cache_capacity_(cache_capacity) {}

void Server::start() {}
void Server::stop() {}
