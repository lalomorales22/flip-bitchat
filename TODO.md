# FlipBitChat TODO & Roadmap

## Phase 1: Core Functionality ✅ (Current)

### Project Setup
- [x] Create flip-bitchat directory structure
- [x] Set up application.fam manifest
- [x] Create main app entry point
- [x] Define project architecture
- [x] Write comprehensive README
- [x] Create development guide
- [x] Add .gitignore for build artifacts

### Code Scaffolding
- [x] BLE manager module skeleton
- [x] Crypto module interface
- [x] Protocol packet structures
- [x] Chat view GUI component
- [x] Main app lifecycle (alloc/free)

## Phase 2: BLE Implementation 🚧

### BLE Mesh Networking
- [ ] Implement BLE GATT service for BitChat
  - [ ] Define custom service UUID
  - [ ] Create TX characteristic (write)
  - [ ] Create RX characteristic (notify)
- [ ] Implement advertising
  - [ ] Start/stop advertising
  - [ ] Broadcast device name and peer ID
- [ ] Implement scanning
  - [ ] Discover nearby BitChat peers
  - [ ] Filter by service UUID
  - [ ] Populate peer list
- [ ] Connection management
  - [ ] Handle incoming connections
  - [ ] Maintain connection list
  - [ ] Detect disconnections
- [ ] Message I/O
  - [ ] Write to TX characteristic
  - [ ] Subscribe to RX notifications
  - [ ] Handle fragmentation (BLE MTU ~512 bytes)

## Phase 3: Cryptography 🔒

### mbedtls Integration
- [ ] Initialize mbedtls contexts
- [ ] Implement ChaCha20-Poly1305 encryption
  - [ ] Encrypt function
  - [ ] Decrypt function
  - [ ] AEAD authentication
- [ ] Implement Curve25519 key exchange
  - [ ] Generate keypair
  - [ ] Perform ECDH
  - [ ] Derive shared secret
- [ ] Implement SHA-256 hashing
  - [ ] Hash function
  - [ ] Fingerprint generation
- [ ] Key management
  - [ ] Store static keys securely
  - [ ] Generate ephemeral keys per session

### Noise Protocol
- [ ] Implement XX handshake pattern
  - [ ] Message 1: -> e
  - [ ] Message 2: <- e, ee, s, es
  - [ ] Message 3: -> s, se
- [ ] Session state management
  - [ ] Track handshake progress
  - [ ] Derive transport keys
  - [ ] Split into tx/rx ciphers
- [ ] Replay protection
  - [ ] Nonce tracking
  - [ ] Sliding window for out-of-order messages

## Phase 4: Protocol Implementation 📡

### Packet Handling
- [ ] Complete packet serialization
  - [ ] Binary packing
  - [ ] Handle optional fields (flags)
  - [ ] Padding for traffic analysis resistance
- [ ] Complete packet deserialization
  - [ ] Binary unpacking
  - [ ] Validation
  - [ ] Error handling
- [ ] Message framing
  - [ ] BitchatMessage serialization
  - [ ] Include metadata (timestamp, sender, etc.)
- [ ] Fragmentation
  - [ ] Split large messages
  - [ ] Reassembly logic
  - [ ] Timeout handling

### Mesh Routing
- [ ] Implement bloom filter for duplicate detection
  - [ ] Add seen packet IDs
  - [ ] Check for duplicates
  - [ ] Handle false positives
- [ ] TTL-based forwarding
  - [ ] Decrement TTL at each hop
  - [ ] Drop packets with TTL=0
- [ ] Broadcast vs. private routing
  - [ ] Forward broadcast to all peers
  - [ ] Route private to specific recipient
- [ ] Message retry service
  - [ ] Track outgoing messages
  - [ ] Retry on timeout
  - [ ] Delivery acknowledgments

## Phase 5: GUI & UX 🎨

### Views
- [ ] Chat view enhancements
  - [ ] Message wrapping for long text
  - [ ] Timestamp display
  - [ ] Sender identification
  - [ ] Scroll up/down
  - [ ] Auto-scroll to latest
- [ ] Message composition
  - [ ] TextInput integration
  - [ ] Send button
  - [ ] Clear/cancel
- [ ] Peer list view
  - [ ] Display discovered peers
  - [ ] RSSI signal strength
  - [ ] Last seen timestamp
  - [ ] Select peer for DM
- [ ] Settings view
  - [ ] Edit username/nickname
  - [ ] View own fingerprint
  - [ ] Clear message history
  - [ ] About/version info

### Scenes
- [ ] Scene manager setup
  - [ ] Chat scene (main)
  - [ ] Peer list scene
  - [ ] Settings scene
  - [ ] Text input scene
- [ ] Scene transitions
  - [ ] Navigation flow
  - [ ] Back button handling

### Icon
- [ ] Design app icon (10x10 monochrome)
- [ ] Create icon asset file
- [ ] Update application.fam reference

## Phase 6: Storage & Persistence 💾

### Message History
- [ ] Store recent messages (limit: 32-64 messages)
- [ ] Load on app start
- [ ] Clear history function
- [ ] Automatic pruning (FIFO)

### Settings Storage
- [ ] Save username/nickname
- [ ] Save static keypair (if not ephemeral)
- [ ] Save blocked peer list
- [ ] Use Flipper's storage API

## Phase 7: Optimization ⚡

### Memory
- [ ] Profile heap usage
- [ ] Minimize allocations
- [ ] Pool allocators for packets
- [ ] Reduce bloom filter size if needed
- [ ] Optimize message history storage

### Battery
- [ ] Adaptive scanning intervals
- [ ] Duty cycling for advertising
- [ ] Sleep mode when idle
- [ ] Reduce BLE transmit power if needed

### Performance
- [ ] Optimize packet serialization
- [ ] Fast path for common operations
- [ ] Reduce cryptographic overhead (session caching)

## Phase 8: Testing & Validation ✔️

### Unit Tests
- [ ] Test packet serialization/deserialization
- [ ] Test crypto primitives
- [ ] Test bloom filter
- [ ] Test TTL decrement

### Integration Tests
- [ ] Two-device messaging
- [ ] Multi-hop relay (3+ devices)
- [ ] Handshake under lossy conditions
- [ ] Concurrent connections
- [ ] Message ordering

### Compatibility Tests
- [ ] BitChat iOS/macOS <-> Flipper Zero
- [ ] Verify protocol version compatibility
- [ ] Cross-platform encryption/decryption

## Phase 9: Documentation 📚

- [ ] API documentation (Doxygen)
- [ ] User manual
  - [ ] Installation instructions
  - [ ] Basic usage tutorial
  - [ ] Troubleshooting
- [ ] Architecture diagrams
- [ ] Protocol compliance notes
- [ ] Security considerations

## Phase 10: Release 🚀

### Pre-release
- [ ] Code review
- [ ] Security audit (crypto implementation)
- [ ] Performance benchmarks
- [ ] Final testing with real devices

### Release Checklist
- [ ] Version tagging (semver)
- [ ] Changelog
- [ ] Release notes
- [ ] Publish .fap to Flipper App Store
- [ ] Announce in BitChat community

## Future Enhancements 🔮

### Advanced Features
- [ ] Group chat support
- [ ] File transfer (small files)
- [ ] Voice messages (audio codec)
- [ ] Location sharing (GPS coords)
- [ ] Emergency broadcast mode

### Protocol Extensions
- [ ] Compressed messages (LZ4)
- [ ] Forward error correction
- [ ] Adaptive routing (prefer strong RSSI paths)
- [ ] Network topology visualization

### Hardware Integration
- [ ] WiFi devboard support (wider range)
- [ ] GPS module integration
- [ ] External antenna support

### Cross-platform
- [ ] Android companion app
- [ ] Desktop Linux/Windows client
- [ ] Web-based mesh visualizer

## Community Contributions Welcome! 🤝

We welcome contributions in any of these areas. See [DEVELOPMENT.md](DEVELOPMENT.md) for details on setting up your dev environment.

Priority areas:
1. BLE GATT implementation (Phase 2)
2. mbedtls crypto integration (Phase 3)
3. GUI polish (Phase 5)
4. Testing (Phase 8)

---

Last updated: 2025-12-06
