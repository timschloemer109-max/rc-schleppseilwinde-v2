#pragma once
#include "Arduino.h"
enum uart_event_t { UART_EVENT_TX_COMPLETE, UART_EVENT_TX_DATA_EMPTY, UART_EVENT_RX_CHAR,
    UART_EVENT_ERR_PARITY, UART_EVENT_ERR_FRAMING, UART_EVENT_ERR_OVERFLOW, UART_EVENT_BREAK_DETECT };
struct uart_callback_args_t { uart_event_t event; const void* p_context; uint32_t data; };
struct baud_setting_t { uint32_t baud; };
enum { SCI_UART_CLOCK_INT, SCI_UART_START_BIT_FALLING_EDGE, SCI_UART_NOISE_CANCELLATION_DISABLE,
    SCI_UART_RX_FIFO_TRIGGER_1, SCI_UART_FLOW_CONTROL_RTS, SCI_UART_RS485_DISABLE,
    UART_DATA_BITS_8, UART_PARITY_OFF, UART_STOP_BITS_1, UART_DIR_TX };
struct sci_uart_extended_cfg_t {
    int clock, rx_edge_start, noise_cancel, rx_fifo_trigger;
    baud_setting_t* p_baud_setting;
    int flow_control;
    bsp_io_port_pin_t flow_control_pin;
    struct { int enable; bsp_io_port_pin_t de_control_pin; } rs485_setting;
};
struct uart_cfg_t {
    int channel, data_bits, parity, stop_bits;
    void (*p_callback)(uart_callback_args_t*);
    const void* p_context;
    const void* p_extend;
    IRQn_Type txi_irq, tei_irq, rxi_irq, eri_irq;
};
struct FakeSciRegisters {
    struct { uint8_t RIE, RE, TE; } SCR_b;
    struct { uint8_t RDRF; } SSR_b;
};
struct sci_uart_instance_ctrl_t { FakeSciRegisters* p_reg; const uart_cfg_t* cfg; };
extern FakeSciRegisters fakeSci;
extern sci_uart_instance_ctrl_t* fakeUartCtrl;
extern unsigned fakeWrites, fakeAborts;
extern uint8_t fakeWritten[64];
extern size_t fakeWrittenCount;
inline fsp_err_t R_SCI_UART_BaudCalculate(uint32_t baud, bool, uint32_t, baud_setting_t* setting) {
    setting->baud = baud; return FSP_SUCCESS;
}
inline fsp_err_t R_SCI_UART_Open(sci_uart_instance_ctrl_t* ctrl, const uart_cfg_t* cfg) {
    ctrl->p_reg = &fakeSci; ctrl->cfg = cfg; fakeUartCtrl = ctrl; return FSP_SUCCESS;
}
inline fsp_err_t R_SCI_UART_BaudSet(sci_uart_instance_ctrl_t*, const baud_setting_t*) { return FSP_SUCCESS; }
inline fsp_err_t R_SCI_UART_Write(sci_uart_instance_ctrl_t*, const uint8_t* data, uint32_t count) {
    ++fakeWrites; fakeWrittenCount = count;
    for (uint32_t i = 0; i < count; ++i) fakeWritten[i] = data[i];
    return FSP_SUCCESS;
}
inline fsp_err_t R_SCI_UART_Abort(sci_uart_instance_ctrl_t*, int) { ++fakeAborts; return FSP_SUCCESS; }
inline void fakeUartEvent(uart_event_t event, uint32_t data = 0) {
    uart_callback_args_t args{event, fakeUartCtrl->cfg->p_context, data};
    fakeUartCtrl->cfg->p_callback(&args);
}
