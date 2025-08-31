#ifndef REGISTRY_H
#define REGISTRY_H

#include <atomic>
#include <etcd/Client.hpp>
#include <etcd/KeepAlive.hpp>
#include <memory>
#include <string>
#include <thread>

/**
 * @brief Registry implementation using etcd for service discovery.
 * 
 * This class provides functionality to register services with an etcd cluster,
 * maintain service registration through keep-alive mechanisms, and unregister
 * services when they are no longer available.
 */
class etcdRegistry {
public:
    /**
     * @brief Construct a new etcd Registry object.
     * 
     * @param endpoint The etcd server endpoint to connect to.
     */
    etcdRegistry(const std::string &endpoint);
    
    /**
     * @brief Destroy the etcd Registry object.
     * 
     * Automatically unregisters the service and cleans up resources.
     */
    ~etcdRegistry();
    
    /**
     * @brief Register a service with the etcd registry.
     * 
     * Creates a lease and registers the service under the specified name and address.
     * Starts a keep-alive thread to maintain the registration.
     * 
     * @param service_name The name of the service to register.
     * @param service_addr The address of the service to register.
     * @return true if registration was successful, false otherwise.
     */
    bool Register(const std::string &service_name, const std::string &service_addr);
    
    /**
     * @brief Unregister the service from the etcd registry.
     * 
     * Stops the keep-alive thread and revokes the lease, effectively
     * removing the service from the registry.
     */
    void Unregister();
    
private:
    /**
     * @brief Keep-alive thread function to maintain service registration.
     * 
     * Periodically sends keep-alive requests to etcd to prevent the lease
     * from expiring and the service from being automatically unregistered.
     */
    void KeepAlive();
    
    int64_t lease_id_; ///< The etcd lease ID for the registered service.
    std::unique_ptr<etcd::Client> etcd_client_; ///< etcd client for communication.
    std::unique_ptr<etcd::KeepAlive> keep_alive_; ///< Keep-alive manager for the lease.
    std::string etcd_addr_; ///< The full etcd key for the registered service.
    std::thread keepalive_thread_; ///< Thread for running keep-alive operations.
    std::atomic<bool> stop_; ///< Flag to signal the keep-alive thread to stop.
};
#endif // REGISTRY_H