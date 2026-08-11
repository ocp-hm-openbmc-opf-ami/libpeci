/* ****************************************************************
 *
 * libpeci — Unit Tests for peci_cmds.c
 * test_peci_cmds.cpp
 *
 * Covers the CLI tool functions:
 *   getTimeDifference, Usage, printLoopSummary, main (all command paths)
 *
 * peci_cmds.c is compiled with -Dmain=peci_cmds_main so its main()
 * does not collide with the gtest main().
 *
 *****************************************************************/

extern "C"
{
#include "mock_syscalls.h"
#include <peci.h>

// peci_cmds.c's main is renamed via -Dmain=peci_cmds_main
int peci_cmds_main(int argc, char* argv[]);

// Helper functions from peci_cmds.c
double getTimeDifference(const struct timespec begin);

// peci_GetDIB is declared in peci_cmds.c but not in peci.h
EPECIStatus peci_GetDIB(uint8_t target, uint64_t* dib);
}

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

// ═══════════════════════════════════════════════════════════════════════════
// Test fixture
// ═══════════════════════════════════════════════════════════════════════════

class PeciCmdsTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        mock_reset_all();
        // All peci calls should succeed by default
        mock_open_return = 3;
        mock_ioctl_return = 0;
    }

    void TearDown() override
    {
        mock_active = 0;
    }

    // Helper to build argv from a command string
    static std::vector<char*> makeArgv(std::vector<std::string>& args)
    {
        std::vector<char*> argv;
        for (auto& a : args)
        {
            argv.push_back(a.data());
        }
        argv.push_back(nullptr);
        return argv;
    }

    int runCmd(std::vector<std::string> args)
    {
        auto argv = makeArgv(args);
        // Reset getopt fully on glibc (optind=0 triggers re-initialization)
        optind = 0;
        return peci_cmds_main(static_cast<int>(args.size()), argv.data());
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// getTimeDifference
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(PeciCmdsTest, GetTimeDifference_ReturnsPositive)
{
    struct timespec begin;
    clock_gettime(CLOCK_REALTIME, &begin);
    // Small busy loop to ensure non-zero time
    volatile int x = 0;
    for (int i = 0; i < 1000; i++)
    {
        x += i;
    }
    double diff = getTimeDifference(begin);
    EXPECT_GE(diff, 0.0);
}

// ═══════════════════════════════════════════════════════════════════════════
// Usage / help
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(PeciCmdsTest, NoArgs_PrintsUsage_Returns0)
{
    EXPECT_EQ(runCmd({"peci_cmds"}), 0);
}

TEST_F(PeciCmdsTest, HelpFlag_Returns0)
{
    EXPECT_EQ(runCmd({"peci_cmds", "-h"}), 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// Option parsing edge cases
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(PeciCmdsTest, InvalidAddress_Returns1)
{
    EXPECT_EQ(runCmd({"peci_cmds", "-a", "0x10", "ping"}), 1);
}

TEST_F(PeciCmdsTest, InvalidDomainId_Returns1)
{
    EXPECT_EQ(runCmd({"peci_cmds", "-i", "200", "ping"}), 1);
}

TEST_F(PeciCmdsTest, InvalidSize_Returns1)
{
    EXPECT_EQ(runCmd({"peci_cmds", "-s", "3", "ping"}), 1);
}

TEST_F(PeciCmdsTest, InvalidLoopCount_Returns1)
{
    EXPECT_EQ(runCmd({"peci_cmds", "-l", "0", "ping"}), 1);
}

TEST_F(PeciCmdsTest, UnrecognizedCommand_Returns1)
{
    EXPECT_EQ(runCmd({"peci_cmds", "foobar"}), 1);
}

TEST_F(PeciCmdsTest, DeviceOption_DoesNotCrash)
{
    EXPECT_EQ(runCmd({"peci_cmds", "-d", "/dev/peci-wire", "ping"}), 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// Ping command
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(PeciCmdsTest, Ping_Success)
{
    EXPECT_EQ(runCmd({"peci_cmds", "ping"}), 0);
}

TEST_F(PeciCmdsTest, Ping_Verbose_Success)
{
    EXPECT_EQ(runCmd({"peci_cmds", "-v", "ping"}), 0);
}

TEST_F(PeciCmdsTest, Ping_VerboseWithTime_Success)
{
    EXPECT_EQ(runCmd({"peci_cmds", "-v", "-t", "ping"}), 0);
}

TEST_F(PeciCmdsTest, Ping_Looped_Success)
{
    EXPECT_EQ(runCmd({"peci_cmds", "-l", "3", "ping"}), 0);
}

TEST_F(PeciCmdsTest, Ping_VerboseLoopedWithTime)
{
    EXPECT_EQ(runCmd({"peci_cmds", "-v", "-t", "-l", "2", "ping"}), 0);
}

TEST_F(PeciCmdsTest, Ping_Failure)
{
    mock_ioctl_callback = [](unsigned long cmd, void* argp) -> int {
        errno = EIO;
        return -1;
    };
    EXPECT_EQ(runCmd({"peci_cmds", "-v", "ping"}), 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// GetDIB command
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(PeciCmdsTest, GetDIB_Success)
{
    EXPECT_EQ(runCmd({"peci_cmds", "getdib"}), 0);
}

TEST_F(PeciCmdsTest, GetDIB_Verbose_Success)
{
    EXPECT_EQ(runCmd({"peci_cmds", "-v", "getdib"}), 0);
}

TEST_F(PeciCmdsTest, GetDIB_Failure)
{
    mock_ioctl_callback = [](unsigned long cmd, void* argp) -> int {
        errno = EIO;
        return -1;
    };
    EXPECT_EQ(runCmd({"peci_cmds", "-v", "getdib"}), 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// GetTemp command
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(PeciCmdsTest, GetTemp_Success)
{
    EXPECT_EQ(runCmd({"peci_cmds", "gettemp"}), 0);
}

TEST_F(PeciCmdsTest, GetTemp_Verbose_Success)
{
    EXPECT_EQ(runCmd({"peci_cmds", "-v", "gettemp"}), 0);
}

TEST_F(PeciCmdsTest, GetTemp_Failure)
{
    mock_ioctl_callback = [](unsigned long cmd, void* argp) -> int {
        errno = EIO;
        return -1;
    };
    EXPECT_EQ(runCmd({"peci_cmds", "-v", "gettemp"}), 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// RdPkgConfig command
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(PeciCmdsTest, RdPkgConfig_Success)
{
    EXPECT_EQ(runCmd({"peci_cmds", "rdpkgconfig", "0", "0"}), 0);
}

TEST_F(PeciCmdsTest, RdPkgConfig_Verbose_Success)
{
    EXPECT_EQ(runCmd({"peci_cmds", "-v", "rdpkgconfig", "0", "0"}), 0);
}

TEST_F(PeciCmdsTest, RdPkgConfig_WrongArgs_Returns1)
{
    EXPECT_EQ(runCmd({"peci_cmds", "rdpkgconfig", "0"}), 1);
}

TEST_F(PeciCmdsTest, RdPkgConfig_Failure)
{
    mock_ioctl_callback = [](unsigned long cmd, void* argp) -> int {
        errno = EIO;
        return -1;
    };
    EXPECT_EQ(runCmd({"peci_cmds", "-v", "rdpkgconfig", "0", "0"}), 0);
}

TEST_F(PeciCmdsTest, RdPkgConfig_Looped)
{
    EXPECT_EQ(
        runCmd({"peci_cmds", "-l", "2", "rdpkgconfig", "0", "0"}), 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// WrPkgConfig command
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(PeciCmdsTest, WrPkgConfig_Success)
{
    EXPECT_EQ(runCmd({"peci_cmds", "wrpkgconfig", "0", "0", "0"}), 0);
}

TEST_F(PeciCmdsTest, WrPkgConfig_WrongArgs_Returns1)
{
    EXPECT_EQ(runCmd({"peci_cmds", "wrpkgconfig", "0"}), 1);
}

TEST_F(PeciCmdsTest, WrPkgConfig_Verbose_Success)
{
    EXPECT_EQ(runCmd({"peci_cmds", "-v", "wrpkgconfig", "0", "0", "0"}), 0);
}

TEST_F(PeciCmdsTest, WrPkgConfig_Failure)
{
    mock_ioctl_callback = [](unsigned long cmd, void* argp) -> int {
        errno = EIO;
        return -1;
    };
    EXPECT_EQ(runCmd({"peci_cmds", "-v", "wrpkgconfig", "0", "0", "0"}), 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// RdIAMSR command
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(PeciCmdsTest, RdIAMSR_Success)
{
    EXPECT_EQ(runCmd({"peci_cmds", "rdiamsr", "0", "0"}), 0);
}

TEST_F(PeciCmdsTest, RdIAMSR_WrongArgs_Returns1)
{
    EXPECT_EQ(runCmd({"peci_cmds", "rdiamsr", "0"}), 1);
}

TEST_F(PeciCmdsTest, RdIAMSR_Verbose_Success)
{
    EXPECT_EQ(runCmd({"peci_cmds", "-v", "rdiamsr", "0", "0"}), 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// RdPCIConfig command
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(PeciCmdsTest, RdPCIConfig_1Arg)
{
    EXPECT_EQ(runCmd({"peci_cmds", "rdpciconfig", "0"}), 0);
}

TEST_F(PeciCmdsTest, RdPCIConfig_2Args)
{
    EXPECT_EQ(runCmd({"peci_cmds", "rdpciconfig", "0", "0"}), 0);
}

TEST_F(PeciCmdsTest, RdPCIConfig_3Args)
{
    EXPECT_EQ(runCmd({"peci_cmds", "rdpciconfig", "0", "0", "0"}), 0);
}

TEST_F(PeciCmdsTest, RdPCIConfig_4Args)
{
    EXPECT_EQ(runCmd({"peci_cmds", "rdpciconfig", "0", "0", "0", "0"}), 0);
}

TEST_F(PeciCmdsTest, RdPCIConfig_WrongArgs_Returns1)
{
    EXPECT_EQ(runCmd({"peci_cmds", "rdpciconfig"}), 1);
}

TEST_F(PeciCmdsTest, RdPCIConfig_Verbose)
{
    EXPECT_EQ(
        runCmd({"peci_cmds", "-v", "rdpciconfig", "0", "0", "0", "0"}), 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// RdPCIConfigLocal command
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(PeciCmdsTest, RdPCIConfigLocal_4Args)
{
    EXPECT_EQ(
        runCmd({"peci_cmds", "rdpciconfiglocal", "0", "0", "0", "0"}), 0);
}

TEST_F(PeciCmdsTest, RdPCIConfigLocal_WrongArgs_Returns1)
{
    EXPECT_EQ(runCmd({"peci_cmds", "rdpciconfiglocal"}), 1);
}

// ═══════════════════════════════════════════════════════════════════════════
// WrPCIConfigLocal command
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(PeciCmdsTest, WrPCIConfigLocal_5Args)
{
    EXPECT_EQ(runCmd({"peci_cmds", "wrpciconfiglocal", "0", "0", "0", "0",
                      "0x1234"}),
              0);
}

TEST_F(PeciCmdsTest, WrPCIConfigLocal_WrongArgs_Returns1)
{
    EXPECT_EQ(runCmd({"peci_cmds", "wrpciconfiglocal", "0"}), 1);
}

TEST_F(PeciCmdsTest, WrPCIConfigLocal_Verbose)
{
    EXPECT_EQ(runCmd({"peci_cmds", "-v", "wrpciconfiglocal", "0", "0", "0",
                      "0", "0x1234"}),
              0);
}

// ═══════════════════════════════════════════════════════════════════════════
// RdEndpointConfigPCILocal command
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(PeciCmdsTest, RdEndpointConfigPCILocal_5Args)
{
    EXPECT_EQ(runCmd({"peci_cmds", "rdendpointconfigpcilocal", "0", "0", "0",
                      "0", "0"}),
              0);
}

TEST_F(PeciCmdsTest, RdEndpointConfigPCILocal_WrongArgs_Returns1)
{
    EXPECT_EQ(runCmd({"peci_cmds", "rdendpointconfigpcilocal", "0"}), 1);
}

// ═══════════════════════════════════════════════════════════════════════════
// WrEndpointConfigPCILocal command
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(PeciCmdsTest, WrEndpointConfigPCILocal_6Args)
{
    EXPECT_EQ(runCmd({"peci_cmds", "wrendpointconfigpcilocal", "0", "0", "0",
                      "0", "0", "0x1234"}),
              0);
}

TEST_F(PeciCmdsTest, WrEndpointConfigPCILocal_WrongArgs_Returns1)
{
    EXPECT_EQ(runCmd({"peci_cmds", "wrendpointconfigpcilocal", "0"}), 1);
}

// ═══════════════════════════════════════════════════════════════════════════
// RdEndpointConfigPCI command
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(PeciCmdsTest, RdEndpointConfigPCI_5Args)
{
    EXPECT_EQ(runCmd({"peci_cmds", "rdendpointconfigpci", "0", "0", "0", "0",
                      "0"}),
              0);
}

TEST_F(PeciCmdsTest, RdEndpointConfigPCI_WrongArgs_Returns1)
{
    EXPECT_EQ(runCmd({"peci_cmds", "rdendpointconfigpci", "0"}), 1);
}

// ═══════════════════════════════════════════════════════════════════════════
// WrEndpointConfigPCI command
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(PeciCmdsTest, WrEndpointConfigPCI_6Args)
{
    EXPECT_EQ(runCmd({"peci_cmds", "wrendpointconfigpci", "0", "0", "0", "0",
                      "0", "0x1234"}),
              0);
}

TEST_F(PeciCmdsTest, WrEndpointConfigPCI_WrongArgs_Returns1)
{
    EXPECT_EQ(runCmd({"peci_cmds", "wrendpointconfigpci", "0"}), 1);
}

// ═══════════════════════════════════════════════════════════════════════════
// RdEndpointConfigMMIO command
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(PeciCmdsTest, RdEndpointConfigMMIO_7Args)
{
    EXPECT_EQ(runCmd({"peci_cmds", "rdendpointconfigmmio", "5", "0", "0", "0",
                      "0", "0", "0"}),
              0);
}

TEST_F(PeciCmdsTest, RdEndpointConfigMMIO_WrongArgs_Returns1)
{
    EXPECT_EQ(runCmd({"peci_cmds", "rdendpointconfigmmio", "0"}), 1);
}

TEST_F(PeciCmdsTest, RdEndpointConfigMMIO_Verbose)
{
    EXPECT_EQ(runCmd({"peci_cmds", "-v", "rdendpointconfigmmio", "5", "0", "0",
                      "0", "0", "0", "0"}),
              0);
}

// ═══════════════════════════════════════════════════════════════════════════
// WrEndpointConfigMMIO command
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(PeciCmdsTest, WrEndpointConfigMMIO_8Args)
{
    EXPECT_EQ(runCmd({"peci_cmds", "wrendpointconfigmmio", "5", "0", "0", "0",
                      "0", "0", "0", "0x1234"}),
              0);
}

TEST_F(PeciCmdsTest, WrEndpointConfigMMIO_WrongArgs_Returns1)
{
    EXPECT_EQ(runCmd({"peci_cmds", "wrendpointconfigmmio", "0"}), 1);
}

// ═══════════════════════════════════════════════════════════════════════════
// Telemetry Discovery (opcode 1)
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(PeciCmdsTest, TelemetryDiscovery_SubOp0)
{
    EXPECT_EQ(
        runCmd({"peci_cmds", "telemetry", "1", "0", "0", "0", "0"}), 0);
}

TEST_F(PeciCmdsTest, TelemetryDiscovery_SubOp1)
{
    EXPECT_EQ(
        runCmd({"peci_cmds", "telemetry", "1", "1", "0", "0", "0"}), 0);
}

TEST_F(PeciCmdsTest, TelemetryDiscovery_SubOp2_Param1)
{
    EXPECT_EQ(
        runCmd({"peci_cmds", "telemetry", "1", "2", "1", "0", "0"}), 0);
}

TEST_F(PeciCmdsTest, TelemetryDiscovery_SubOp2_Param2)
{
    EXPECT_EQ(
        runCmd({"peci_cmds", "telemetry", "1", "2", "2", "0", "0"}), 0);
}

TEST_F(PeciCmdsTest, TelemetryDiscovery_SubOp2_Param3)
{
    EXPECT_EQ(
        runCmd({"peci_cmds", "telemetry", "1", "2", "3", "0", "0"}), 0);
}

TEST_F(PeciCmdsTest, TelemetryDiscovery_SubOp2_Param4)
{
    EXPECT_EQ(
        runCmd({"peci_cmds", "telemetry", "1", "2", "4", "0", "0"}), 0);
}

TEST_F(PeciCmdsTest, TelemetryDiscovery_SubOp3)
{
    EXPECT_EQ(
        runCmd({"peci_cmds", "telemetry", "1", "3", "0", "0", "0"}), 0);
}

TEST_F(PeciCmdsTest, TelemetryDiscovery_SubOp4)
{
    EXPECT_EQ(
        runCmd({"peci_cmds", "telemetry", "1", "4", "0", "0", "0"}), 0);
}

TEST_F(PeciCmdsTest, TelemetryDiscovery_SubOp5)
{
    EXPECT_EQ(
        runCmd({"peci_cmds", "telemetry", "1", "5", "0", "0", "0"}), 0);
}

TEST_F(PeciCmdsTest, TelemetryDiscovery_WrongArgs_Returns1)
{
    EXPECT_EQ(runCmd({"peci_cmds", "telemetry", "1", "0"}), 1);
}

TEST_F(PeciCmdsTest, TelemetryDiscovery_Verbose)
{
    EXPECT_EQ(runCmd({"peci_cmds", "-v", "telemetry", "1", "0", "0", "0",
                      "0"}),
              0);
}

TEST_F(PeciCmdsTest, TelemetryDiscovery_Failure)
{
    mock_ioctl_callback = [](unsigned long cmd, void* argp) -> int {
        errno = EIO;
        return -1;
    };
    EXPECT_EQ(runCmd({"peci_cmds", "-v", "telemetry", "1", "0", "0", "0",
                      "0"}),
              0);
}

// ═══════════════════════════════════════════════════════════════════════════
// Telemetry GetTelemSample (opcode 2)
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(PeciCmdsTest, TelemetryGetTelemSample_Success)
{
    EXPECT_EQ(runCmd({"peci_cmds", "telemetry", "2", "0", "0"}), 0);
}

TEST_F(PeciCmdsTest, TelemetryGetTelemSample_WrongArgs_Returns1)
{
    EXPECT_EQ(runCmd({"peci_cmds", "telemetry", "2", "0"}), 1);
}

TEST_F(PeciCmdsTest, TelemetryGetTelemSample_Verbose)
{
    EXPECT_EQ(
        runCmd({"peci_cmds", "-v", "telemetry", "2", "0", "0"}), 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// Telemetry ConfigWatcher (opcode 3)
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(PeciCmdsTest, TelemetryConfigWatcherRd_Success)
{
    EXPECT_EQ(
        runCmd({"peci_cmds", "telemetry", "3", "0", "0", "0"}), 0);
}

TEST_F(PeciCmdsTest, TelemetryConfigWatcherWr_Success)
{
    EXPECT_EQ(
        runCmd({"peci_cmds", "telemetry", "3", "1", "0", "0", "0"}), 0);
}

TEST_F(PeciCmdsTest, TelemetryConfigWatcher_NoRWParam_Returns1)
{
    EXPECT_EQ(runCmd({"peci_cmds", "telemetry", "3"}), 1);
}

TEST_F(PeciCmdsTest, TelemetryConfigWatcherRd_WrongArgs_Returns1)
{
    EXPECT_EQ(runCmd({"peci_cmds", "telemetry", "3", "0", "0"}), 1);
}

TEST_F(PeciCmdsTest, TelemetryConfigWatcherWr_WrongArgs_Returns1)
{
    EXPECT_EQ(runCmd({"peci_cmds", "telemetry", "3", "1", "0"}), 1);
}

TEST_F(PeciCmdsTest, TelemetryConfigWatcher_InvalidRW_Returns1)
{
    EXPECT_EQ(
        runCmd({"peci_cmds", "telemetry", "3", "5", "0", "0"}), 1);
}

TEST_F(PeciCmdsTest, TelemetryConfigWatcherRd_Verbose)
{
    EXPECT_EQ(runCmd({"peci_cmds", "-v", "telemetry", "3", "0", "0", "0"}), 0);
}

TEST_F(PeciCmdsTest, TelemetryConfigWatcherWr_Verbose)
{
    EXPECT_EQ(
        runCmd({"peci_cmds", "-v", "telemetry", "3", "1", "0", "0", "0"}), 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// Telemetry GetCrashlogSample (opcode 12)
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(PeciCmdsTest, TelemetryGetCrashlogSample_Success)
{
    EXPECT_EQ(runCmd({"peci_cmds", "telemetry", "12", "0", "0"}), 0);
}

TEST_F(PeciCmdsTest, TelemetryGetCrashlogSample_WrongArgs_Returns1)
{
    EXPECT_EQ(runCmd({"peci_cmds", "telemetry", "12", "0"}), 1);
}

TEST_F(PeciCmdsTest, TelemetryGetCrashlogSample_Verbose)
{
    EXPECT_EQ(
        runCmd({"peci_cmds", "-v", "telemetry", "12", "0", "0"}), 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// Telemetry unsupported opcode
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(PeciCmdsTest, TelemetryUnsupportedOpcode_Returns1)
{
    EXPECT_EQ(runCmd({"peci_cmds", "telemetry", "99"}), 1);
}

TEST_F(PeciCmdsTest, TelemetryNoOpcode_Returns1)
{
    EXPECT_EQ(runCmd({"peci_cmds", "telemetry"}), 1);
}

// ═══════════════════════════════════════════════════════════════════════════
// Raw command
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(PeciCmdsTest, Raw_Success)
{
    EXPECT_EQ(
        runCmd({"peci_cmds", "raw", "0x30", "2", "4", "0xF7", "0x00"}), 0);
}

TEST_F(PeciCmdsTest, Raw_TooFewArgs_Returns1)
{
    EXPECT_EQ(runCmd({"peci_cmds", "raw", "0x30", "2"}), 1);
}

TEST_F(PeciCmdsTest, Raw_IncorrectWriteLen_Returns1)
{
    // writeLength=1 but we provide 3 extra args
    EXPECT_EQ(runCmd({"peci_cmds", "raw", "0x30", "1", "4", "0x01", "0x02",
                      "0x03"}),
              1);
}

TEST_F(PeciCmdsTest, Raw_Verbose)
{
    EXPECT_EQ(runCmd({"peci_cmds", "-v", "raw", "0x30", "2", "4", "0xF7",
                      "0x00"}),
              0);
}

TEST_F(PeciCmdsTest, Raw_Looped)
{
    EXPECT_EQ(runCmd({"peci_cmds", "-l", "2", "raw", "0x30", "2", "4", "0xF7",
                      "0x00"}),
              0);
}

TEST_F(PeciCmdsTest, Raw_Failure)
{
    mock_ioctl_callback = [](unsigned long cmd, void* argp) -> int {
        errno = EIO;
        return -1;
    };
    EXPECT_EQ(
        runCmd({"peci_cmds", "raw", "0x30", "2", "4", "0xF7", "0x00"}), 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// Time measurement
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(PeciCmdsTest, TimeMeasurement_Ping)
{
    EXPECT_EQ(runCmd({"peci_cmds", "-t", "ping"}), 0);
}

TEST_F(PeciCmdsTest, TimeMeasurement_Looped)
{
    EXPECT_EQ(runCmd({"peci_cmds", "-t", "-l", "2", "ping"}), 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// Custom address and domain
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(PeciCmdsTest, CustomAddress_Success)
{
    EXPECT_EQ(runCmd({"peci_cmds", "-a", "0x31", "ping"}), 0);
}

TEST_F(PeciCmdsTest, CustomDomainId_Success)
{
    EXPECT_EQ(runCmd({"peci_cmds", "-i", "1", "ping"}), 0);
}

TEST_F(PeciCmdsTest, CustomSize_Success)
{
    EXPECT_EQ(runCmd({"peci_cmds", "-s", "8", "rdpkgconfig", "0", "0"}), 0);
}
