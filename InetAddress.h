#pragma once
#include <string>
#include <arpa/inet.h>
/**
 * 封装socket地址类型，IP地址和端口号
 * 
 */

class InetAddress {
public:
    explicit InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1");
    explicit InetAddress(const sockaddr_in &addr) :addr_(addr) {}

    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t toPort() const;
    void setSockAddr(const sockaddr_in &addr) { addr_ = addr; }

    const sockaddr_in *getSockAddr() const { return &addr_;}
private:
    sockaddr_in addr_;

};