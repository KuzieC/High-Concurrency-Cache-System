#ifndef HTTPGATEWAY_H
#define HTTPGATEWAY_H

#include <gflags/gflags.h>
#include <httplib.h>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include "kcache/consistent_hash.h"

DEFINE_int32(http_port, 9000, "HTTP port");
DEFINE_string(etcd_endpoints, "http://127.0.0.1:2379", "etcd address");
DEFINE_string(service_name, "kcache", "cache service name");

/**
 * @brief HTTP gateway for the distributed cache system.
 * 
 * HttpGateway provides a RESTful HTTP interface to the distributed cache system.
 * It acts as a gateway that routes HTTP requests to appropriate cache nodes using
 * consistent hashing and service discovery via etcd. This allows clients to access
 * the cache system through standard HTTP protocols.
 */
class HttpGateway {
public:
    /**
     * @brief Construct a new HttpGateway instance.
     * 
     * @param port The HTTP port to listen on for incoming requests.
     * @param etcd_endpoints The comma-separated list of etcd endpoints for service discovery.
     * @param service_name The name of the cache service to discover in etcd.
     */
    HttpGateway(int port, const std::string &etcd_endpoints, const std::string &service_name);
    
    /**
     * @brief Destructor that properly shuts down the HTTP server and discovery services.
     */
    ~HttpGateway();
    
    /**
     * @brief Start the HTTP gateway service.
     * 
     * This method starts the HTTP server, sets up routes, and begins service discovery.
     * The gateway will start accepting HTTP requests and routing them to cache nodes.
     */
    void StartService();    
private:
    /**
     * @brief Set up HTTP route handlers for cache operations.
     * 
     * Configures the HTTP server with endpoints for GET, SET, and DELETE operations.
     */
    void SetupRoute();
    
    /**
     * @brief Start the service discovery process.
     * 
     * Begins monitoring etcd for available cache service instances and updates
     * the consistent hash ring accordingly.
     */
    void StartDiscovery();
    
    /**
     * @brief Get the cache node responsible for handling a specific key.
     * 
     * @param key The cache key to route.
     * @return The network address of the cache node that should handle this key.
     */
    auto GetCacheClient(std::string &key);
    
    /**
     * @brief Handle HTTP GET requests for cache retrieval.
     * 
     * @param req The incoming HTTP request containing the cache key.
     * @param res The HTTP response to populate with the cached value.
     */
    void Get(const httplib::Request &req, httplib::Response &res);
    
    /**
     * @brief Handle HTTP POST/PUT requests for cache storage.
     * 
     * @param req The incoming HTTP request containing the key and value to store.
     * @param res The HTTP response to indicate operation success.
     */
    void Set(const httplib::Request &req, httplib::Response &res);
    
    /**
     * @brief Handle HTTP DELETE requests for cache key removal.
     * 
     * @param req The incoming HTTP request containing the key to delete.
     * @param res The HTTP response to indicate operation success.
     */
    void Del(const httplib::Request &req, httplib::Response &res);
    
    int port_; ///< The HTTP port this gateway listens on.
    std::string etcd_endpoints_; ///< The etcd endpoints for service discovery.
    std::string service_name_; ///< The name of the cache service to discover.
    httplib::Server http_server_; ///< The underlying HTTP server instance.
    std::shared_ptr<etcd::Client> etcd_client_; ///< etcd client for service discovery.
    std::mutex mtx_; ///< Mutex for thread-safe operations.
    std::thread discovery_thread_; ///< Thread for running service discovery.
    consistentHash consistent_hash_; ///< Consistent hash ring for load balancing.
};

#endif // HTTPGATEWAY_H
