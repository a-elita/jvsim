//
//  AgentSystem.hpp
//  Telemetry Agent
//
//  Created: 2/21/16.
//
//  Authors: NITIN KUMAR
//           ABBAS SAKARWALA
//
//  Copyright © 2016 Juniper Networks. All rights reserved.
//

#ifndef AgentSystem_hpp
#define AgentSystem_hpp

#include "AgentServerProtos.h"
#include "AgentServerLog.hpp"

// Unique Identifier to track an outstanding system request
class SystemId {
    size_t _id;

public:
    SystemId(size_t value) : _id(value) {}

    size_t getId()
    {
        return _id;
    }

    void description ()
    {
        std::cout << "Id = " << _id << std::endl;
    }
};

// Interface presented by a system where all telemetry resources are provisioned
class AgentSystem {

protected:
    AgentServerLog *_logger;

    // Statistics to track requests into the system
    uint64_t _add_system_count;
    uint64_t _remove_system_count;
    uint64_t _error_system_count;

public:
    AgentSystem (AgentServerLog *logger) : _logger(logger)
    {
        _add_system_count = _remove_system_count = _error_system_count = 0;
    }

    virtual ~AgentSystem () {};

    AgentServerLog  *getLogger (void)       { return _logger; }

    virtual bool systemAdd(SystemId sys_id,
                           const telemetry::Path *request_path) = 0;
    virtual bool systemRemove(SystemId sys_id,
                              const telemetry::Path *request_path) = 0;
    virtual telemetry::Path * systemGet(SystemId sys_id) = 0;
    virtual bool systemClearAll(void) = 0;
};

#endif /* AgentSystem_hpp */