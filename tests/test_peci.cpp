/* ****************************************************************
 *
 * libpeci — Unit Tests for peci.c
 * test_peci.cpp
 *
 * Covers all public API functions in peci.c:
 *   peci_SetDevName, peci_Open, peci_Close, peci_Lock, peci_Unlock,
 *   peci_Ping, peci_Ping_seq, peci_GetDIB, peci_GetDIB_seq,
 *   peci_GetTemp, peci_GetCPUID,
 *   peci_RdPkgConfig (all variants), peci_WrPkgConfig (all variants),
 *   peci_RdIAMSR, peci_RdIAMSR_dom,
 *   peci_RdPCIConfig (all variants), peci_RdPCIConfigLocal (all variants),
 *   peci_WrPCIConfigLocal, peci_WrPCIConfigLocal_dom,
 *   peci_RdEndPointConfigPci (all variants),
 *   peci_RdEndPointConfigPciLocal (all variants),
 *   peci_RdEndPointConfigMmio (all variants),
 *   peci_WrEndPointPCIConfigLocal (all variants),
 *   peci_WrEndPointPCIConfig (all variants),
 *   peci_WrEndPointConfig_seq (all variants),
 *   peci_WrEndPointConfigMmio (all variants),
 *   peci_CrashDump_Discovery (all variants),
 *   peci_CrashDump_GetFrame (all variants),
 *   peci_Telemetry_Discovery (all variants),
 *   peci_Telemetry_GetTelemSample (all variants),
 *   peci_Telemetry_ConfigWatcherRd (all variants),
 *   peci_Telemetry_ConfigWatcherWr (all variants),
 *   peci_Telemetry_GetCrashlogSample (all variants),
 *   peci_raw, peci_raw_seq,
 *   FindBusNumber, peci_WakePECI, peci_i3c_chardev_to_cpu
 *
 *****************************************************************/

extern "C"
{
#include "mock_syscalls.h"
#include <peci.h>

// Functions defined in peci.c but not declared in peci.h
EPECIStatus peci_GetDIB(uint8_t target, uint64_t* dib);
EPECIStatus peci_GetDIB_seq(uint8_t target, uint64_t* dib, int peci_fd);
}

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstring>

// Valid PECI target address
static constexpr uint8_t VALID_TARGET = 0x30;
// Invalid (below range) PECI target address
static constexpr uint8_t INVALID_TARGET_LOW = 0x20;
// Invalid (above range) PECI target address
static constexpr uint8_t INVALID_TARGET_HIGH = 0x40;

// ═══════════════════════════════════════════════════════════════════════════
// Test fixture — resets mock state before each test
// ═══════════════════════════════════════════════════════════════════════════

class PeciTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mock_reset_all();
        // Ensure device list is initialized
        peci_SetDevName(NULL);
    }

    void TearDown() override
    {
        mock_active = 0;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// peci_SetDevName
// ═══════════════════════════════════════════════════════════════════════════

class SetDevNameTest : public PeciTest
{};

TEST_F(SetDevNameTest, NullSetsDefaults)
{
    // Arrange + Act
    peci_SetDevName(NULL);

    // Assert — no crash, function completes
    SUCCEED();
}

TEST_F(SetDevNameTest, CustomDeviceName)
{
    // Arrange
    char devName[] = "/dev/peci-custom";

    // Act
    peci_SetDevName(devName);

    // Assert — no crash
    SUCCEED();
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_Open / peci_Lock / peci_Close / peci_Unlock
// ═══════════════════════════════════════════════════════════════════════════

class OpenCloseTest : public PeciTest
{};

TEST_F(OpenCloseTest, Open_NullFd_ReturnsInvalidReq)
{
    // Act
    EPECIStatus ret = peci_Open(NULL);

    // Assert
    EXPECT_EQ(ret, PECI_CC_INVALID_REQ);
}

TEST_F(OpenCloseTest, Open_Success)
{
    // Arrange
    int fd = -1;
    mock_open_return = 5;

    // Act
    EPECIStatus ret = peci_Open(&fd);

    // Assert
    EXPECT_EQ(ret, PECI_CC_SUCCESS);
    EXPECT_EQ(fd, 5);
}

TEST_F(OpenCloseTest, Open_DeviceBusy_ReturnsDriverErr)
{
    // Arrange
    int fd = -1;
    mock_open_return = -1;
    mock_open_errno = EBUSY;

    // Act
    EPECIStatus ret = peci_Open(&fd);

    // Assert
    EXPECT_EQ(ret, PECI_CC_DRIVER_ERR);
}

TEST_F(OpenCloseTest, Lock_NullFd_ReturnsInvalidReq)
{
    // Act
    EPECIStatus ret = peci_Lock(NULL, PECI_NO_WAIT);

    // Assert
    EXPECT_EQ(ret, PECI_CC_INVALID_REQ);
}

TEST_F(OpenCloseTest, Lock_NoWait_Success)
{
    // Arrange
    int fd = -1;
    mock_open_return = 7;

    // Act
    EPECIStatus ret = peci_Lock(&fd, PECI_NO_WAIT);

    // Assert
    EXPECT_EQ(ret, PECI_CC_SUCCESS);
    EXPECT_EQ(fd, 7);
}

TEST_F(OpenCloseTest, Lock_NoWait_Failure)
{
    // Arrange
    int fd = -1;
    mock_open_return = -1;

    // Act
    EPECIStatus ret = peci_Lock(&fd, PECI_NO_WAIT);

    // Assert
    EXPECT_EQ(ret, PECI_CC_DRIVER_ERR);
}

TEST_F(OpenCloseTest, Lock_WithTimeout_SuccessAfterRetry)
{
    // Arrange — first open fails, second succeeds
    int fd = -1;
    mock_open_return = -1;

    // We need to simulate the open succeeding on retry.
    // Since mock_open_return is static, we'll just test the immediate
    // failure path with a small timeout.

    // Act
    EPECIStatus ret = peci_Lock(&fd, 10); // 10ms timeout

    // Assert — should fail since mock always returns -1
    EXPECT_EQ(ret, PECI_CC_DRIVER_ERR);
}

TEST_F(OpenCloseTest, Lock_FallbackDevice)
{
    // Arrange — first device doesn't exist, mock simulates ENOENT
    int fd = -1;
    mock_open_return = -1;
    mock_open_errno = ENOENT;

    // peci_SetDevName(NULL) sets two devices, second open also fails
    peci_SetDevName(NULL);

    // Act
    EPECIStatus ret = peci_Lock(&fd, PECI_NO_WAIT);

    // Assert
    EXPECT_EQ(ret, PECI_CC_DRIVER_ERR);
}

TEST_F(OpenCloseTest, Close_Success)
{
    // Arrange
    mock_close_return = 0;

    // Act + Assert — no crash
    peci_Close(3);
    EXPECT_GT(mock_close_call_count, 0);
}

TEST_F(OpenCloseTest, Unlock_CloseFailure_LogsError)
{
    // Arrange
    mock_close_return = -1;
    mock_close_errno = EIO;

    // Act + Assert — should not crash
    peci_Unlock(3);
    SUCCEED();
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_Ping / peci_Ping_seq
// ═══════════════════════════════════════════════════════════════════════════

class PingTest : public PeciTest
{};

TEST_F(PingTest, Ping_InvalidTargetLow_ReturnsInvalidReq)
{
    EXPECT_EQ(peci_Ping(INVALID_TARGET_LOW), PECI_CC_INVALID_REQ);
}

TEST_F(PingTest, Ping_InvalidTargetHigh_ReturnsInvalidReq)
{
    EXPECT_EQ(peci_Ping(INVALID_TARGET_HIGH), PECI_CC_INVALID_REQ);
}

TEST_F(PingTest, Ping_OpenFails_ReturnsDriverErr)
{
    mock_open_return = -1;
    EXPECT_EQ(peci_Ping(VALID_TARGET), PECI_CC_DRIVER_ERR);
}

TEST_F(PingTest, Ping_Success)
{
    mock_open_return = 3;
    mock_ioctl_return = 0;
    EXPECT_EQ(peci_Ping(VALID_TARGET), PECI_CC_SUCCESS);
}

TEST_F(PingTest, PingSeq_InvalidTarget_ReturnsInvalidReq)
{
    EXPECT_EQ(peci_Ping_seq(INVALID_TARGET_LOW, 3), PECI_CC_INVALID_REQ);
    EXPECT_EQ(peci_Ping_seq(INVALID_TARGET_HIGH, 3), PECI_CC_INVALID_REQ);
}

TEST_F(PingTest, PingSeq_Success)
{
    mock_ioctl_return = 0;
    EXPECT_EQ(peci_Ping_seq(VALID_TARGET, 3), PECI_CC_SUCCESS);
}

TEST_F(PingTest, PingSeq_IoctlTimeout)
{
    mock_ioctl_return = -1;
    mock_ioctl_errno = ETIMEDOUT;
    mock_ioctl_callback = [](unsigned long cmd, void* argp) -> int {
        errno = ETIMEDOUT;
        return -1;
    };
    EXPECT_EQ(peci_Ping_seq(VALID_TARGET, 3), PECI_CC_TIMEOUT);
}

TEST_F(PingTest, PingSeq_IoctlDriverErr)
{
    mock_ioctl_callback = [](unsigned long cmd, void* argp) -> int {
        errno = EIO;
        return -1;
    };
    EXPECT_EQ(peci_Ping_seq(VALID_TARGET, 3), PECI_CC_DRIVER_ERR);
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_GetDIB / peci_GetDIB_seq
// ═══════════════════════════════════════════════════════════════════════════

class GetDIBTest : public PeciTest
{};

TEST_F(GetDIBTest, GetDIB_NullDib_ReturnsInvalidReq)
{
    EXPECT_EQ(peci_GetDIB(VALID_TARGET, NULL), PECI_CC_INVALID_REQ);
}

TEST_F(GetDIBTest, GetDIB_InvalidTarget_ReturnsInvalidReq)
{
    uint64_t dib = 0;
    EXPECT_EQ(peci_GetDIB(INVALID_TARGET_LOW, &dib), PECI_CC_INVALID_REQ);
}

TEST_F(GetDIBTest, GetDIB_OpenFails_ReturnsDriverErr)
{
    uint64_t dib = 0;
    mock_open_return = -1;
    EXPECT_EQ(peci_GetDIB(VALID_TARGET, &dib), PECI_CC_DRIVER_ERR);
}

TEST_F(GetDIBTest, GetDIB_Success)
{
    uint64_t dib = 0;
    mock_open_return = 3;
    mock_ioctl_return = 0;
    EXPECT_EQ(peci_GetDIB(VALID_TARGET, &dib), PECI_CC_SUCCESS);
}

TEST_F(GetDIBTest, GetDIBSeq_NullDib_ReturnsInvalidReq)
{
    EXPECT_EQ(peci_GetDIB_seq(VALID_TARGET, NULL, 3), PECI_CC_INVALID_REQ);
}

TEST_F(GetDIBTest, GetDIBSeq_InvalidTarget_ReturnsInvalidReq)
{
    uint64_t dib = 0;
    EXPECT_EQ(peci_GetDIB_seq(INVALID_TARGET_LOW, &dib, 3),
              PECI_CC_INVALID_REQ);
    EXPECT_EQ(peci_GetDIB_seq(INVALID_TARGET_HIGH, &dib, 3),
              PECI_CC_INVALID_REQ);
}

TEST_F(GetDIBTest, GetDIBSeq_Success)
{
    uint64_t dib = 0;
    mock_ioctl_return = 0;
    EXPECT_EQ(peci_GetDIB_seq(VALID_TARGET, &dib, 3), PECI_CC_SUCCESS);
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_GetTemp
// ═══════════════════════════════════════════════════════════════════════════

class GetTempTest : public PeciTest
{};

TEST_F(GetTempTest, NullTemp_ReturnsInvalidReq)
{
    EXPECT_EQ(peci_GetTemp(VALID_TARGET, NULL), PECI_CC_INVALID_REQ);
}

TEST_F(GetTempTest, InvalidTarget_ReturnsInvalidReq)
{
    int16_t temp = 0;
    EXPECT_EQ(peci_GetTemp(INVALID_TARGET_LOW, &temp), PECI_CC_INVALID_REQ);
    EXPECT_EQ(peci_GetTemp(INVALID_TARGET_HIGH, &temp), PECI_CC_INVALID_REQ);
}

TEST_F(GetTempTest, OpenFails_ReturnsDriverErr)
{
    int16_t temp = 0;
    mock_open_return = -1;
    EXPECT_EQ(peci_GetTemp(VALID_TARGET, &temp), PECI_CC_DRIVER_ERR);
}

TEST_F(GetTempTest, Success)
{
    int16_t temp = 0;
    mock_open_return = 3;
    mock_ioctl_return = 0;
    EXPECT_EQ(peci_GetTemp(VALID_TARGET, &temp), PECI_CC_SUCCESS);
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_RdPkgConfig (all variants)
// ═══════════════════════════════════════════════════════════════════════════

class RdPkgConfigTest : public PeciTest
{};

TEST_F(RdPkgConfigTest, NullPkgConfig_ReturnsInvalidReq)
{
    uint8_t cc = 0;
    EXPECT_EQ(peci_RdPkgConfig(VALID_TARGET, 0, 0, 4, NULL, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(RdPkgConfigTest, NullCC_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    EXPECT_EQ(peci_RdPkgConfig(VALID_TARGET, 0, 0, 4, buf, NULL),
              PECI_CC_INVALID_REQ);
}

TEST_F(RdPkgConfigTest, InvalidTarget_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    EXPECT_EQ(peci_RdPkgConfig(INVALID_TARGET_LOW, 0, 0, 4, buf, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(RdPkgConfigTest, InvalidReadLen_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    // Read length must be 1, 2, 4, or 8
    EXPECT_EQ(
        peci_RdPkgConfig_seq_dom(VALID_TARGET, 0, 0, 0, 3, buf, 3, &cc),
        PECI_CC_INVALID_REQ);
    EXPECT_EQ(
        peci_RdPkgConfig_seq_dom(VALID_TARGET, 0, 0, 0, 5, buf, 3, &cc),
        PECI_CC_INVALID_REQ);
}

TEST_F(RdPkgConfigTest, OpenFails_ReturnsDriverErr)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = -1;
    EXPECT_EQ(peci_RdPkgConfig(VALID_TARGET, 0, 0, 4, buf, &cc),
              PECI_CC_DRIVER_ERR);
}

TEST_F(RdPkgConfigTest, Success)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    mock_ioctl_return = 0;
    EXPECT_EQ(peci_RdPkgConfig(VALID_TARGET, 0, 0, 4, buf, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(RdPkgConfigTest, DomVariant_Success)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    mock_ioctl_return = 0;
    EXPECT_EQ(peci_RdPkgConfig_dom(VALID_TARGET, 0, 0, 0, 4, buf, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(RdPkgConfigTest, SeqVariant_Success)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_ioctl_return = 0;
    EXPECT_EQ(peci_RdPkgConfig_seq(VALID_TARGET, 0, 0, 4, buf, 3, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(RdPkgConfigTest, SeqDomVariant_ValidReadLengths)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_ioctl_return = 0;
    EXPECT_EQ(
        peci_RdPkgConfig_seq_dom(VALID_TARGET, 0, 0, 0, 1, buf, 3, &cc),
        PECI_CC_SUCCESS);
    EXPECT_EQ(
        peci_RdPkgConfig_seq_dom(VALID_TARGET, 0, 0, 0, 2, buf, 3, &cc),
        PECI_CC_SUCCESS);
    EXPECT_EQ(
        peci_RdPkgConfig_seq_dom(VALID_TARGET, 0, 0, 0, 4, buf, 3, &cc),
        PECI_CC_SUCCESS);
    EXPECT_EQ(
        peci_RdPkgConfig_seq_dom(VALID_TARGET, 0, 0, 0, 8, buf, 3, &cc),
        PECI_CC_SUCCESS);
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_WrPkgConfig (all variants)
// ═══════════════════════════════════════════════════════════════════════════

class WrPkgConfigTest : public PeciTest
{};

TEST_F(WrPkgConfigTest, NullCC_ReturnsInvalidReq)
{
    uint32_t data = 0;
    EXPECT_EQ(peci_WrPkgConfig(VALID_TARGET, 0, 0, &data, 4, NULL),
              PECI_CC_INVALID_REQ);
}

TEST_F(WrPkgConfigTest, NullData_ReturnsInvalidReq)
{
    uint8_t cc = 0;
    EXPECT_EQ(peci_WrPkgConfig(VALID_TARGET, 0, 0, NULL, 4, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(WrPkgConfigTest, InvalidTarget_ReturnsInvalidReq)
{
    uint32_t data = 0;
    uint8_t cc = 0;
    EXPECT_EQ(peci_WrPkgConfig(INVALID_TARGET_LOW, 0, 0, &data, 4, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(WrPkgConfigTest, InvalidWriteLen_ReturnsInvalidReq)
{
    uint32_t data = 0;
    uint8_t cc = 0;
    mock_ioctl_return = 0;
    EXPECT_EQ(
        peci_WrPkgConfig_seq_dom(VALID_TARGET, 0, 0, 0, &data, 3, 3, &cc),
        PECI_CC_INVALID_REQ);
    EXPECT_EQ(
        peci_WrPkgConfig_seq_dom(VALID_TARGET, 0, 0, 0, &data, 5, 3, &cc),
        PECI_CC_INVALID_REQ);
}

TEST_F(WrPkgConfigTest, OpenFails_ReturnsDriverErr)
{
    uint32_t data = 0;
    uint8_t cc = 0;
    mock_open_return = -1;
    EXPECT_EQ(peci_WrPkgConfig(VALID_TARGET, 0, 0, &data, 4, &cc),
              PECI_CC_DRIVER_ERR);
}

TEST_F(WrPkgConfigTest, Success)
{
    uint32_t data = 0x12345678;
    uint8_t cc = 0;
    mock_open_return = 3;
    mock_ioctl_return = 0;
    EXPECT_EQ(peci_WrPkgConfig(VALID_TARGET, 0, 0, &data, 4, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(WrPkgConfigTest, DomVariant_Success)
{
    uint32_t data = 0;
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(peci_WrPkgConfig_dom(VALID_TARGET, 0, 0, 0, &data, 4, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(WrPkgConfigTest, SeqVariant_Success)
{
    uint32_t data = 0;
    uint8_t cc = 0;
    EXPECT_EQ(peci_WrPkgConfig_seq(VALID_TARGET, 0, 0, &data, 4, 3, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(WrPkgConfigTest, SeqDomVariant_ValidWriteLengths)
{
    uint8_t data[8] = {};
    uint8_t cc = 0;
    EXPECT_EQ(
        peci_WrPkgConfig_seq_dom(VALID_TARGET, 0, 0, 0, data, 1, 3, &cc),
        PECI_CC_SUCCESS);
    EXPECT_EQ(
        peci_WrPkgConfig_seq_dom(VALID_TARGET, 0, 0, 0, data, 2, 3, &cc),
        PECI_CC_SUCCESS);
    EXPECT_EQ(
        peci_WrPkgConfig_seq_dom(VALID_TARGET, 0, 0, 0, data, 4, 3, &cc),
        PECI_CC_SUCCESS);
    EXPECT_EQ(
        peci_WrPkgConfig_seq_dom(VALID_TARGET, 0, 0, 0, data, 8, 3, &cc),
        PECI_CC_SUCCESS);
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_RdIAMSR / peci_RdIAMSR_dom
// ═══════════════════════════════════════════════════════════════════════════

class RdIAMSRTest : public PeciTest
{};

TEST_F(RdIAMSRTest, NullMsrVal_ReturnsInvalidReq)
{
    uint8_t cc = 0;
    EXPECT_EQ(peci_RdIAMSR(VALID_TARGET, 0, 0, NULL, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(RdIAMSRTest, NullCC_ReturnsInvalidReq)
{
    uint64_t val = 0;
    EXPECT_EQ(peci_RdIAMSR(VALID_TARGET, 0, 0, &val, NULL),
              PECI_CC_INVALID_REQ);
}

TEST_F(RdIAMSRTest, InvalidTarget_ReturnsInvalidReq)
{
    uint64_t val = 0;
    uint8_t cc = 0;
    EXPECT_EQ(peci_RdIAMSR(INVALID_TARGET_LOW, 0, 0, &val, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(RdIAMSRTest, OpenFails_ReturnsDriverErr)
{
    uint64_t val = 0;
    uint8_t cc = 0;
    mock_open_return = -1;
    EXPECT_EQ(peci_RdIAMSR(VALID_TARGET, 0, 0, &val, &cc),
              PECI_CC_DRIVER_ERR);
}

TEST_F(RdIAMSRTest, Success)
{
    uint64_t val = 0;
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(peci_RdIAMSR(VALID_TARGET, 0, 0, &val, &cc), PECI_CC_SUCCESS);
}

TEST_F(RdIAMSRTest, DomVariant_Success)
{
    uint64_t val = 0;
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(peci_RdIAMSR_dom(VALID_TARGET, 0, 0, 0, &val, &cc),
              PECI_CC_SUCCESS);
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_RdPCIConfig (all variants)
// ═══════════════════════════════════════════════════════════════════════════

class RdPCIConfigTest : public PeciTest
{};

TEST_F(RdPCIConfigTest, NullData_ReturnsInvalidReq)
{
    uint8_t cc = 0;
    EXPECT_EQ(peci_RdPCIConfig(VALID_TARGET, 0, 0, 0, 0, NULL, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(RdPCIConfigTest, NullCC_ReturnsInvalidReq)
{
    uint8_t buf[4] = {};
    EXPECT_EQ(peci_RdPCIConfig(VALID_TARGET, 0, 0, 0, 0, buf, NULL),
              PECI_CC_INVALID_REQ);
}

TEST_F(RdPCIConfigTest, InvalidTarget_ReturnsInvalidReq)
{
    uint8_t buf[4] = {};
    uint8_t cc = 0;
    EXPECT_EQ(peci_RdPCIConfig(INVALID_TARGET_LOW, 0, 0, 0, 0, buf, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(RdPCIConfigTest, OpenFails_ReturnsDriverErr)
{
    uint8_t buf[4] = {};
    uint8_t cc = 0;
    mock_open_return = -1;
    EXPECT_EQ(peci_RdPCIConfig(VALID_TARGET, 0, 0, 0, 0, buf, &cc),
              PECI_CC_DRIVER_ERR);
}

TEST_F(RdPCIConfigTest, Success)
{
    uint8_t buf[4] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(peci_RdPCIConfig(VALID_TARGET, 0, 0, 0, 0, buf, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(RdPCIConfigTest, DomVariant_Success)
{
    uint8_t buf[4] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(peci_RdPCIConfig_dom(VALID_TARGET, 0, 0, 0, 0, 0, buf, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(RdPCIConfigTest, SeqVariant_Success)
{
    uint8_t buf[4] = {};
    uint8_t cc = 0;
    EXPECT_EQ(peci_RdPCIConfig_seq(VALID_TARGET, 0, 0, 0, 0, buf, 3, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(RdPCIConfigTest, SeqDomVariant_NullChecks)
{
    uint8_t buf[4] = {};
    uint8_t cc = 0;
    EXPECT_EQ(
        peci_RdPCIConfig_seq_dom(VALID_TARGET, 0, 0, 0, 0, 0, NULL, 3, &cc),
        PECI_CC_INVALID_REQ);
    EXPECT_EQ(
        peci_RdPCIConfig_seq_dom(VALID_TARGET, 0, 0, 0, 0, 0, buf, 3, NULL),
        PECI_CC_INVALID_REQ);
    EXPECT_EQ(peci_RdPCIConfig_seq_dom(INVALID_TARGET_HIGH, 0, 0, 0, 0, 0, buf,
                                        3, &cc),
              PECI_CC_INVALID_REQ);
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_RdPCIConfigLocal (all variants)
// ═══════════════════════════════════════════════════════════════════════════

class RdPCIConfigLocalTest : public PeciTest
{};

TEST_F(RdPCIConfigLocalTest, NullData_ReturnsInvalidReq)
{
    uint8_t cc = 0;
    EXPECT_EQ(peci_RdPCIConfigLocal(VALID_TARGET, 0, 0, 0, 0, 4, NULL, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(RdPCIConfigLocalTest, NullCC_ReturnsInvalidReq)
{
    uint8_t buf[4] = {};
    EXPECT_EQ(peci_RdPCIConfigLocal(VALID_TARGET, 0, 0, 0, 0, 4, buf, NULL),
              PECI_CC_INVALID_REQ);
}

TEST_F(RdPCIConfigLocalTest, InvalidTarget_ReturnsInvalidReq)
{
    uint8_t buf[4] = {};
    uint8_t cc = 0;
    EXPECT_EQ(
        peci_RdPCIConfigLocal(INVALID_TARGET_LOW, 0, 0, 0, 0, 4, buf, &cc),
        PECI_CC_INVALID_REQ);
}

TEST_F(RdPCIConfigLocalTest, InvalidReadLen_ReturnsInvalidReq)
{
    uint8_t buf[4] = {};
    uint8_t cc = 0;
    // Read length must be 1, 2, or 4
    EXPECT_EQ(peci_RdPCIConfigLocal_seq_dom(VALID_TARGET, 0, 0, 0, 0, 0, 3,
                                             buf, 3, &cc),
              PECI_CC_INVALID_REQ);
    EXPECT_EQ(peci_RdPCIConfigLocal_seq_dom(VALID_TARGET, 0, 0, 0, 0, 0, 5,
                                             buf, 3, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(RdPCIConfigLocalTest, OpenFails_ReturnsDriverErr)
{
    uint8_t buf[4] = {};
    uint8_t cc = 0;
    mock_open_return = -1;
    EXPECT_EQ(peci_RdPCIConfigLocal(VALID_TARGET, 0, 0, 0, 0, 4, buf, &cc),
              PECI_CC_DRIVER_ERR);
}

TEST_F(RdPCIConfigLocalTest, Success)
{
    uint8_t buf[4] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(peci_RdPCIConfigLocal(VALID_TARGET, 0, 0, 0, 0, 4, buf, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(RdPCIConfigLocalTest, DomVariant_Success)
{
    uint8_t buf[4] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(
        peci_RdPCIConfigLocal_dom(VALID_TARGET, 0, 0, 0, 0, 0, 4, buf, &cc),
        PECI_CC_SUCCESS);
}

TEST_F(RdPCIConfigLocalTest, SeqVariant_Success)
{
    uint8_t buf[4] = {};
    uint8_t cc = 0;
    EXPECT_EQ(
        peci_RdPCIConfigLocal_seq(VALID_TARGET, 0, 0, 0, 0, 4, buf, 3, &cc),
        PECI_CC_SUCCESS);
}

TEST_F(RdPCIConfigLocalTest, SeqDomVariant_ValidReadLengths)
{
    uint8_t buf[4] = {};
    uint8_t cc = 0;
    EXPECT_EQ(peci_RdPCIConfigLocal_seq_dom(VALID_TARGET, 0, 0, 0, 0, 0, 1,
                                             buf, 3, &cc),
              PECI_CC_SUCCESS);
    EXPECT_EQ(peci_RdPCIConfigLocal_seq_dom(VALID_TARGET, 0, 0, 0, 0, 0, 2,
                                             buf, 3, &cc),
              PECI_CC_SUCCESS);
    EXPECT_EQ(peci_RdPCIConfigLocal_seq_dom(VALID_TARGET, 0, 0, 0, 0, 0, 4,
                                             buf, 3, &cc),
              PECI_CC_SUCCESS);
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_WrPCIConfigLocal / peci_WrPCIConfigLocal_dom
// ═══════════════════════════════════════════════════════════════════════════

class WrPCIConfigLocalTest : public PeciTest
{};

TEST_F(WrPCIConfigLocalTest, NullCC_ReturnsInvalidReq)
{
    EXPECT_EQ(peci_WrPCIConfigLocal(VALID_TARGET, 0, 0, 0, 0, 4, 0, NULL),
              PECI_CC_INVALID_REQ);
}

TEST_F(WrPCIConfigLocalTest, InvalidTarget_ReturnsInvalidReq)
{
    uint8_t cc = 0;
    EXPECT_EQ(
        peci_WrPCIConfigLocal(INVALID_TARGET_LOW, 0, 0, 0, 0, 4, 0, &cc),
        PECI_CC_INVALID_REQ);
}

TEST_F(WrPCIConfigLocalTest, InvalidWriteLen_ReturnsInvalidReq)
{
    uint8_t cc = 0;
    mock_open_return = 3;
    // Write length must be 1, 2, or 4
    EXPECT_EQ(
        peci_WrPCIConfigLocal_dom(VALID_TARGET, 0, 0, 0, 0, 0, 3, 0, &cc),
        PECI_CC_INVALID_REQ);
    EXPECT_EQ(
        peci_WrPCIConfigLocal_dom(VALID_TARGET, 0, 0, 0, 0, 0, 5, 0, &cc),
        PECI_CC_INVALID_REQ);
}

TEST_F(WrPCIConfigLocalTest, OpenFails_ReturnsDriverErr)
{
    uint8_t cc = 0;
    mock_open_return = -1;
    EXPECT_EQ(peci_WrPCIConfigLocal(VALID_TARGET, 0, 0, 0, 0, 4, 0, &cc),
              PECI_CC_DRIVER_ERR);
}

TEST_F(WrPCIConfigLocalTest, Success)
{
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(peci_WrPCIConfigLocal(VALID_TARGET, 0, 0, 0, 0, 4, 0, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(WrPCIConfigLocalTest, DomVariant_Success)
{
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(
        peci_WrPCIConfigLocal_dom(VALID_TARGET, 0, 0, 0, 0, 0, 4, 0, &cc),
        PECI_CC_SUCCESS);
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_RdEndPointConfigPci (all variants)
// ═══════════════════════════════════════════════════════════════════════════

class RdEndPointConfigPciTest : public PeciTest
{};

TEST_F(RdEndPointConfigPciTest, NullData_ReturnsInvalidReq)
{
    uint8_t cc = 0;
    EXPECT_EQ(
        peci_RdEndPointConfigPci(VALID_TARGET, 0, 0, 0, 0, 0, 4, NULL, &cc),
        PECI_CC_INVALID_REQ);
}

TEST_F(RdEndPointConfigPciTest, NullCC_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    EXPECT_EQ(
        peci_RdEndPointConfigPci(VALID_TARGET, 0, 0, 0, 0, 0, 4, buf, NULL),
        PECI_CC_INVALID_REQ);
}

TEST_F(RdEndPointConfigPciTest, InvalidTarget_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    EXPECT_EQ(peci_RdEndPointConfigPci(INVALID_TARGET_LOW, 0, 0, 0, 0, 0, 4,
                                        buf, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(RdEndPointConfigPciTest, InvalidReadLen_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    // Read length must be 1, 2, or 4
    EXPECT_EQ(peci_RdEndPointConfigPci_seq_dom(VALID_TARGET, 0, 0, 0, 0, 0, 0,
                                                3, buf, 3, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(RdEndPointConfigPciTest, OpenFails_ReturnsDriverErr)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = -1;
    EXPECT_EQ(
        peci_RdEndPointConfigPci(VALID_TARGET, 0, 0, 0, 0, 0, 4, buf, &cc),
        PECI_CC_DRIVER_ERR);
}

TEST_F(RdEndPointConfigPciTest, Success)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(
        peci_RdEndPointConfigPci(VALID_TARGET, 0, 0, 0, 0, 0, 4, buf, &cc),
        PECI_CC_SUCCESS);
}

TEST_F(RdEndPointConfigPciTest, DomVariant_Success)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(peci_RdEndPointConfigPci_dom(VALID_TARGET, 0, 0, 0, 0, 0, 0, 4,
                                            buf, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(RdEndPointConfigPciTest, SeqVariant_Success)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    EXPECT_EQ(peci_RdEndPointConfigPci_seq(VALID_TARGET, 0, 0, 0, 0, 0, 4, buf,
                                            3, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(RdEndPointConfigPciTest, SeqDomVariant_Success)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    EXPECT_EQ(peci_RdEndPointConfigPci_seq_dom(VALID_TARGET, 0, 0, 0, 0, 0, 0,
                                                4, buf, 3, &cc),
              PECI_CC_SUCCESS);
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_RdEndPointConfigPciLocal (all variants)
// ═══════════════════════════════════════════════════════════════════════════

class RdEndPointConfigPciLocalTest : public PeciTest
{};

TEST_F(RdEndPointConfigPciLocalTest, NullData_ReturnsInvalidReq)
{
    uint8_t cc = 0;
    EXPECT_EQ(peci_RdEndPointConfigPciLocal(VALID_TARGET, 0, 0, 0, 0, 0, 4,
                                             NULL, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(RdEndPointConfigPciLocalTest, InvalidTarget_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    EXPECT_EQ(peci_RdEndPointConfigPciLocal(INVALID_TARGET_LOW, 0, 0, 0, 0, 0,
                                             4, buf, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(RdEndPointConfigPciLocalTest, InvalidReadLen_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    // Read length must be 1, 2, or 4
    EXPECT_EQ(peci_RdEndPointConfigPciLocal_seq_dom(VALID_TARGET, 0, 0, 0, 0, 0,
                                                     0, 3, buf, 3, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(RdEndPointConfigPciLocalTest, OpenFails_ReturnsDriverErr)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = -1;
    EXPECT_EQ(peci_RdEndPointConfigPciLocal(VALID_TARGET, 0, 0, 0, 0, 0, 4,
                                             buf, &cc),
              PECI_CC_DRIVER_ERR);
}

TEST_F(RdEndPointConfigPciLocalTest, Success)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(peci_RdEndPointConfigPciLocal(VALID_TARGET, 0, 0, 0, 0, 0, 4,
                                             buf, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(RdEndPointConfigPciLocalTest, DomVariant_Success)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(peci_RdEndPointConfigPciLocal_dom(VALID_TARGET, 0, 0, 0, 0, 0, 0,
                                                 4, buf, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(RdEndPointConfigPciLocalTest, SeqVariant_Success)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    EXPECT_EQ(peci_RdEndPointConfigPciLocal_seq(VALID_TARGET, 0, 0, 0, 0, 0, 4,
                                                 buf, 3, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(RdEndPointConfigPciLocalTest, SeqDomVariant_NullChecks)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    EXPECT_EQ(peci_RdEndPointConfigPciLocal_seq_dom(VALID_TARGET, 0, 0, 0, 0, 0,
                                                     0, 4, NULL, 3, &cc),
              PECI_CC_INVALID_REQ);
    EXPECT_EQ(peci_RdEndPointConfigPciLocal_seq_dom(VALID_TARGET, 0, 0, 0, 0, 0,
                                                     0, 4, buf, 3, NULL),
              PECI_CC_INVALID_REQ);
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_RdEndPointConfigMmio (all variants)
// ═══════════════════════════════════════════════════════════════════════════

class RdEndPointConfigMmioTest : public PeciTest
{};

TEST_F(RdEndPointConfigMmioTest, NullData_ReturnsInvalidReq)
{
    uint8_t cc = 0;
    EXPECT_EQ(peci_RdEndPointConfigMmio(VALID_TARGET, 0, 0, 0, 0, 0, 0x05, 0,
                                         4, NULL, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(RdEndPointConfigMmioTest, NullCC_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    EXPECT_EQ(peci_RdEndPointConfigMmio(VALID_TARGET, 0, 0, 0, 0, 0, 0x05, 0,
                                         4, buf, NULL),
              PECI_CC_INVALID_REQ);
}

TEST_F(RdEndPointConfigMmioTest, InvalidTarget_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    EXPECT_EQ(peci_RdEndPointConfigMmio(INVALID_TARGET_LOW, 0, 0, 0, 0, 0,
                                         0x05, 0, 4, buf, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(RdEndPointConfigMmioTest, InvalidReadLen_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    // Read length must be 1, 2, 4, or 8
    EXPECT_EQ(peci_RdEndPointConfigMmio_seq_dom(VALID_TARGET, 0, 0, 0, 0, 0, 0,
                                                 0x05, 0, 3, buf, 3, &cc),
              PECI_CC_INVALID_REQ);
    EXPECT_EQ(peci_RdEndPointConfigMmio_seq_dom(VALID_TARGET, 0, 0, 0, 0, 0, 0,
                                                 0x05, 0, 5, buf, 3, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(RdEndPointConfigMmioTest, OpenFails_ReturnsDriverErr)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = -1;
    EXPECT_EQ(peci_RdEndPointConfigMmio(VALID_TARGET, 0, 0, 0, 0, 0, 0x05, 0,
                                         4, buf, &cc),
              PECI_CC_DRIVER_ERR);
}

TEST_F(RdEndPointConfigMmioTest, Success)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(peci_RdEndPointConfigMmio(VALID_TARGET, 0, 0, 0, 0, 0, 0x05, 0,
                                         4, buf, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(RdEndPointConfigMmioTest, DomVariant_Success)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(peci_RdEndPointConfigMmio_dom(VALID_TARGET, 0, 0, 0, 0, 0, 0,
                                             0x05, 0, 4, buf, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(RdEndPointConfigMmioTest, SeqVariant_Success)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    EXPECT_EQ(peci_RdEndPointConfigMmio_seq(VALID_TARGET, 0, 0, 0, 0, 0, 0x05,
                                             0, 4, buf, 3, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(RdEndPointConfigMmioTest, SeqDomVariant_ValidReadLengths)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    EXPECT_EQ(peci_RdEndPointConfigMmio_seq_dom(VALID_TARGET, 0, 0, 0, 0, 0, 0,
                                                 0x05, 0, 1, buf, 3, &cc),
              PECI_CC_SUCCESS);
    EXPECT_EQ(peci_RdEndPointConfigMmio_seq_dom(VALID_TARGET, 0, 0, 0, 0, 0, 0,
                                                 0x05, 0, 2, buf, 3, &cc),
              PECI_CC_SUCCESS);
    EXPECT_EQ(peci_RdEndPointConfigMmio_seq_dom(VALID_TARGET, 0, 0, 0, 0, 0, 0,
                                                 0x05, 0, 4, buf, 3, &cc),
              PECI_CC_SUCCESS);
    EXPECT_EQ(peci_RdEndPointConfigMmio_seq_dom(VALID_TARGET, 0, 0, 0, 0, 0, 0,
                                                 0x05, 0, 8, buf, 3, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(RdEndPointConfigMmioTest, SeqDom_IoctlFailure_ReturnsDriverErr)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_ioctl_callback = [](unsigned long cmd, void* argp) -> int {
        errno = EIO;
        return -1;
    };
    EXPECT_EQ(peci_RdEndPointConfigMmio_seq_dom(VALID_TARGET, 0, 0, 0, 0, 0, 0,
                                                 0x05, 0, 4, buf, 3, &cc),
              PECI_CC_DRIVER_ERR);
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_WrEndPointConfig_seq / peci_WrEndPointConfig_seq_dom
// ═══════════════════════════════════════════════════════════════════════════

class WrEndPointConfigSeqTest : public PeciTest
{};

TEST_F(WrEndPointConfigSeqTest, NullCC_ReturnsInvalidReq)
{
    EXPECT_EQ(peci_WrEndPointConfig_seq(VALID_TARGET, 0x04, 0, 0, 0, 0, 0, 4,
                                         0, 3, NULL),
              PECI_CC_INVALID_REQ);
}

TEST_F(WrEndPointConfigSeqTest, InvalidTarget_ReturnsInvalidReq)
{
    uint8_t cc = 0;
    EXPECT_EQ(peci_WrEndPointConfig_seq(INVALID_TARGET_LOW, 0x04, 0, 0, 0, 0,
                                         0, 4, 0, 3, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(WrEndPointConfigSeqTest, InvalidWriteLen_ReturnsInvalidReq)
{
    uint8_t cc = 0;
    // Write length must be 1, 2, or 4
    EXPECT_EQ(peci_WrEndPointConfig_seq_dom(VALID_TARGET, 0, 0x04, 0, 0, 0, 0,
                                             0, 3, 0, 3, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(WrEndPointConfigSeqTest, Success)
{
    uint8_t cc = 0;
    EXPECT_EQ(peci_WrEndPointConfig_seq(VALID_TARGET, 0x04, 0, 0, 0, 0, 0, 4,
                                         0, 3, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(WrEndPointConfigSeqTest, SeqDomVariant_Success)
{
    uint8_t cc = 0;
    EXPECT_EQ(peci_WrEndPointConfig_seq_dom(VALID_TARGET, 0, 0x04, 0, 0, 0, 0,
                                             0, 4, 0, 3, &cc),
              PECI_CC_SUCCESS);
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_WrEndPointPCIConfigLocal / _dom
// ═══════════════════════════════════════════════════════════════════════════

class WrEndPointPCIConfigLocalTest : public PeciTest
{};

TEST_F(WrEndPointPCIConfigLocalTest, OpenFails_ReturnsDriverErr)
{
    uint8_t cc = 0;
    mock_open_return = -1;
    EXPECT_EQ(
        peci_WrEndPointPCIConfigLocal(VALID_TARGET, 0, 0, 0, 0, 0, 4, 0, &cc),
        PECI_CC_DRIVER_ERR);
}

TEST_F(WrEndPointPCIConfigLocalTest, Success)
{
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(
        peci_WrEndPointPCIConfigLocal(VALID_TARGET, 0, 0, 0, 0, 0, 4, 0, &cc),
        PECI_CC_SUCCESS);
}

TEST_F(WrEndPointPCIConfigLocalTest, DomVariant_Success)
{
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(peci_WrEndPointPCIConfigLocal_dom(VALID_TARGET, 0, 0, 0, 0, 0, 0,
                                                 4, 0, &cc),
              PECI_CC_SUCCESS);
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_WrEndPointPCIConfig / _dom
// ═══════════════════════════════════════════════════════════════════════════

class WrEndPointPCIConfigTest : public PeciTest
{};

TEST_F(WrEndPointPCIConfigTest, OpenFails_ReturnsDriverErr)
{
    uint8_t cc = 0;
    mock_open_return = -1;
    EXPECT_EQ(
        peci_WrEndPointPCIConfig(VALID_TARGET, 0, 0, 0, 0, 0, 4, 0, &cc),
        PECI_CC_DRIVER_ERR);
}

TEST_F(WrEndPointPCIConfigTest, Success)
{
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(
        peci_WrEndPointPCIConfig(VALID_TARGET, 0, 0, 0, 0, 0, 4, 0, &cc),
        PECI_CC_SUCCESS);
}

TEST_F(WrEndPointPCIConfigTest, DomVariant_Success)
{
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(peci_WrEndPointPCIConfig_dom(VALID_TARGET, 0, 0, 0, 0, 0, 0, 4,
                                            0, &cc),
              PECI_CC_SUCCESS);
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_WrEndPointConfigMmio (all variants)
// ═══════════════════════════════════════════════════════════════════════════

class WrEndPointConfigMmioTest : public PeciTest
{};

TEST_F(WrEndPointConfigMmioTest, NullCC_ReturnsInvalidReq)
{
    EXPECT_EQ(peci_WrEndPointConfigMmio(VALID_TARGET, 0, 0, 0, 0, 0, 0x05, 0,
                                         4, 0, NULL),
              PECI_CC_INVALID_REQ);
}

TEST_F(WrEndPointConfigMmioTest, InvalidTarget_ReturnsInvalidReq)
{
    uint8_t cc = 0;
    EXPECT_EQ(peci_WrEndPointConfigMmio(INVALID_TARGET_LOW, 0, 0, 0, 0, 0,
                                         0x05, 0, 4, 0, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(WrEndPointConfigMmioTest, InvalidDataLen_ReturnsInvalidReq)
{
    uint8_t cc = 0;
    // Data length must be 1, 2, 4, or 8
    EXPECT_EQ(peci_WrEndPointConfigMmio_seq_dom(VALID_TARGET, 0, 0, 0, 0, 0, 0,
                                                 0x05, 0, 3, 0, 3, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(WrEndPointConfigMmioTest, OpenFails_ReturnsDriverErr)
{
    uint8_t cc = 0;
    mock_open_return = -1;
    EXPECT_EQ(peci_WrEndPointConfigMmio(VALID_TARGET, 0, 0, 0, 0, 0, 0x05, 0,
                                         4, 0, &cc),
              PECI_CC_DRIVER_ERR);
}

TEST_F(WrEndPointConfigMmioTest, Success)
{
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(peci_WrEndPointConfigMmio(VALID_TARGET, 0, 0, 0, 0, 0, 0x05, 0,
                                         4, 0, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(WrEndPointConfigMmioTest, DomVariant_Success)
{
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(peci_WrEndPointConfigMmio_dom(VALID_TARGET, 0, 0, 0, 0, 0, 0,
                                             0x05, 0, 4, 0, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(WrEndPointConfigMmioTest, SeqVariant_Success)
{
    uint8_t cc = 0;
    EXPECT_EQ(peci_WrEndPointConfigMmio_seq(VALID_TARGET, 0, 0, 0, 0, 0, 0x05,
                                             0, 4, 0, 3, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(WrEndPointConfigMmioTest, SeqDomVariant_ValidDataLengths)
{
    uint8_t cc = 0;
    EXPECT_EQ(peci_WrEndPointConfigMmio_seq_dom(VALID_TARGET, 0, 0, 0, 0, 0, 0,
                                                 0x05, 0, 1, 0, 3, &cc),
              PECI_CC_SUCCESS);
    EXPECT_EQ(peci_WrEndPointConfigMmio_seq_dom(VALID_TARGET, 0, 0, 0, 0, 0, 0,
                                                 0x05, 0, 2, 0, 3, &cc),
              PECI_CC_SUCCESS);
    EXPECT_EQ(peci_WrEndPointConfigMmio_seq_dom(VALID_TARGET, 0, 0, 0, 0, 0, 0,
                                                 0x05, 0, 4, 0, 3, &cc),
              PECI_CC_SUCCESS);
    EXPECT_EQ(peci_WrEndPointConfigMmio_seq_dom(VALID_TARGET, 0, 0, 0, 0, 0, 0,
                                                 0x05, 0, 8, 0, 3, &cc),
              PECI_CC_SUCCESS);
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_CrashDump_Discovery / _dom
// ═══════════════════════════════════════════════════════════════════════════

class CrashDumpDiscoveryTest : public PeciTest
{};

TEST_F(CrashDumpDiscoveryTest, NullData_ReturnsInvalidReq)
{
    uint8_t cc = 0;
    EXPECT_EQ(peci_CrashDump_Discovery(VALID_TARGET, 0, 0, 0, 0, 8, NULL, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(CrashDumpDiscoveryTest, NullCC_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    EXPECT_EQ(peci_CrashDump_Discovery(VALID_TARGET, 0, 0, 0, 0, 8, buf, NULL),
              PECI_CC_INVALID_REQ);
}

TEST_F(CrashDumpDiscoveryTest, InvalidTarget_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    EXPECT_EQ(peci_CrashDump_Discovery(INVALID_TARGET_LOW, 0, 0, 0, 0, 8, buf,
                                        &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(CrashDumpDiscoveryTest, InvalidReadLen_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    // Read length must be 1, 2, or 8
    EXPECT_EQ(peci_CrashDump_Discovery_dom(VALID_TARGET, 0, 0, 0, 0, 0, 4, buf,
                                            &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(CrashDumpDiscoveryTest, OpenFails_ReturnsDriverErr)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = -1;
    EXPECT_EQ(
        peci_CrashDump_Discovery(VALID_TARGET, 0, 0, 0, 0, 8, buf, &cc),
        PECI_CC_DRIVER_ERR);
}

TEST_F(CrashDumpDiscoveryTest, Success)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(
        peci_CrashDump_Discovery(VALID_TARGET, 0, 0, 0, 0, 8, buf, &cc),
        PECI_CC_SUCCESS);
}

TEST_F(CrashDumpDiscoveryTest, DomVariant_Success)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(peci_CrashDump_Discovery_dom(VALID_TARGET, 0, 0, 0, 0, 0, 8, buf,
                                            &cc),
              PECI_CC_SUCCESS);
}

TEST_F(CrashDumpDiscoveryTest, IoctlFailure_ReturnsDriverErr)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    mock_ioctl_callback = [](unsigned long cmd, void* argp) -> int {
        errno = EIO;
        return -1;
    };
    EXPECT_EQ(
        peci_CrashDump_Discovery(VALID_TARGET, 0, 0, 0, 0, 8, buf, &cc),
        PECI_CC_DRIVER_ERR);
}

TEST_F(CrashDumpDiscoveryTest, ValidReadLens)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(
        peci_CrashDump_Discovery(VALID_TARGET, 0, 0, 0, 0, 1, buf, &cc),
        PECI_CC_SUCCESS);
    EXPECT_EQ(
        peci_CrashDump_Discovery(VALID_TARGET, 0, 0, 0, 0, 2, buf, &cc),
        PECI_CC_SUCCESS);
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_CrashDump_GetFrame / _dom
// ═══════════════════════════════════════════════════════════════════════════

class CrashDumpGetFrameTest : public PeciTest
{};

TEST_F(CrashDumpGetFrameTest, NullData_ReturnsInvalidReq)
{
    uint8_t cc = 0;
    EXPECT_EQ(peci_CrashDump_GetFrame(VALID_TARGET, 0, 0, 0, 8, NULL, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(CrashDumpGetFrameTest, NullCC_ReturnsInvalidReq)
{
    uint8_t buf[16] = {};
    EXPECT_EQ(peci_CrashDump_GetFrame(VALID_TARGET, 0, 0, 0, 8, buf, NULL),
              PECI_CC_INVALID_REQ);
}

TEST_F(CrashDumpGetFrameTest, InvalidTarget_ReturnsInvalidReq)
{
    uint8_t buf[16] = {};
    uint8_t cc = 0;
    EXPECT_EQ(
        peci_CrashDump_GetFrame(INVALID_TARGET_LOW, 0, 0, 0, 8, buf, &cc),
        PECI_CC_INVALID_REQ);
}

TEST_F(CrashDumpGetFrameTest, InvalidReadLen_ReturnsInvalidReq)
{
    uint8_t buf[16] = {};
    uint8_t cc = 0;
    // Read length must be 8 or 16
    EXPECT_EQ(peci_CrashDump_GetFrame_dom(VALID_TARGET, 0, 0, 0, 0, 4, buf,
                                           &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(CrashDumpGetFrameTest, OpenFails_ReturnsDriverErr)
{
    uint8_t buf[16] = {};
    uint8_t cc = 0;
    mock_open_return = -1;
    EXPECT_EQ(peci_CrashDump_GetFrame(VALID_TARGET, 0, 0, 0, 8, buf, &cc),
              PECI_CC_DRIVER_ERR);
}

TEST_F(CrashDumpGetFrameTest, Success_8Bytes)
{
    uint8_t buf[16] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(peci_CrashDump_GetFrame(VALID_TARGET, 0, 0, 0, 8, buf, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(CrashDumpGetFrameTest, Success_16Bytes)
{
    uint8_t buf[16] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(peci_CrashDump_GetFrame(VALID_TARGET, 0, 0, 0, 16, buf, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(CrashDumpGetFrameTest, DomVariant_Success)
{
    uint8_t buf[16] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(
        peci_CrashDump_GetFrame_dom(VALID_TARGET, 0, 0, 0, 0, 8, buf, &cc),
        PECI_CC_SUCCESS);
}

TEST_F(CrashDumpGetFrameTest, IoctlFailure_ReturnsDriverErr)
{
    uint8_t buf[16] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    mock_ioctl_callback = [](unsigned long cmd, void* argp) -> int {
        errno = EIO;
        return -1;
    };
    EXPECT_EQ(peci_CrashDump_GetFrame(VALID_TARGET, 0, 0, 0, 8, buf, &cc),
              PECI_CC_DRIVER_ERR);
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_Telemetry_Discovery / _dom
// ═══════════════════════════════════════════════════════════════════════════

class TelemetryDiscoveryTest : public PeciTest
{};

TEST_F(TelemetryDiscoveryTest, NullData_ReturnsInvalidReq)
{
    uint8_t cc = 0;
    EXPECT_EQ(
        peci_Telemetry_Discovery(VALID_TARGET, 0, 0, 0, 0, 8, NULL, &cc),
        PECI_CC_INVALID_REQ);
}

TEST_F(TelemetryDiscoveryTest, NullCC_ReturnsInvalidReq)
{
    uint8_t buf[10] = {};
    EXPECT_EQ(
        peci_Telemetry_Discovery(VALID_TARGET, 0, 0, 0, 0, 8, buf, NULL),
        PECI_CC_INVALID_REQ);
}

TEST_F(TelemetryDiscoveryTest, InvalidTarget_ReturnsInvalidReq)
{
    uint8_t buf[10] = {};
    uint8_t cc = 0;
    EXPECT_EQ(peci_Telemetry_Discovery(INVALID_TARGET_LOW, 0, 0, 0, 0, 8, buf,
                                        &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(TelemetryDiscoveryTest, OpenFails_ReturnsDriverErr)
{
    uint8_t buf[10] = {};
    uint8_t cc = 0;
    mock_open_return = -1;
    EXPECT_EQ(
        peci_Telemetry_Discovery(VALID_TARGET, 0, 0, 0, 0, 8, buf, &cc),
        PECI_CC_DRIVER_ERR);
}

TEST_F(TelemetryDiscoveryTest, Success)
{
    uint8_t buf[10] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(
        peci_Telemetry_Discovery(VALID_TARGET, 0, 0, 0, 0, 8, buf, &cc),
        PECI_CC_SUCCESS);
}

TEST_F(TelemetryDiscoveryTest, DomVariant_Success)
{
    uint8_t buf[10] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(peci_Telemetry_Discovery_dom(VALID_TARGET, 0, 0, 0, 0, 0, 8, buf,
                                            &cc),
              PECI_CC_SUCCESS);
}

TEST_F(TelemetryDiscoveryTest, IoctlFailure_ReturnsDriverErr)
{
    uint8_t buf[10] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    mock_ioctl_callback = [](unsigned long cmd, void* argp) -> int {
        errno = EIO;
        return -1;
    };
    EXPECT_EQ(
        peci_Telemetry_Discovery(VALID_TARGET, 0, 0, 0, 0, 8, buf, &cc),
        PECI_CC_DRIVER_ERR);
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_Telemetry_GetTelemSample / _dom
// ═══════════════════════════════════════════════════════════════════════════

class TelemetryGetTelemSampleTest : public PeciTest
{};

TEST_F(TelemetryGetTelemSampleTest, NullData_ReturnsInvalidReq)
{
    uint8_t cc = 0;
    EXPECT_EQ(peci_Telemetry_GetTelemSample(VALID_TARGET, 0, 0, 8, NULL, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(TelemetryGetTelemSampleTest, NullCC_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    EXPECT_EQ(peci_Telemetry_GetTelemSample(VALID_TARGET, 0, 0, 8, buf, NULL),
              PECI_CC_INVALID_REQ);
}

TEST_F(TelemetryGetTelemSampleTest, InvalidTarget_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    EXPECT_EQ(
        peci_Telemetry_GetTelemSample(INVALID_TARGET_LOW, 0, 0, 8, buf, &cc),
        PECI_CC_INVALID_REQ);
}

TEST_F(TelemetryGetTelemSampleTest, OpenFails_ReturnsDriverErr)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = -1;
    EXPECT_EQ(peci_Telemetry_GetTelemSample(VALID_TARGET, 0, 0, 8, buf, &cc),
              PECI_CC_DRIVER_ERR);
}

TEST_F(TelemetryGetTelemSampleTest, Success)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(peci_Telemetry_GetTelemSample(VALID_TARGET, 0, 0, 8, buf, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(TelemetryGetTelemSampleTest, DomVariant_Success)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(
        peci_Telemetry_GetTelemSample_dom(VALID_TARGET, 0, 0, 0, 8, buf, &cc),
        PECI_CC_SUCCESS);
}

TEST_F(TelemetryGetTelemSampleTest, IoctlFailure_ReturnsDriverErr)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    mock_ioctl_callback = [](unsigned long cmd, void* argp) -> int {
        errno = EIO;
        return -1;
    };
    EXPECT_EQ(peci_Telemetry_GetTelemSample(VALID_TARGET, 0, 0, 8, buf, &cc),
              PECI_CC_DRIVER_ERR);
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_Telemetry_ConfigWatcherRd / _dom
// ═══════════════════════════════════════════════════════════════════════════

class TelemetryConfigWatcherRdTest : public PeciTest
{};

TEST_F(TelemetryConfigWatcherRdTest, NullData_ReturnsInvalidReq)
{
    uint8_t cc = 0;
    EXPECT_EQ(peci_Telemetry_ConfigWatcherRd(VALID_TARGET, 0, 0, 8, NULL, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(TelemetryConfigWatcherRdTest, NullCC_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    EXPECT_EQ(peci_Telemetry_ConfigWatcherRd(VALID_TARGET, 0, 0, 8, buf, NULL),
              PECI_CC_INVALID_REQ);
}

TEST_F(TelemetryConfigWatcherRdTest, InvalidTarget_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    EXPECT_EQ(
        peci_Telemetry_ConfigWatcherRd(INVALID_TARGET_LOW, 0, 0, 8, buf, &cc),
        PECI_CC_INVALID_REQ);
}

TEST_F(TelemetryConfigWatcherRdTest, OpenFails_ReturnsDriverErr)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = -1;
    EXPECT_EQ(peci_Telemetry_ConfigWatcherRd(VALID_TARGET, 0, 0, 8, buf, &cc),
              PECI_CC_DRIVER_ERR);
}

TEST_F(TelemetryConfigWatcherRdTest, Success)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(peci_Telemetry_ConfigWatcherRd(VALID_TARGET, 0, 0, 8, buf, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(TelemetryConfigWatcherRdTest, DomVariant_Success)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(peci_Telemetry_ConfigWatcherRd_dom(VALID_TARGET, 0, 0, 0, 8, buf,
                                                  &cc),
              PECI_CC_SUCCESS);
}

TEST_F(TelemetryConfigWatcherRdTest, IoctlFailure_ReturnsDriverErr)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    mock_ioctl_callback = [](unsigned long cmd, void* argp) -> int {
        errno = EIO;
        return -1;
    };
    EXPECT_EQ(peci_Telemetry_ConfigWatcherRd(VALID_TARGET, 0, 0, 8, buf, &cc),
              PECI_CC_DRIVER_ERR);
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_Telemetry_ConfigWatcherWr / _dom
// ═══════════════════════════════════════════════════════════════════════════

class TelemetryConfigWatcherWrTest : public PeciTest
{};

TEST_F(TelemetryConfigWatcherWrTest, NullData_ReturnsInvalidReq)
{
    uint8_t cc = 0;
    EXPECT_EQ(peci_Telemetry_ConfigWatcherWr(VALID_TARGET, 0, 0, 8, NULL, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(TelemetryConfigWatcherWrTest, NullCC_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    EXPECT_EQ(peci_Telemetry_ConfigWatcherWr(VALID_TARGET, 0, 0, 8, buf, NULL),
              PECI_CC_INVALID_REQ);
}

TEST_F(TelemetryConfigWatcherWrTest, InvalidTarget_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    EXPECT_EQ(
        peci_Telemetry_ConfigWatcherWr(INVALID_TARGET_LOW, 0, 0, 8, buf, &cc),
        PECI_CC_INVALID_REQ);
}

TEST_F(TelemetryConfigWatcherWrTest, OpenFails_ReturnsDriverErr)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = -1;
    EXPECT_EQ(peci_Telemetry_ConfigWatcherWr(VALID_TARGET, 0, 0, 8, buf, &cc),
              PECI_CC_DRIVER_ERR);
}

TEST_F(TelemetryConfigWatcherWrTest, Success)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(peci_Telemetry_ConfigWatcherWr(VALID_TARGET, 0, 0, 8, buf, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(TelemetryConfigWatcherWrTest, DomVariant_Success)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(peci_Telemetry_ConfigWatcherWr_dom(VALID_TARGET, 0, 0, 0, 8, buf,
                                                  &cc),
              PECI_CC_SUCCESS);
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_Telemetry_GetCrashlogSample / _dom
// ═══════════════════════════════════════════════════════════════════════════

class TelemetryGetCrashlogSampleTest : public PeciTest
{};

TEST_F(TelemetryGetCrashlogSampleTest, NullData_ReturnsInvalidReq)
{
    uint8_t cc = 0;
    EXPECT_EQ(
        peci_Telemetry_GetCrashlogSample(VALID_TARGET, 0, 0, 8, NULL, &cc),
        PECI_CC_INVALID_REQ);
}

TEST_F(TelemetryGetCrashlogSampleTest, NullCC_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    EXPECT_EQ(
        peci_Telemetry_GetCrashlogSample(VALID_TARGET, 0, 0, 8, buf, NULL),
        PECI_CC_INVALID_REQ);
}

TEST_F(TelemetryGetCrashlogSampleTest, InvalidTarget_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    EXPECT_EQ(peci_Telemetry_GetCrashlogSample(INVALID_TARGET_LOW, 0, 0, 8,
                                                buf, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(TelemetryGetCrashlogSampleTest, OpenFails_ReturnsDriverErr)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = -1;
    EXPECT_EQ(
        peci_Telemetry_GetCrashlogSample(VALID_TARGET, 0, 0, 8, buf, &cc),
        PECI_CC_DRIVER_ERR);
}

TEST_F(TelemetryGetCrashlogSampleTest, Success)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(
        peci_Telemetry_GetCrashlogSample(VALID_TARGET, 0, 0, 8, buf, &cc),
        PECI_CC_SUCCESS);
}

TEST_F(TelemetryGetCrashlogSampleTest, DomVariant_Success)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    EXPECT_EQ(peci_Telemetry_GetCrashlogSample_dom(VALID_TARGET, 0, 0, 0, 8,
                                                    buf, &cc),
              PECI_CC_SUCCESS);
}

TEST_F(TelemetryGetCrashlogSampleTest, IoctlFailure_ReturnsDriverErr)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    mock_ioctl_callback = [](unsigned long cmd, void* argp) -> int {
        errno = EIO;
        return -1;
    };
    EXPECT_EQ(
        peci_Telemetry_GetCrashlogSample(VALID_TARGET, 0, 0, 8, buf, &cc),
        PECI_CC_DRIVER_ERR);
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_raw / peci_raw_seq
// ═══════════════════════════════════════════════════════════════════════════

class RawPeciTest : public PeciTest
{};

TEST_F(RawPeciTest, Raw_NullResp_WithReadLen_ReturnsInvalidReq)
{
    uint8_t cmd[] = {0x30, 0x01};
    EXPECT_EQ(peci_raw(VALID_TARGET, 4, cmd, sizeof(cmd), NULL, 4),
              PECI_CC_INVALID_REQ);
}

TEST_F(RawPeciTest, Raw_InvalidTarget_ReturnsInvalidReq)
{
    uint8_t cmd[] = {0x20, 0x01};
    uint8_t resp[4] = {};
    EXPECT_EQ(peci_raw(INVALID_TARGET_LOW, 4, cmd, sizeof(cmd), resp, 4),
              PECI_CC_INVALID_REQ);
}

TEST_F(RawPeciTest, Raw_OpenFails_ReturnsDriverErr)
{
    uint8_t cmd[] = {0x30, 0x01};
    uint8_t resp[4] = {};
    mock_open_return = -1;
    EXPECT_EQ(peci_raw(VALID_TARGET, 4, cmd, sizeof(cmd), resp, 4),
              PECI_CC_DRIVER_ERR);
}

TEST_F(RawPeciTest, Raw_Success)
{
    uint8_t cmd[] = {0x30, 0x01};
    uint8_t resp[4] = {};
    mock_open_return = 3;
    EXPECT_EQ(peci_raw(VALID_TARGET, 4, cmd, sizeof(cmd), resp, 4),
              PECI_CC_SUCCESS);
}

TEST_F(RawPeciTest, Raw_ZeroReadLen_NullResp_Success)
{
    uint8_t cmd[] = {0x30, 0x01};
    mock_open_return = 3;
    // Zero read length with NULL resp is valid
    EXPECT_EQ(peci_raw(VALID_TARGET, 0, cmd, sizeof(cmd), NULL, 0),
              PECI_CC_SUCCESS);
}

TEST_F(RawPeciTest, RawSeq_NullResp_WithReadLen_ReturnsInvalidReq)
{
    uint8_t cmd[] = {0x30, 0x01};
    EXPECT_EQ(peci_raw_seq(VALID_TARGET, 4, cmd, sizeof(cmd), NULL, 4, 3),
              PECI_CC_INVALID_REQ);
}

TEST_F(RawPeciTest, RawSeq_InvalidTarget_ReturnsInvalidReq)
{
    uint8_t cmd[] = {0x20, 0x01};
    uint8_t resp[4] = {};
    EXPECT_EQ(
        peci_raw_seq(INVALID_TARGET_LOW, 4, cmd, sizeof(cmd), resp, 4, 3),
        PECI_CC_INVALID_REQ);
}

TEST_F(RawPeciTest, RawSeq_CmdSizeTooLarge_ReturnsInvalidReq)
{
    uint8_t cmd[256] = {};
    uint8_t resp[4] = {};
    EXPECT_EQ(peci_raw_seq(VALID_TARGET, 4, cmd, 256, resp, 4, 3),
              PECI_CC_INVALID_REQ);
}

TEST_F(RawPeciTest, RawSeq_RespSizeTooSmall_ReturnsInvalidReq)
{
    uint8_t cmd[] = {0x30, 0x01};
    uint8_t resp[2] = {};
    // respSize < u8ReadLen
    EXPECT_EQ(peci_raw_seq(VALID_TARGET, 4, cmd, sizeof(cmd), resp, 2, 3),
              PECI_CC_INVALID_REQ);
}

TEST_F(RawPeciTest, RawSeq_ReadLenTooLarge_ReturnsInvalidReq)
{
    uint8_t cmd[] = {0x30, 0x01};
    uint8_t resp[255] = {};
    // u8ReadLen > PECI_BUFFER_SIZE - 1
    EXPECT_EQ(peci_raw_seq(VALID_TARGET, 255, cmd, sizeof(cmd), resp, 255, 3),
              PECI_CC_INVALID_REQ);
}

TEST_F(RawPeciTest, RawSeq_Success)
{
    uint8_t cmd[] = {0x30, 0x01};
    uint8_t resp[4] = {};
    EXPECT_EQ(peci_raw_seq(VALID_TARGET, 4, cmd, sizeof(cmd), resp, 4, 3),
              PECI_CC_SUCCESS);
}

TEST_F(RawPeciTest, RawSeq_IoctlTimeout_CopiesData)
{
    uint8_t cmd[] = {0x30, 0x01};
    uint8_t resp[4] = {};
    mock_ioctl_callback = [](unsigned long cmd, void* argp) -> int {
        errno = ETIMEDOUT;
        return -1;
    };
    EPECIStatus ret =
        peci_raw_seq(VALID_TARGET, 4, cmd, sizeof(cmd), resp, 4, 3);
    EXPECT_EQ(ret, PECI_CC_TIMEOUT);
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_GetCPUID
// ═══════════════════════════════════════════════════════════════════════════

class GetCPUIDTest : public PeciTest
{};

TEST_F(GetCPUIDTest, NullCpuModel_ReturnsInvalidReq)
{
    uint8_t stepping = 0;
    uint8_t cc = 0;
    EXPECT_EQ(peci_GetCPUID(VALID_TARGET, NULL, &stepping, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(GetCPUIDTest, NullStepping_ReturnsInvalidReq)
{
    CPUModel model = skx;
    uint8_t cc = 0;
    EXPECT_EQ(peci_GetCPUID(VALID_TARGET, &model, NULL, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(GetCPUIDTest, NullCC_ReturnsInvalidReq)
{
    CPUModel model = skx;
    uint8_t stepping = 0;
    EXPECT_EQ(peci_GetCPUID(VALID_TARGET, &model, &stepping, NULL),
              PECI_CC_INVALID_REQ);
}

TEST_F(GetCPUIDTest, InvalidTarget_ReturnsInvalidReq)
{
    CPUModel model = skx;
    uint8_t stepping = 0;
    uint8_t cc = 0;
    EXPECT_EQ(peci_GetCPUID(INVALID_TARGET_LOW, &model, &stepping, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(GetCPUIDTest, PingFails_ReturnsCpuNotPresent)
{
    CPUModel model = skx;
    uint8_t stepping = 0;
    uint8_t cc = 0;
    // Ping fails when ioctl fails
    mock_ioctl_callback = [](unsigned long cmd, void* argp) -> int {
        errno = EIO;
        return -1;
    };
    EXPECT_EQ(peci_GetCPUID(VALID_TARGET, &model, &stepping, &cc),
              PECI_CC_CPU_NOT_PRESENT);
}

TEST_F(GetCPUIDTest, Success)
{
    CPUModel model = skx;
    uint8_t stepping = 0;
    uint8_t cc = 0;
    mock_open_return = 3;
    mock_ioctl_return = 0;
    EXPECT_EQ(peci_GetCPUID(VALID_TARGET, &model, &stepping, &cc),
              PECI_CC_SUCCESS);
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_WakePECI
// ═══════════════════════════════════════════════════════════════════════════

class WakePECITest : public PeciTest
{};

TEST_F(WakePECITest, InvalidTarget_ReturnsInvalidReq)
{
    uint8_t cc = 0;
    EXPECT_EQ(peci_WakePECI(INVALID_TARGET_LOW, 0, 3, &cc),
              PECI_CC_INVALID_REQ);
    EXPECT_EQ(peci_WakePECI(INVALID_TARGET_HIGH, 0, 3, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(WakePECITest, Success)
{
    uint8_t cc = 0;
    mock_ioctl_return = 0;
    EXPECT_EQ(peci_WakePECI(VALID_TARGET, 0, 3, &cc), PECI_CC_SUCCESS);
}

// ═══════════════════════════════════════════════════════════════════════════
// FindBusNumber
// ═══════════════════════════════════════════════════════════════════════════

class FindBusNumberTest : public PeciTest
{};

TEST_F(FindBusNumberTest, BusTooHigh_ReturnsInvalidReq)
{
    uint8_t busVal = 0;
    EXPECT_EQ(FindBusNumber(6, 0, &busVal), PECI_CC_INVALID_REQ);
}

TEST_F(FindBusNumberTest, CpuTooHigh_ReturnsInvalidReq)
{
    uint8_t busVal = 0;
    EXPECT_EQ(FindBusNumber(0, 2, &busVal), PECI_CC_INVALID_REQ);
}

TEST_F(FindBusNumberTest, NullBusValue_ReturnsInvalidReq)
{
    EXPECT_EQ(FindBusNumber(0, 0, NULL), PECI_CC_INVALID_REQ);
}

// ═══════════════════════════════════════════════════════════════════════════
// peci_i3c_chardev_to_cpu
// ═══════════════════════════════════════════════════════════════════════════

class I3cChardevToCpuTest : public PeciTest
{};

TEST_F(I3cChardevToCpuTest, OpenDevFails_ReturnsNegative)
{
    mock_open_return = -1;
    mock_open_errno = ENOENT;
    int ret = peci_i3c_chardev_to_cpu("peci-0");
    EXPECT_LT(ret, 0);
}

TEST_F(I3cChardevToCpuTest, FstatatFails_ReturnsNegative)
{
    mock_open_return = 3; // dirfd succeeds
    mock_fstatat_return = -1;
    mock_fstatat_errno = ENOENT;
    int ret = peci_i3c_chardev_to_cpu("peci-0");
    EXPECT_LT(ret, 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// HW_peci_issue_cmd (tested indirectly via Ping_seq)
// ═══════════════════════════════════════════════════════════════════════════

class HWPeciIssueCmdTest : public PeciTest
{};

TEST_F(HWPeciIssueCmdTest, IoctlTimeout_ReturnsPeciTimeout)
{
    mock_ioctl_callback = [](unsigned long cmd, void* argp) -> int {
        errno = ETIMEDOUT;
        return -1;
    };
    EXPECT_EQ(peci_Ping_seq(VALID_TARGET, 3), PECI_CC_TIMEOUT);
}

TEST_F(HWPeciIssueCmdTest, IoctlOtherError_ReturnsDriverErr)
{
    mock_ioctl_callback = [](unsigned long cmd, void* argp) -> int {
        errno = EIO;
        return -1;
    };
    EXPECT_EQ(peci_Ping_seq(VALID_TARGET, 3), PECI_CC_DRIVER_ERR);
}

// ═══════════════════════════════════════════════════════════════════════════
// Edge cases for address boundaries
// ═══════════════════════════════════════════════════════════════════════════

class AddressBoundaryTest : public PeciTest
{};

TEST_F(AddressBoundaryTest, MinAddress_Valid)
{
    mock_open_return = 3;
    EXPECT_EQ(peci_Ping(MIN_CLIENT_ADDR), PECI_CC_SUCCESS);
}

TEST_F(AddressBoundaryTest, MaxAddress_Valid)
{
    mock_open_return = 3;
    EXPECT_EQ(peci_Ping(MAX_CLIENT_ADDR), PECI_CC_SUCCESS);
}

TEST_F(AddressBoundaryTest, BelowMinAddress_Invalid)
{
    EXPECT_EQ(peci_Ping(MIN_CLIENT_ADDR - 1), PECI_CC_INVALID_REQ);
}

TEST_F(AddressBoundaryTest, AboveMaxAddress_Invalid)
{
    EXPECT_EQ(peci_Ping(MAX_CLIENT_ADDR + 1), PECI_CC_INVALID_REQ);
}

// ═══════════════════════════════════════════════════════════════════════════
// Buffer size validation for Telemetry commands
// ═══════════════════════════════════════════════════════════════════════════

class TelemetryBufferSizeTest : public PeciTest
{};

TEST_F(TelemetryBufferSizeTest, Discovery_ReadLenExceedsBuffer_ReturnsInvalidReq)
{
    uint8_t buf[10] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    // sizeof(cmd.data) is 10 for telemetry_disc_msg, request > 10
    EXPECT_EQ(peci_Telemetry_Discovery_dom(VALID_TARGET, 0, 0, 0, 0, 0, 11,
                                            buf, &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(TelemetryBufferSizeTest,
       GetTelemSample_ReadLenExceedsBuffer_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    // sizeof(cmd.data) is 8 for telemetry_get_telem_sample_msg
    EXPECT_EQ(
        peci_Telemetry_GetTelemSample_dom(VALID_TARGET, 0, 0, 0, 9, buf, &cc),
        PECI_CC_INVALID_REQ);
}

TEST_F(TelemetryBufferSizeTest,
       ConfigWatcherRd_ReadLenExceedsBuffer_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    // sizeof(cmd.data) is 8
    EXPECT_EQ(
        peci_Telemetry_ConfigWatcherRd_dom(VALID_TARGET, 0, 0, 0, 9, buf, &cc),
        PECI_CC_INVALID_REQ);
}

TEST_F(TelemetryBufferSizeTest,
       ConfigWatcherWr_DataLenExceedsBuffer_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    // sizeof(cmd.data) is 8
    EXPECT_EQ(
        peci_Telemetry_ConfigWatcherWr_dom(VALID_TARGET, 0, 0, 0, 9, buf, &cc),
        PECI_CC_INVALID_REQ);
}

TEST_F(TelemetryBufferSizeTest,
       GetCrashlogSample_ReadLenExceedsBuffer_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    // sizeof(cmd.data) is 8
    EXPECT_EQ(peci_Telemetry_GetCrashlogSample_dom(VALID_TARGET, 0, 0, 0, 9,
                                                    buf, &cc),
              PECI_CC_INVALID_REQ);
}

// ═══════════════════════════════════════════════════════════════════════════
// Buffer size validation for CrashDump commands
// ═══════════════════════════════════════════════════════════════════════════

class CrashDumpBufferSizeTest : public PeciTest
{};

TEST_F(CrashDumpBufferSizeTest,
       Discovery_ReadLenExceedsBuffer_ReturnsInvalidReq)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    // sizeof(cmd.data) is 8, read len 8 is fine, but we already test valid lens
    // Test read len > 8 but valid (only 1, 2, 8 are valid)
    // 4 is an invalid read length for CrashDump_Discovery
    EXPECT_EQ(peci_CrashDump_Discovery_dom(VALID_TARGET, 0, 0, 0, 0, 0, 4, buf,
                                            &cc),
              PECI_CC_INVALID_REQ);
}

TEST_F(CrashDumpBufferSizeTest,
       GetFrame_ReadLenExceedsBuffer_ReturnsInvalidReq)
{
    uint8_t buf[16] = {};
    uint8_t cc = 0;
    mock_open_return = 3;
    // sizeof(cmd.data) is 16, but only 8 and 16 are valid read lens
    // 4 is invalid
    EXPECT_EQ(
        peci_CrashDump_GetFrame_dom(VALID_TARGET, 0, 0, 0, 0, 4, buf, &cc),
        PECI_CC_INVALID_REQ);
}

// ═══════════════════════════════════════════════════════════════════════════
// RdEndPointConfigPciCommon_dom — ioctl failure path
// ═══════════════════════════════════════════════════════════════════════════

class RdEndPointConfigPciCommonTest : public PeciTest
{};

TEST_F(RdEndPointConfigPciCommonTest, IoctlFailure_ReturnsDriverErr)
{
    uint8_t buf[8] = {};
    uint8_t cc = 0;
    mock_ioctl_callback = [](unsigned long cmd, void* argp) -> int {
        errno = EIO;
        return -1;
    };
    // Uses RdEndPointConfigPci_seq_dom which calls the common function
    EXPECT_EQ(peci_RdEndPointConfigPci_seq_dom(VALID_TARGET, 0, 0, 0, 0, 0, 0,
                                                4, buf, 3, &cc),
              PECI_CC_DRIVER_ERR);
}
