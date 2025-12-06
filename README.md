# FlipBitChat - BitChat for Flipper Zero

A decentralized peer-to-peer messaging application for the Flipper Zero, built on top of BitChat's BLE mesh networking technology.

## Overview

FlipBitChat brings BitChat's encrypted, serverless messaging capabilities to the Flipper Zero handheld device. Using Bluetooth Low Energy (BLE) 5.4, it creates an ad-hoc mesh network for offline communication without requiring internet connectivity.

### Key Features

- **Offline Mesh Networking**: Pure peer-to-peer communication via BLE mesh
- **End-to-End Encryption**: ChaCha20-Poly1305 AEAD encryption using mbedtls
- **Ephemeral Identity**: No accounts or registration required
- **Multi-hop Message Relay**: Messages propagate through the mesh network (max 7 hops)
- **Compact GUI**: Optimized for Flipper Zero's 128x64 monochrome LCD
- **Low Resource Footprint**: Designed for 256KB SRAM constraint

## Architecture

FlipBitChat follows a modular architecture aligned with BitChat's protocol stack:

```
┌─────────────────────────────────────┐
│     Application Layer (GUI)         │
│   - Chat view (text display)        │
│   - Message composition              │
│   - Peer list                        │
└─────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────┐
│    Protocol Layer (BitChat)         │
│   - Packet framing                   │
│   - Message routing (TTL)            │
│   - Fragmentation                    │
└─────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────┐
│    Encryption Layer (mbedtls)       │
│   - ChaCha20-Poly1305                │
│   - Curve25519 key exchange          │
│   - SHA-256 hashing                  │
└─────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────┐
│    Transport Layer (BLE)            │
│   - furi_hal_bt APIs                 │
│   - BLE GATT characteristics         │
│   - Peer discovery                   │
└─────────────────────────────────────┘
```

## Project Structure

```
flip-bitchat/
├── application.fam          # Flipper app manifest
├── flip_bitchat.c          # Main app entry point
├── flip_bitchat.h          # App configuration and constants
├── ble/                    # BLE mesh networking
│   ├── ble_manager.h       # BLE API definitions
│   └── ble_manager.c       # BLE implementation
├── crypto/                 # Cryptography (mbedtls)
│   ├── crypto.h            # Crypto API definitions
│   └── crypto.c            # Crypto implementation
├── gui/                    # Flipper Zero GUI
│   ├── views.h             # View definitions
│   └── chat_view.c         # Chat display view
├── protocol/               # BitChat protocol
│   ├── packet.h            # Packet structure definitions
│   └── packet.c            # Packet serialization
└── README.md               # This file
```

## BitChat Protocol Integration

FlipBitChat implements the [BitChat Protocol](../WHITEPAPER.md) with the following components:

### Packet Format

Follows the BitChat binary packet format (from whitepaper):

| Field           | Size     | Description                          |
|-----------------|----------|--------------------------------------|
| Version         | 1 byte   | Protocol version (currently `1`)     |
| Type            | 1 byte   | Message type enum                    |
| TTL             | 1 byte   | Time-to-live for mesh routing        |
| Timestamp       | 8 bytes  | Millisecond timestamp                |
| Flags           | 1 byte   | Bitmask for optional fields          |
| Payload Length  | 2 bytes  | Length of payload                    |
| Sender ID       | 8 bytes  | Truncated peer ID                    |
| Recipient ID    | 8 bytes  | Optional recipient ID (broadcast if 0xFF..FF) |
| Payload         | Variable | Message content                      |
| Signature       | 64 bytes | Optional Ed25519 signature           |

### Message Types

- `PacketTypeMessage` (0x01): Chat messages
- `PacketTypeDeliveryAck` (0x02): Delivery acknowledgments
- `PacketTypeReadReceipt` (0x03): Read receipts
- `PacketTypeNoiseHandshake*` (0x04-0x06): Noise protocol handshake
- `PacketTypePeerAnnounce` (0x07): Peer discovery
- `PacketTypeFragment*` (0x08-0x0A): Message fragmentation

### Mesh Routing

Messages propagate using BitChat's efficient gossip protocol:

1. Bloom filter tracks recently seen packet IDs (prevents loops)
2. TTL decrements at each hop (max 7 hops)
3. Packets with TTL=0 are not relayed further
4. Private messages remain encrypted end-to-end through relays

## Building

### Prerequisites

1. Install [ufbt (Flipper Build Tool)](https://github.com/flipperdevices/flipperzero-ufbt):
   ```bash
   pip3 install ufbt
   ```

2. Update Flipper firmware SDK:
   ```bash
   ufbt update
   ```

### Build the App

From the repository root:

```bash
cd flip-bitchat
ufbt
```

This will:
- Compile all C sources
- Link with Flipper SDK and mbedtls
- Generate `.fap` (Flipper Application Package) file

### Install to Flipper Zero

Connect your Flipper Zero via USB, then:

```bash
ufbt launch
```

Or manually copy the `.fap` file to the Flipper's SD card under `/ext/apps/Communication/`.

## Development

### Running in Debug Mode

```bash
ufbt debug
```

This launches the app with GDB attached for debugging.

### Viewing Logs

Flipper logs can be viewed via serial console:

```bash
ufbt cli
```

Look for logs tagged with `FlipBitChat`, `BLEManager`, `Crypto`, etc.

### Code Style

Follow Flipper's coding conventions:
- 4-space indentation
- Snake case for functions and variables
- Doxygen-style comments for public APIs
- Use `furi_assert()` for parameter validation

## Flipper Zero SDK References

### Core APIs

- **FURI**: Real-time OS primitives
  - `furi.h` - Core types and utilities
  - `furi_hal.h` - Hardware abstraction layer
  - `furi_hal_bt.h` - Bluetooth HAL

- **GUI**: Display and input
  - `gui/gui.h` - GUI subsystem
  - `gui/view_dispatcher.h` - View management
  - `gui/scene_manager.h` - Scene navigation
  - `gui/canvas.h` - Drawing primitives

- **Storage**: File system access
  - `storage/storage.h` - Storage API

### BLE APIs

The Flipper Zero has built-in BLE 5.4 support. Key functions:

- `furi_hal_bt_start_advertising()` - Broadcast presence
- `furi_hal_bt_stop_advertising()` - Stop advertising
- `furi_hal_bt_is_connected()` - Check connection status
- Custom GATT services/characteristics for BitChat protocol

### Cryptography (mbedtls)

Flipper includes mbedtls. Relevant modules:

- `mbedtls/chacha20.h` - ChaCha20 stream cipher
- `mbedtls/poly1305.h` - Poly1305 MAC
- `mbedtls/chachapoly.h` - Combined ChaCha20-Poly1305 AEAD
- `mbedtls/ecdh.h` - Curve25519 key exchange
- `mbedtls/sha256.h` - SHA-256 hashing

## Implementation Status

### ✅ Completed

- [x] Project structure and build configuration
- [x] Application manifest (`application.fam`)
- [x] Main app skeleton with ViewDispatcher
- [x] BLE manager scaffolding
- [x] Crypto module interface
- [x] BitChat protocol packet structures
- [x] Chat view GUI component

### 🚧 In Progress / TODO

- [ ] Implement actual BLE GATT service/characteristics
- [ ] Integrate real mbedtls crypto (currently placeholders)
- [ ] Complete Noise protocol handshake (XX pattern)
- [ ] Implement message composition (TextInput view)
- [ ] Add peer list view
- [ ] Implement message retry service
- [ ] Add delivery acknowledgments and read receipts
- [ ] Implement fragmentation for large messages
- [ ] Add storage for message history (limited)
- [ ] Create settings view (username, etc.)
- [ ] Add icon graphics (`A_BitChat_14`)
- [ ] Optimize memory usage for 256KB SRAM constraint
- [ ] Battery optimization (duty cycling, adaptive scanning)

## Constraints and Considerations

### Hardware Limitations

- **RAM**: 256KB total (shared with radio stack)
  - Keep message history minimal (e.g., last 32 messages)
  - Use stack allocation where possible
  - Free resources promptly

- **Display**: 128x64 monochrome LCD
  - Text-only UI, minimal graphics
  - ~5 lines of chat history visible at once
  - Simple scrolling mechanism

- **No Internet**: Completely offline
  - No Nostr integration (Bluetooth only)
  - Ephemeral peer IDs generated at startup
  - No persistent user accounts

### Protocol Adaptations

- **Simplified Noise**: May use static keys instead of full XX handshake to save RAM
- **Limited Mesh**: Fewer max hops (7) due to device constraints
- **No Bloom Filter**: May use simpler duplicate detection if memory constrained
- **Minimal Fragmentation**: Prefer smaller messages to reduce complexity

## Testing

### Manual Testing

1. Load app on two or more Flipper Zero devices
2. Navigate to BitChat app in Communication category
3. Devices should auto-discover via BLE advertising
4. Send test messages between devices
5. Verify encryption, delivery, and multi-hop relay

### Integration with BitChat

FlipBitChat should be compatible with the main BitChat iOS/macOS app's Bluetooth mesh protocol:

1. BitChat app can discover Flipper Zero devices
2. Messages sent from Flipper appear in BitChat
3. Messages from BitChat reach Flipper devices
4. End-to-end encryption works across platforms

## References

- [BitChat Whitepaper](../WHITEPAPER.md) - Protocol specification
- [BitChat README](../README.md) - Main app documentation
- [Flipper Zero Developer Docs](https://developer.flipper.net/flipperzero/doxygen/)
- [Flipper Firmware GitHub](https://github.com/flipperdevices/flipperzero-firmware)
- [ufbt Documentation](https://github.com/flipperdevices/flipperzero-ufbt)
- [Noise Protocol Framework](https://noiseprotocol.org/noise.html)
- [Enhanced Sub-GHz Chat](https://github.com/flipperdevices/flipperzero-firmware/tree/dev/applications/external/subghz_chat) - Reference Flipper messaging app

## Contributing

Contributions are welcome! Areas where help is needed:

- BLE GATT service implementation
- mbedtls crypto integration
- Memory optimization
- GUI polish and additional views
- Testing with multiple devices
- Documentation improvements

## License

This project inherits BitChat's public domain license. See [LICENSE](../LICENSE) for details.

## Contact

For questions or discussions:
- BitChat GitHub: https://github.com/lalomorales22/bitchat
- Flipper Zero Community: https://forum.flipper.net/
