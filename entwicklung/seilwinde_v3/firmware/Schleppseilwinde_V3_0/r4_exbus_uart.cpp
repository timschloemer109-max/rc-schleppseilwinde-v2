#include "r4_exbus_uart.hpp"
#include <IRQManager.h>
#include <string.h>

#if !defined(ARDUINO_NANO_R4)
#error "R4 EX Bus adapter is qualified only for Nano R4 / Renesas core 1.6.0."
#endif

namespace {
// Save/restore PRIMASK instead of unconditionally enabling caller interrupts.
class CriticalSection {
public:
    CriticalSection() : previous_(__get_PRIMASK()) { __disable_irq(); }
    ~CriticalSection() { __set_PRIMASK(previous_); }
private:
    uint32_t previous_;
};
constexpr bsp_io_port_pin_t kRxPin = BSP_IO_PORT_03_PIN_01;
constexpr bsp_io_port_pin_t kTxPin = BSP_IO_PORT_03_PIN_02;
constexpr uint32_t kSciPinConfig = IOPORT_CFG_PERIPHERAL_PIN | IOPORT_PERIPHERAL_SCI0_2_4_6_8;
}

bool R4ExBusUart::begin(uint32_t baud) {
    if (opened_) return setBaud(baud);
    if (baud != 125000 && baud != 250000) return false;
    if (R_SCI_UART_BaudCalculate(baud, false, 1000, &baudSetting_) != FSP_SUCCESS) return false;
    extended_.clock = SCI_UART_CLOCK_INT;
    extended_.rx_edge_start = SCI_UART_START_BIT_FALLING_EDGE;
    extended_.noise_cancel = SCI_UART_NOISE_CANCELLATION_DISABLE;
    extended_.rx_fifo_trigger = SCI_UART_RX_FIFO_TRIGGER_1;
    extended_.p_baud_setting = &baudSetting_;
    extended_.flow_control = SCI_UART_FLOW_CONTROL_RTS;
    extended_.flow_control_pin = static_cast<bsp_io_port_pin_t>(UINT16_MAX);
    extended_.rs485_setting.enable = SCI_UART_RS485_DISABLE;
    extended_.rs485_setting.de_control_pin = static_cast<bsp_io_port_pin_t>(UINT16_MAX);
    cfg_.channel = 2;
    cfg_.data_bits = UART_DATA_BITS_8;
    cfg_.parity = UART_PARITY_OFF;
    cfg_.stop_bits = UART_STOP_BITS_1;
    cfg_.p_callback = callback;
    cfg_.p_context = this;
    cfg_.p_extend = &extended_;
    if (!irqsAllocated_) {
        cfg_.txi_irq = FSP_INVALID_VECTOR;
        cfg_.tei_irq = FSP_INVALID_VECTOR;
        cfg_.rxi_irq = FSP_INVALID_VECTOR;
        cfg_.eri_irq = FSP_INVALID_VECTOR;
        if (!IRQManager::getInstance().addPeripheral(IRQ_SCI_UART, &cfg_)) return false;
        irqsAllocated_ = true;
    }
    // Input until an explicitly accepted response; no driven idle bus.
    if (R_IOPORT_PinCfg(&g_ioport_ctrl, kRxPin, kSciPinConfig) != FSP_SUCCESS) return false;
    if (R_IOPORT_PinCfg(&g_ioport_ctrl, kTxPin, IOPORT_CFG_PORT_DIRECTION_INPUT | IOPORT_CFG_PULLUP_ENABLE) != FSP_SUCCESS) return false;
    if (R_SCI_UART_Open(&ctrl_, &cfg_) != FSP_SUCCESS) return false;
    opened_ = true;
    baud_ = baud;
    releaseTx();
    return true;
}

bool R4ExBusUart::setBaud(uint32_t baud) {
    if (!opened_ || txBusy_ || (baud != 125000 && baud != 250000)) return false;
    baud_setting_t setting{};
    if (R_SCI_UART_BaudCalculate(baud, false, 1000, &setting) != FSP_SUCCESS) return false;
    CriticalSection guard;
    if (txBusy_) return false;
    if (R_SCI_UART_BaudSet(&ctrl_, &setting) != FSP_SUCCESS) return false;
    baudSetting_ = setting;
    baud_ = baud;
    head_ = tail_ = 0;
    // Baud change invalidates all partially received frames.
    ++errors_;
    releaseTx();
    return true;
}

size_t R4ExBusUart::available() const {
    return static_cast<uint16_t>(head_ - tail_) & kRxMask;
}

bool R4ExBusUart::pop(Byte& byte) {
    CriticalSection guard;
    if (tail_ == head_) return false;
    byte = rx_[tail_];
    tail_ = (tail_ + 1U) & kRxMask;
    return true;
}

bool R4ExBusUart::trySend(const uint8_t* bytes, size_t count) {
    if (!opened_ || bytes == nullptr || count == 0 || count > sizeof(tx_) || txBusy_) return false;
    // Private lifetime-stable buffer consumed asynchronously by FSP interrupts.
    memcpy(tx_, bytes, count);
    CriticalSection guard;
    // No response may take the line while newer master data awaits processing.
    // Check after disabling IRQs, closing the main/ISR race of available().
    if (txBusy_ || head_ != tail_ || ctrl_.p_reg->SSR_b.RDRF != 0 ||
        NVIC_GetPendingIRQ(cfg_.rxi_irq) != 0) return false;
    txBusy_ = true;
    txStartedUs_ = micros();
    txMaximumUs_ = static_cast<uint32_t>((count * 10000000ULL + baud_ - 1U) / baud_) + 200U;
    // Receiver is deliberately disabled during own response. EX Bus master
    // owns the bus except for its documented response window; self echo is
    // never parsed as new channel data.
    ctrl_.p_reg->SCR_b.RIE = 0;
    ctrl_.p_reg->SCR_b.RE = 0;
    ctrl_.p_reg->SCR_b.TE = 1;
    if (R_IOPORT_PinCfg(&g_ioport_ctrl, kTxPin, kSciPinConfig) != FSP_SUCCESS ||
        R_SCI_UART_Write(&ctrl_, tx_, static_cast<uint32_t>(count)) != FSP_SUCCESS) {
        ++errors_;
        releaseTx();
        txBusy_ = false;
        return false;
    }
    return true;
}

void R4ExBusUart::service(uint32_t nowUs) {
    if (!opened_ || !txBusy_ || static_cast<uint32_t>(nowUs - txStartedUs_) < txMaximumUs_) return;
    CriticalSection guard;
    if (!txBusy_) return;
    // A lost/wedged TEI must never leave the bus actively driven forever.
    // FSP Abort only disables transfer interrupts/registers; it does not wait.
    R_SCI_UART_Abort(&ctrl_, UART_DIR_TX);
    releaseTx();
    txBusy_ = false;
    ++errors_;
}

void R4ExBusUart::releaseTx() {
    ctrl_.p_reg->SCR_b.TE = 0;
    // A weak pull-up is not an active HIGH driver; the master may drive LOW.
    R_IOPORT_PinCfg(&g_ioport_ctrl, kTxPin, IOPORT_CFG_PORT_DIRECTION_INPUT | IOPORT_CFG_PULLUP_ENABLE);
    ctrl_.p_reg->SCR_b.RE = 1;
    ctrl_.p_reg->SCR_b.RIE = 1;
}

void R4ExBusUart::callback(uart_callback_args_t* args) {
    if (args == nullptr || args->p_context == nullptr) return;
    auto* instance = static_cast<R4ExBusUart*>(const_cast<void*>(args->p_context));
    instance->onEvent(args);
}

void R4ExBusUart::onEvent(uart_callback_args_t* args) {
    if (args->event == UART_EVENT_TX_COMPLETE) {
        // TX_DATA_EMPTY still has a byte in flight. Only TEI/TX_COMPLETE is
        // allowed to disconnect TX after the final stop bit.
        releaseTx();
        txBusy_ = false;
    } else if (args->event == UART_EVENT_RX_CHAR && !txBusy_) {
        const uint16_t next = (head_ + 1U) & kRxMask;
        if (next == tail_) {
            ++overflows_;
            ++errors_;
            head_ = tail_ = 0;
            return;
        }
        rx_[head_].value = static_cast<uint8_t>(args->data);
        rx_[head_].receivedUs = micros();
        __DMB();
        head_ = next;
    } else if (args->event == UART_EVENT_ERR_PARITY || args->event == UART_EVENT_ERR_FRAMING ||
               args->event == UART_EVENT_ERR_OVERFLOW || args->event == UART_EVENT_BREAK_DETECT) {
        ++errors_;
        head_ = tail_ = 0;
    }
}
