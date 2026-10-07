#pragma once
#include "r_sci_uart.h"
constexpr int IRQ_SCI_UART = 1;
class IRQManager {
public:
    static IRQManager& getInstance() { static IRQManager manager; return manager; }
    bool addPeripheral(int, uart_cfg_t* cfg) { cfg->rxi_irq = 3; return true; }
};
