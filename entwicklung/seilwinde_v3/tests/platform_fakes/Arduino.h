#pragma once
#include <stddef.h>
#include <stdint.h>
// Deterministic API/register spy, not an RA4M1 timing or electrical emulator.
using bsp_io_port_pin_t = uint16_t;
using IRQn_Type = int;
using fsp_err_t = int;
constexpr fsp_err_t FSP_SUCCESS = 0;
constexpr IRQn_Type FSP_INVALID_VECTOR = -1;
constexpr bsp_io_port_pin_t BSP_IO_PORT_03_PIN_01 = 0x301;
constexpr bsp_io_port_pin_t BSP_IO_PORT_03_PIN_02 = 0x302;
constexpr uint32_t IOPORT_CFG_PERIPHERAL_PIN = 1;
constexpr uint32_t IOPORT_PERIPHERAL_SCI0_2_4_6_8 = 2;
constexpr uint32_t IOPORT_CFG_PORT_DIRECTION_INPUT = 4;
constexpr uint32_t IOPORT_CFG_PULLUP_ENABLE = 8;
constexpr uint32_t IOPORT_CFG_PORT_DIRECTION_OUTPUT = 16;
constexpr uint32_t IOPORT_CFG_PORT_OUTPUT_LOW = 32;
constexpr unsigned PINS_COUNT = 29;
struct PinMuxCfg_t { bsp_io_port_pin_t pin; };
struct FakePort { uint32_t PCNTR3; };
extern PinMuxCfg_t g_pin_cfg[PINS_COUNT];
extern FakePort fakePorts[6];
extern int g_ioport_ctrl;
extern uint32_t fakeNowUs, fakePrimask, fakeTxPinConfig, fakeRxPinConfig;
extern bool fakeRxIrqPending;
#define R_PORT0 (&fakePorts[0])
#define R_PORT1 (&fakePorts[1])
inline uint32_t micros() { return fakeNowUs; }
inline uint32_t __get_PRIMASK() { return fakePrimask; }
inline void __disable_irq() { fakePrimask = 1; }
inline void __set_PRIMASK(uint32_t mask) { fakePrimask = mask; }
inline void __DMB() {}
inline uint32_t NVIC_GetPendingIRQ(IRQn_Type) { return fakeRxIrqPending ? 1U : 0U; }
inline fsp_err_t R_IOPORT_PinCfg(int*, bsp_io_port_pin_t pin, uint32_t cfg) {
    if (pin == BSP_IO_PORT_03_PIN_02) fakeTxPinConfig = cfg;
    if (pin == BSP_IO_PORT_03_PIN_01) fakeRxPinConfig = cfg;
    return FSP_SUCCESS;
}
