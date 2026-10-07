#pragma once

#include <Arduino.h>
#include <r_sci_uart.h>
#include <stddef.h>
#include <stdint.h>

// Nano R4, Arduino Renesas core 1.6.0: SCI2 RX=D0/P301, TX=D1/P302.
// Exclusive peripheral ownership: never call Serial1.begin()/write().
class R4ExBusUart {
public:
    struct Byte { uint8_t value; uint32_t receivedUs; };
    bool begin(uint32_t baud);
    bool setBaud(uint32_t baud);
    size_t available() const;
    bool pop(Byte& byte);
    bool trySend(const uint8_t* bytes, size_t count);
    void service(uint32_t nowUs);
    bool transmitting() const { return txBusy_; }
    uint32_t errors() const { return errors_; }
    uint32_t overflows() const { return overflows_; }
    uint32_t baud() const { return baud_; }
private:
    static void callback(uart_callback_args_t* args);
    void onEvent(uart_callback_args_t* args);
    void releaseTx();
    static constexpr uint16_t kRxCapacity = 128;
    static constexpr uint16_t kRxMask = kRxCapacity - 1;
    Byte rx_[kRxCapacity]{};
    uint8_t tx_[64]{};
    volatile uint16_t head_ = 0;
    volatile uint16_t tail_ = 0;
    volatile bool txBusy_ = false;
    volatile uint32_t errors_ = 0;
    volatile uint32_t overflows_ = 0;
    uint32_t baud_ = 0;
    uint32_t txStartedUs_ = 0;
    uint32_t txMaximumUs_ = 0;
    bool opened_ = false;
    bool irqsAllocated_ = false;
    sci_uart_instance_ctrl_t ctrl_{};
    uart_cfg_t cfg_{};
    baud_setting_t baudSetting_{};
    sci_uart_extended_cfg_t extended_{};
};
