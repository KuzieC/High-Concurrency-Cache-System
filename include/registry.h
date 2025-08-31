#ifndef REGISTRY_H
#define REGISTRY_H

#include <atomic>
#include <etcd/Client.hpp>
#include <etcd/KeepAlive.hpp>
#include <memory>
#include <string>
#include <thread>

class etcdRegistry {
public:
    etcdRegistry(const std::string &endpoint);
    ~etcdRegistry();
    // service_name: /service_name/service_addr
    bool Register(const std::string &service_name, const std::string &service_addr);
    void Unregister();
private:
    void KeepAlive();
    int64_t lease_id_;
    std::unique_ptr<etcd::Client> etcd_client_;
    std::unique_ptr<etcd::KeepAlive> keep_alive_;
    std::string etcd_addr_;
    std::thread keepalive_thread_;
    std::atomic<bool> stop_;
};
#endif // REGISTRY_H