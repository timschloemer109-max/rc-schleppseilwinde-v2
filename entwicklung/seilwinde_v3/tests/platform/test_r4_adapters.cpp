#include "r4_exbus_uart.hpp"
#include "r4_esc_pwm.hpp"
#include <cstdio>
#include <cstdlib>
PinMuxCfg_t g_pin_cfg[PINS_COUNT]{};
FakePort fakePorts[6]{};
int g_ioport_ctrl = 0;
uint32_t fakeNowUs = 0, fakePrimask = 0, fakeTxPinConfig = 0, fakeRxPinConfig = 0;
bool fakeRxIrqPending = false;
FakeSciRegisters fakeSci{};
sci_uart_instance_ctrl_t* fakeUartCtrl = nullptr;
unsigned fakeWrites = 0, fakeAborts = 0;
uint8_t fakeWritten[64]{};
size_t fakeWrittenCount = 0;
void (*fakeTimerCallback)(timer_callback_args_t*) = nullptr;
const void* fakeTimerContext = nullptr;
uint32_t fakeTimerPeriod = 0;
unsigned fakeTimerStarts = 0, fakeTimerStops = 0, fakeTimerResets = 0;
bool fakeTimerRunning = false;
static unsigned checks = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
    std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); std::exit(1); } } while (false)
static void uartChecks() {
    R4ExBusUart bus;
    CHECK(!bus.begin(115200)); CHECK(bus.begin(125000)); CHECK(bus.baud() == 125000);
    CHECK(fakeSci.SCR_b.TE == 0);
    CHECK(fakeSci.SCR_b.RE == 1 && fakeSci.SCR_b.RIE == 1);
    CHECK(fakeTxPinConfig == (IOPORT_CFG_PORT_DIRECTION_INPUT | IOPORT_CFG_PULLUP_ENABLE));
    fakeNowUs = 123; fakeUartEvent(UART_EVENT_RX_CHAR, 0x42);
    R4ExBusUart::Byte byte{};
    const uint8_t reply[] = {0x3b, 1, 2};
    CHECK(!bus.trySend(reply, sizeof(reply)));
    fakePrimask = 1; CHECK(bus.pop(byte)); CHECK(fakePrimask == 1); fakePrimask = 0;
    CHECK(byte.value == 0x42 && byte.receivedUs == 123); CHECK(!bus.pop(byte));
    fakeSci.SSR_b.RDRF = 1; CHECK(!bus.trySend(reply, sizeof(reply)));
    fakeSci.SSR_b.RDRF = 0; fakeRxIrqPending = true; CHECK(!bus.trySend(reply, sizeof(reply)));
    fakeRxIrqPending = false;
    CHECK(!bus.trySend(nullptr, 3)); CHECK(!bus.trySend(reply, 0)); CHECK(!bus.trySend(reply, 65));
    CHECK(bus.trySend(reply, sizeof(reply)));
    CHECK(bus.transmitting() && fakeSci.SCR_b.TE == 1 && fakeSci.SCR_b.RE == 0);
    CHECK(fakeWrittenCount == sizeof(reply) && fakeWritten[0] == 0x3b);
    CHECK(!bus.trySend(reply, sizeof(reply))); CHECK(!bus.setBaud(250000));
    fakeUartEvent(UART_EVENT_RX_CHAR, 0x55); CHECK(bus.available() == 0);
    fakeUartEvent(UART_EVENT_TX_DATA_EMPTY); CHECK(bus.transmitting() && fakeSci.SCR_b.TE == 1);
    fakeUartEvent(UART_EVENT_TX_COMPLETE);
    CHECK(!bus.transmitting() && fakeSci.SCR_b.TE == 0 && fakeSci.SCR_b.RE == 1);
    CHECK(fakeTxPinConfig == (IOPORT_CFG_PORT_DIRECTION_INPUT | IOPORT_CFG_PULLUP_ENABLE));
    const auto previousErrors = bus.errors();
    CHECK(bus.setBaud(250000) && bus.baud() == 250000); CHECK(bus.errors() == previousErrors + 1U);
    for (unsigned i = 0; i < 127; ++i) fakeUartEvent(UART_EVENT_RX_CHAR, i);
    CHECK(bus.available() == 127); fakeUartEvent(UART_EVENT_RX_CHAR, 127);
    CHECK(bus.overflows() == 1 && bus.available() == 0);
    fakeUartEvent(UART_EVENT_RX_CHAR, 1); fakeUartEvent(UART_EVENT_ERR_FRAMING); CHECK(bus.available() == 0);
    fakeNowUs = 0xfffffff0U; CHECK(bus.trySend(reply, sizeof(reply)));
    bus.service(fakeNowUs + 319U); CHECK(bus.transmitting());
    bus.service(fakeNowUs + 320U);
    CHECK(!bus.transmitting() && fakeAborts == 1 && fakeSci.SCR_b.TE == 0); CHECK(fakePrimask == 0);
}
static void pwmChecks() {
    g_pin_cfg[4].pin = 0x103;
    R4EscPwm esc; CHECK(esc.begin(4) && esc.healthy());
    CHECK(fakePorts[1].PCNTR3 == (1U << 19U));
    fakeTimerEvent(); CHECK(fakePorts[1].PCNTR3 == 8U && fakeTimerPeriod == 3000U);
    const unsigned initialResets = fakeTimerResets;
    for (unsigned i = 0; i < 100; ++i) esc.stopNow();
    CHECK(fakePorts[1].PCNTR3 == 8U && fakeTimerResets == initialResets);
    fakeTimerEvent(); CHECK(fakePorts[1].PCNTR3 == (1U << 19U) && fakeTimerPeriod == 57000U);
    esc.writeMicroseconds(1800); fakeTimerEvent();
    CHECK(fakePorts[1].PCNTR3 == 8U && fakeTimerPeriod == 5400U);
    esc.stopNow(); CHECK(fakePorts[1].PCNTR3 == (1U << 19U));
    CHECK(fakeTimerPeriod == 60000U && fakeTimerResets == initialResets + 1U);
    fakeTimerEvent(); CHECK(fakeTimerPeriod == 3000U); fakeTimerEvent();
    esc.writeMicroseconds(999); fakeTimerEvent(); CHECK(fakeTimerPeriod == 3000U); fakeTimerEvent();
    esc.writeMicroseconds(2001); fakeTimerEvent(); CHECK(fakeTimerPeriod == 3000U);
    esc.disable(); CHECK(!esc.healthy() && !fakeTimerRunning && fakePorts[1].PCNTR3 == (1U << 19U));
    esc.stopNow(); CHECK(!fakeTimerRunning && !esc.healthy());
    fakeTimerEvent(); CHECK(fakePorts[1].PCNTR3 == (1U << 19U));
}
int main() { uartChecks(); pwmChecks();
    std::printf("PASS: %u actual R4 UART/PWM adapter checks using deterministic API spies\n", checks); return 0; }
