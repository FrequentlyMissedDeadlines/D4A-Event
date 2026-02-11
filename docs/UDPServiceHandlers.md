# UDP Service Message Handler Registration

## Overview

The UDPService now supports callback registration, allowing other classes to register custom handlers for incoming UDP messages. This enables a modular, event-driven architecture where different components can respond to UDP messages independently.

## UDPService Interface

### Header File
Location: [src/services/UDPService.h](../src/services/UDPService.h)

The UDPService implements both `IsServiceInterface` and `IsOpenAPIInterface`, providing:
- Lifecycle management (initialize, start, stop)
- HTTP API endpoints for UDP statistics
- Message handler registration
- Thread-safe message handling

### Callback Type

```cpp
using UDPMessageHandler = std::function<bool(const std::string& message, 
                                              const IPAddress& remoteIP, 
                                              uint16_t remotePort)>;
```

### Registration Methods

#### `int registerMessageHandler(UDPMessageHandler handler)`
- **Purpose**: Register a callback function to handle UDP messages
- **Parameters**: 
  - `handler`: A function or lambda that processes incoming messages
- **Returns**: A unique handler ID (>= 0) on success, or -1 on failure
- **Thread-safe**: Yes

#### `bool unregisterMessageHandler(int handlerId)`
- **Purpose**: Remove a previously registered handler
- **Parameters**:
  - `handlerId`: The ID returned by `registerMessageHandler`
- **Returns**: `true` if handler was found and removed, `false` otherwise
- **Thread-safe**: Yes

## Usage Example

### Simple Lambda Handler

```cpp
UDPService* udpService = ...; // Get your UDP service instance
RollingLogger* logger = ...; // Get your logger instance

// Register a simple handler using a lambda
int handlerId = udpService->registerMessageHandler(
    [logger](const std::string& msg, const IPAddress& ip, uint16_t port) {
        if (logger) {
            std::string log_msg = std::string("Received: ") + msg + " from " + ip.toString().c_str() + ":" + std::to_string(port);
            logger->debug(log_msg);
        }
        return true; // Message handled
    }
);

// Later, unregister when done
udpService->unregisterMessageHandler(handlerId);
```

### Class-Based Handler (RemoteControlService)

The included `RemoteControlService` demonstrates a complete implementation:

```cpp
class RemoteControlService : public IsServiceInterface
{
private:

    UDPService* udp_service;
    ServoService* servo_service;
    int handler_id;
    
    bool handleMessage(const std::string& message, 
                      const IPAddress& remoteIP, 
                      uint16_t remotePort)
    {
        if (message == FPSTR(RemoteControlConsts::cmd_forward)) {
            executeForward();
            return true;
        }
        if (message == FPSTR(RemoteControlConsts::cmd_stop)) {
            executeStop();
            return true;
        }
        return false; // Not our command
    }

public:
    RemoteControlService(UDPService* udpService, ServoService* servoService)
        : udp_service(udpService), servo_service(servoService), handler_id(-1) {}
    
    
    
    bool startService() override
    {
        handler_id = udp_service->registerMessageHandler(
            [this](const std::string& msg, const IPAddress& ip, uint16_t port) {
                return this->handleMessage(msg, ip, port);
            }
        );
        
        if (handler_id >= 0) {
            service_status_ = STARTED;
            status_timestamp_ = millis();
            return true;
        }
        
        service_status_ = START_FAILED;
        status_timestamp_ = millis();
        return false;
    }
    
    bool stopService() override
    {
        if (handler_id >= 0) {
            udp_service->unregisterMessageHandler(handler_id);
            handler_id = -1;
        }
        service_status_ = STOPPED;
        status_timestamp_ = millis();
        return true;
    }
    
    std::string getServiceName() override {
        return fpstr_to_string(FPSTR(RemoteControlConsts::str_service_name));
    }
};
```

## How It Works

1. **Registration**: When you call `registerMessageHandler()`, your callback is added to an internal list
2. **Message Reception**: When a UDP packet arrives, `handleUDPPacket()` is invoked
3. **Handler Invocation**: All registered handlers are called in registration order
4. **Message Logging**: After handlers run, the message is logged to the internal buffer for API access
5. **Thread Safety**: All handler operations are protected by a mutex

## Design Considerations

### Handler Execution Order
- Handlers are invoked in the order they were registered
- Each handler receives every UDP message
- Handlers run on the UDP task core for optimal responsiveness

### Return Value
- Return `true` if your handler processed the message
- Return `false` if the message wasn't relevant to your handler
- The return value is currently informational (all handlers still get called)

### Exception Safety
- Handler exceptions are caught and ignored to prevent crashing the UDP service
- Design your handlers to be exception-safe

### Performance
- Keep handlers fast - they run synchronously on each UDP packet
- Avoid blocking operations (disk I/O, long computations, delays)
- For heavy processing, queue the message and process in your own task
- Handler mutex timeout is 10ms - if exceeded, handlers are skipped for that packet

### Memory
- Each handler adds one entry to a `std::vector`
- Callback overhead is minimal (one `std::function` per handler)

## Command Examples

With RemoteControlService running, you can send UDP commands:

**Available Commands** (defined in `RemoteControlConsts`):
- `up`, `down`, `left`, `right` - D-pad controls
- `circle`, `square`, `triangle`, `cross` - Button commands
- `forward`, `backward`, `turn_left`, `turn_right` - Movement commands
- `stop` - Stop all movement

```bash
# Linux/Mac
echo "forward" | nc -u <robot-ip> 24642
echo "turn_left" | nc -u <robot-ip> 24642
echo "stop" | nc -u <robot-ip> 24642

# Python
import socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.sendto(b"forward", ("<robot-ip>", 24642))
sock.sendto(b"stop", ("<robot-ip>", 24642))
```

## Best Practices

1. **Store Handler IDs**: Always save the returned handler ID so you can unregister later
2. **Unregister on Cleanup**: Call `unregisterMessageHandler()` in your destructor or `stopService()`
3. **Check Return Value**: Verify that registration succeeded (handlerId >= 0)
4. **Validate Input**: Always validate message content before acting on it
5. **Use Member Functions**: Capture `this` in lambdas to access member functions safely
6. **Log Errors**: Use the logger service for debugging handler issues

## Thread Safety Notes

- Handler registration/unregistration is protected by `handler_mutex`
- Handler invocation is also protected by the same mutex
- The UDP service can be safely used from multiple tasks
- Message buffer access uses a separate `messageMutex` for statistics
