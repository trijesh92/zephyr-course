#ifndef USB_CLI_H
#define USB_CLI_H

#include "usb_app.h"

#include <cstdint>
#include <memory>
#include <string>

/**
 * @brief Command-line application that wires together a transport connection
 *        and a protocol implementation, parses arguments with argparse.hpp,
 *        dispatches USB commands, and prints results to the console.
 *
 * Keeping the CLI in its own class (single responsibility) means main()
 * stays minimal and the CLI can be unit-tested or reused independently.
 */
class UsbCli {
public:
    /**
     * Construct the CLI with ownership of the connection and protocol.
     * @param connection  USB transport (WinUSB, etc.)
     * @param protocol    USB protocol (wire frame encode/decode)
     */
    UsbCli(std::unique_ptr<IUsbConnection> connection,
           std::unique_ptr<IUsbProtocol> protocol);
    ~UsbCli() = default;

    // Non-copyable, non-movable.
    UsbCli(const UsbCli&) = delete;
    UsbCli& operator=(const UsbCli&) = delete;

    /**
     * Parse command-line arguments, connect to the device, run the command,
     * print the result, and disconnect.
     * @return process exit code (0 = success, 1 = error).
     */
    int run(int argc, char* argv[]);

private:
    void print_response_hex(const UsbResponse& resp);

    std::unique_ptr<IUsbConnection> connection_;
    std::unique_ptr<IUsbProtocol> protocol_;
};

#endif // USB_CLI_H
