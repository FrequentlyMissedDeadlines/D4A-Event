#include "LoggerService.h"
#pragma once
struct HasLoggerInterface {
    public:
        bool setLogger(LoggerService* logger) {
            if (!logger) return false;
            this->logger = logger;
            return true;
        };
    protected:
        LoggerService* logger = nullptr;
};
