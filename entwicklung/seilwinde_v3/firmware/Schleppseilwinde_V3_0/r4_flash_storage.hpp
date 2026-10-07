#pragma once
#include <Arduino.h>
#include <r_flash_lp.h>
#include "src/winch/persistence.hpp"

// Exclusive owner of RA4M1 data flash. Do not mix EEPROM/RM_VEE calls.
// ByteStorage updates only stage RAM; journal.save() is not a physical commit.
class R4FlashStorage : public winch::ByteStorage {
public:
    enum class Status : uint8_t { Idle, Busy, Complete, Error };
    bool begin();
    size_t size() const override { return kStride + kRecordSize; }
    uint8_t read(size_t address) const override;
    bool update(size_t address, uint8_t value) override;
    bool startCommit(uint32_t nowMs);
    void service(uint32_t nowMs);
    Status status() const { return status_; }
    bool busy() const { return status_ == Status::Busy; }
    uint32_t error() const { return error_; }
private:
    static constexpr size_t kRecordSize = winch::SettingsJournal::RecordSize;
    static constexpr size_t kStride = 1024;
    static constexpr uint32_t kFlashBase = 0x40100000U;
    static constexpr uint32_t kStageTimeoutMs = 500;
    enum class Phase : uint8_t { Erase, Body, Commit };
    static void callback(flash_callback_args_t* args);
    static bool locate(size_t address, size_t& slot, size_t& offset);
    void fail(uint32_t reason);
    bool startWrite(size_t offset, size_t count, uint32_t nowMs);
    uint8_t records_[2][kRecordSize]{};
    bool dirty_[2]{false, false};
    bool opened_ = false;
    bool irqAllocated_ = false;
    uint8_t target_ = 0;
    Phase phase_ = Phase::Erase;
    Status status_ = Status::Idle;
    uint32_t stageStartedMs_ = 0;
    uint32_t error_ = 0;
    volatile bool eventPending_ = false;
    volatile flash_event_t event_ = FLASH_EVENT_ERR_FAILURE;
    flash_lp_instance_ctrl_t ctrl_{};
    flash_cfg_t cfg_{};
};
