#include <Arduino.h>
#include <Wire.h>

#include "logger/logger.h"
#include "sockets/sockets.h"
#include "wifi/wifi.h"

static const char* TAG = "MAIN";

extern "C" void app_main() {
    initArduino();
    display::init_display();
    D_LOGI(TAG, "Display Initialized");

    wifi::init_wifi();

    static auto svr = sockets::udp::Socket(sockets::udp::SocketType::SERVER, "0.0.0.0", 46729);
    static auto client =
        sockets::udp::Socket(sockets::udp::SocketType::CLIENT, "192.168.0.183", 36729);

    svr.callback = sockets::udp::basic_handle;
    sockets::udp::start_receive(&svr);
    client.send("Client test msg");
}
