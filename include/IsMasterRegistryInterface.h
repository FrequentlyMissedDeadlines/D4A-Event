#pragma once

#include <string>

/**
 * @file IsMasterRegistryInterface.h
 * @brief Interface for services that track a single registered master client.
 * @details Provides a common abstraction for master-authorization checks so that
 *          other services can enforce master-only access without depending directly
 *          on AmakerBotService.
 */
struct IsMasterRegistryInterface
{
    virtual ~IsMasterRegistryInterface() = default;

    /**
     * @brief Check whether the given IP is the currently registered master.
     * @param ip IP address string to check
     * @return true if ip matches the registered master IP
     */
    virtual bool isMaster(const std::string &ip) const = 0;

    /**
     * @brief Return the currently registered master IP address.
     * @return Master IP string, empty if no master is registered
     */
    virtual std::string getMasterIP() const = 0;
};
