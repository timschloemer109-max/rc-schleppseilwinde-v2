#include "r4_flash_storage.hpp"
#include <IRQManager.h>

#if !defined(ARDUINO_NANO_R4)
#error "R4 data flash adapter is qualified only for Nano R4 / Renesas core 1.6.0."
#endif
static_assert(BSP_FEATURE_FLASH_LP_DF_BLOCK_SIZE == 1024, "Journal erase isolation assumes 1024-byte blocks.");
static_assert(BSP_FEATURE_FLASH_LP_DF_WRITE_SIZE == 1, "Commit marker needs byte programming.");
extern "C" void fcu_frdyi_isr(void);

bool R4FlashStorage::locate(size_t address, size_t& slot, size_t& offset) {
    if (address < kRecordSize) { slot = 0; offset = address; return true; }
    if (address >= kStride && address - kStride < kRecordSize) {
        slot = 1; offset = address - kStride; return true;
    }
    return false;
}

bool R4FlashStorage::begin() {
    if (opened_) return true;
    // At boot there is no P/E operation. Afterwards use RAM while BGO is active:
    // the data-flash array cannot safely be read during erase/program.
    const auto* flash = reinterpret_cast<const volatile uint8_t*>(kFlashBase);
    for (size_t slot = 0; slot < 2; ++slot)
        for (size_t i = 0; i < kRecordSize; ++i) records_[slot][i] = flash[slot * kStride + i];
    cfg_.data_flash_bgo = true;
    cfg_.p_callback = callback;
    cfg_.p_context = this;
    cfg_.ipl = 14;  // Below PWM, Hall, UART and USB; persistence cannot preempt them.
    cfg_.err_ipl = BSP_IRQ_DISABLED;
    cfg_.err_irq = FSP_INVALID_VECTOR;
    if (!irqAllocated_) {
        GenericIrqCfg_t irq{};
        irq.irq = FSP_INVALID_VECTOR;
        irq.ipl = cfg_.ipl;
        irq.event = ELC_EVENT_FCU_FRDYI;
        if (!IRQManager::getInstance().addGenericInterrupt(irq, fcu_frdyi_isr)) {
            fail(static_cast<uint32_t>(FSP_ERR_IRQ_BSP_DISABLED));
            return false;
        }
        cfg_.irq = irq.irq;
        irqAllocated_ = true;
    }
    const auto result = R_FLASH_LP_Open(&ctrl_, &cfg_);
    if (result != FSP_SUCCESS) { fail(static_cast<uint32_t>(result)); return false; }
    opened_ = true;
    status_ = Status::Idle;
    return true;
}

uint8_t R4FlashStorage::read(size_t address) const {
    size_t slot = 0, offset = 0;
    if (!opened_ || !locate(address, slot, offset)) return 0xff;
    return records_[slot][offset];
}

bool R4FlashStorage::update(size_t address, uint8_t value) {
    size_t slot = 0, offset = 0;
    if (!opened_ || busy() || status_ == Status::Error || !locate(address, slot, offset)) return false;
    if (records_[slot][offset] != value) {
        records_[slot][offset] = value;
        dirty_[slot] = true;
    }
    return true;
}

bool R4FlashStorage::startCommit(uint32_t nowMs) {
    if (!opened_ || busy() || status_ == Status::Error) return false;
    if (!dirty_[0] && !dirty_[1]) { status_ = Status::Complete; return true; }
    // A journal transaction must touch only its inactive isolated erase block.
    if (dirty_[0] && dirty_[1]) { fail(static_cast<uint32_t>(FSP_ERR_INVALID_ARGUMENT)); return false; }
    target_ = dirty_[0] ? 0 : 1;
    phase_ = Phase::Erase;
    stageStartedMs_ = nowMs;
    eventPending_ = false;
    status_ = Status::Busy;
    const auto result = R_FLASH_LP_Erase(&ctrl_, kFlashBase + target_ * kStride, 1);
    if (result != FSP_SUCCESS) { fail(static_cast<uint32_t>(result)); return false; }
    return true;
}

void R4FlashStorage::fail(uint32_t reason) {
    error_ = reason;
    status_ = Status::Error;
    // Do not read flash or synchronously reset/wait here. A late BGO interrupt
    // may still occur. Firmware must inhibit power/restart until reboot.
}

bool R4FlashStorage::startWrite(size_t offset, size_t count, uint32_t nowMs) {
    stageStartedMs_ = nowMs;
    const auto result = R_FLASH_LP_Write(&ctrl_,
        reinterpret_cast<uint32_t>(&records_[target_][offset]),
        kFlashBase + target_ * kStride + static_cast<uint32_t>(offset), static_cast<uint32_t>(count));
    if (result != FSP_SUCCESS) { fail(static_cast<uint32_t>(result)); return false; }
    return true;
}

void R4FlashStorage::service(uint32_t nowMs) {
    if (!busy()) return;
    // One state transition per call; no polling loop waiting for flash.
    const uint32_t previousMask = __get_PRIMASK();
    __disable_irq();
    const bool pending = eventPending_;
    const auto event = event_;
    eventPending_ = false;
    __set_PRIMASK(previousMask);
    if (pending) {
        if (phase_ == Phase::Erase && event == FLASH_EVENT_ERASE_COMPLETE) {
            phase_ = Phase::Body;
            startWrite(0, kRecordSize - 1, nowMs);
        } else if (phase_ == Phase::Body && event == FLASH_EVENT_WRITE_COMPLETE) {
            phase_ = Phase::Commit;
            startWrite(kRecordSize - 1, 1, nowMs);
        } else if (phase_ == Phase::Commit && event == FLASH_EVENT_WRITE_COMPLETE) {
            // FSP callback runs after exiting data-flash P/E mode. Read back the
            // complete record before reporting actual persistence to the menu.
            const auto* flash = reinterpret_cast<const volatile uint8_t*>(kFlashBase + target_ * kStride);
            for (size_t i = 0; i < kRecordSize; ++i) {
                if (flash[i] != records_[target_][i]) {
                    fail(static_cast<uint32_t>(FSP_ERR_WRITE_FAILED));
                    return;
                }
            }
            dirty_[target_] = false;
            status_ = Status::Complete;
        } else {
            fail(0x10000U + static_cast<uint32_t>(event));
        }
    } else if (static_cast<uint32_t>(nowMs - stageStartedMs_) >= kStageTimeoutMs) {
        fail(static_cast<uint32_t>(FSP_ERR_TIMEOUT));
    }
}

void R4FlashStorage::callback(flash_callback_args_t* args) {
    if (args == nullptr || args->p_context == nullptr) return;
    auto* instance = static_cast<R4FlashStorage*>(const_cast<void*>(args->p_context));
    instance->event_ = args->event;
    __DMB();
    instance->eventPending_ = true;
}
