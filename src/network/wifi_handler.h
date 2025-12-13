class WifiModule{
public:
    bool open_access_point();
    bool connect_to_wifi(const char* ssid, const char* password);
    bool disconnect_from_wifi(  );
    bool connect_and_fallback(const char* ssid, const char* password);
};