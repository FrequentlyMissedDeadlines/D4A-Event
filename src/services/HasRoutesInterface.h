#pragma once

#include <string>
#include <WebServer.h>
#include <set>
/**
 * @file withRoutes.h
 * @brief Interface for classes that can register HTTP routes with a WebServer.
 * @details Classes inheriting from withRoutes should implement methods to register their routes
 *          with a WebServer instance and provide a list of their routes.
 */
struct HasRoutesInterface {
public: 
    /**
     * @fn: registerRoutes
     * @brief: Register HTTP routes with the provided WebServer instance.
     * @param server: Pointer to the WebServer instance.
     * @param basePath: Base path to prefix to all routes.
     * @return: true if registration was successful, false otherwise.
     */
    virtual bool registerRoutes(WebServer *server, std::string basePath) = 0;
    /**
     * @fn: getRoutes
     * @brief: Get the set of HTTP routes provided by this module.
     * @return: A set of strings representing the routes.   
     */
    virtual std::set<std::string> getRoutes() { return routes; };
    
    virtual ~HasRoutesInterface() = default;

protected:
    std::set<std::string> routes;
};