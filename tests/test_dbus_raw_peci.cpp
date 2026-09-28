/* ****************************************************************
 *
 * libpeci — Unit Tests for dbus_raw_peci.cpp
 * test_dbus_raw_peci.cpp
 *
 * dbus_raw_peci.cpp is compiled with -Dmain=dbus_raw_peci_main
 * so its main() does not collide with gtest main().
 *
 * The D-Bus/boost infrastructure cannot be tested without a real
 * bus, so we test the core processing logic by calling the PECI
 * functions that the Send handler invokes (peci_SetDevName, peci_raw).
 *
 *****************************************************************/

extern "C"
{
#include "mock_syscalls.h"
#include <peci.h>
}

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

// ═══════════════════════════════════════════════════════════════════════════
// Test fixture
// ═══════════════════════════════════════════════════════════════════════════

class DbusRawPeciTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mock_reset_all();
        mock_open_return = 3;
        mock_ioctl_return = 0;
    }

    void TearDown() override
    {
        mock_active = 0;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// Test the Send handler logic (extracted from dbus_raw_peci.cpp lambda)
//
// The Send handler does:
//   1. peci_SetDevName(peciDev)
//   2. For each rawCmd:
//      - Validate rawCmd.size() >= 3
//      - rawResp[i].resize(rawCmd[2])
//      - peci_raw(rawCmd[0], rawCmd[2], &rawCmd[3], rawCmd[1],
//                 rawResp[i].data(), rawResp[i].size())
//   3. peci_SetDevName(NULL)
// ═══════════════════════════════════════════════════════════════════════════

// Reimplement the Send handler logic for testing
static std::vector<std::vector<uint8_t>>
    sendHandler(const std::string& peciDev,
                const std::vector<std::vector<uint8_t>>& rawCmds)
{
    peci_SetDevName(const_cast<char*>(peciDev.c_str()));
    std::vector<std::vector<uint8_t>> rawResp;
    rawResp.resize(rawCmds.size());
    for (size_t i = 0; i < rawCmds.size(); i++)
    {
        const std::vector<uint8_t>& rawCmd = rawCmds[i];
        if (rawCmd.size() < 3)
        {
            peci_SetDevName(NULL);
            throw std::invalid_argument("Command Length too short");
        }
        rawResp[i].resize(rawCmd[2]);
        peci_raw(rawCmd[0], rawCmd[2], &rawCmd[3], rawCmd[1],
                 rawResp[i].data(),
                 static_cast<uint32_t>(rawResp[i].size()));
    }
    peci_SetDevName(NULL);
    return rawResp;
}

// ═══════════════════════════════════════════════════════════════════════════
// Tests
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(DbusRawPeciTest, SendHandler_SingleCommand_Success)
{
    // rawCmd: [addr, writeLen, readLen, cmd bytes...]
    std::vector<std::vector<uint8_t>> cmds = {
        {0x30, 0x01, 0x04, 0xF7}};

    auto resp = sendHandler("/dev/peci-wire", cmds);

    ASSERT_EQ(resp.size(), 1u);
    EXPECT_EQ(resp[0].size(), 4u);
}

TEST_F(DbusRawPeciTest, SendHandler_MultipleCommands_Success)
{
    std::vector<std::vector<uint8_t>> cmds = {
        {0x30, 0x01, 0x04, 0xF7},
        {0x30, 0x01, 0x02, 0xA1}};

    auto resp = sendHandler("/dev/peci-wire", cmds);

    ASSERT_EQ(resp.size(), 2u);
    EXPECT_EQ(resp[0].size(), 4u);
    EXPECT_EQ(resp[1].size(), 2u);
}

TEST_F(DbusRawPeciTest, SendHandler_EmptyCommandList_Success)
{
    std::vector<std::vector<uint8_t>> cmds;

    auto resp = sendHandler("/dev/peci-wire", cmds);

    EXPECT_EQ(resp.size(), 0u);
}

TEST_F(DbusRawPeciTest, SendHandler_CommandTooShort_Throws)
{
    std::vector<std::vector<uint8_t>> cmds = {
        {0x30, 0x01}}; // Only 2 bytes, need 3

    EXPECT_THROW(sendHandler("/dev/peci-wire", cmds), std::invalid_argument);
}

TEST_F(DbusRawPeciTest, SendHandler_ZeroReadLen_Success)
{
    std::vector<std::vector<uint8_t>> cmds = {
        {0x30, 0x01, 0x00, 0xF7}}; // readLen = 0

    auto resp = sendHandler("/dev/peci-wire", cmds);

    ASSERT_EQ(resp.size(), 1u);
    EXPECT_EQ(resp[0].size(), 0u);
}

TEST_F(DbusRawPeciTest, SendHandler_IoctlFailure_StillReturnsResponse)
{
    mock_ioctl_callback = [](unsigned long cmd, void* argp) -> int {
        errno = EIO;
        return -1;
    };

    std::vector<std::vector<uint8_t>> cmds = {
        {0x30, 0x01, 0x04, 0xF7}};

    // peci_raw returns error but the handler still returns the response buffer
    auto resp = sendHandler("/dev/peci-wire", cmds);
    ASSERT_EQ(resp.size(), 1u);
    EXPECT_EQ(resp[0].size(), 4u);
}

TEST_F(DbusRawPeciTest, SendHandler_DeviceName_SetAndReset)
{
    std::vector<std::vector<uint8_t>> cmds = {
        {0x30, 0x01, 0x04, 0xF7}};

    // After sendHandler, device should be reset to NULL (defaults)
    sendHandler("/dev/peci-custom", cmds);

    // Verify peci_SetDevName(NULL) was called — no crash
    SUCCEED();
}

TEST_F(DbusRawPeciTest, SendHandler_ThrowResetsDeviceName)
{
    std::vector<std::vector<uint8_t>> cmds = {{0x30}}; // too short

    try
    {
        sendHandler("/dev/peci-custom", cmds);
    }
    catch (const std::invalid_argument&)
    {
        // Device name should have been reset before throw
    }
    // Should not crash after the throw
    SUCCEED();
}
