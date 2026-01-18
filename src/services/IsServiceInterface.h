#pragma once

struct IsServiceInterface {
    virtual bool init() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    
    virtual ~IsServiceInterface() = default;
};