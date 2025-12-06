# FlipBitChat Development Guide

This guide provides detailed information for developers working on FlipBitChat.

## Getting Started

### Development Environment Setup

1. **Install Prerequisites**
   ```bash
   # Python 3.8+
   python3 --version
   
   # Install ufbt (Flipper Build Tool)
   pip3 install --upgrade ufbt
   ```

2. **Update Flipper SDK**
   ```bash
   ufbt update --channel=release
   # Or for development firmware:
   # ufbt update --channel=dev
   ```

3. **Clone Repository**
   ```bash
   git clone https://github.com/lalomorales22/bitchat.git
   cd bitchat/flip-bitchat
   ```

4. **Build**
   ```bash
   ufbt
   ```

### Flipper Zero Connection

#### Via USB
```bash
# Connect Flipper via USB cable
# Check connection
ufbt cli

# Deploy and launch app
ufbt launch
```

#### Via WiFi (if WiFi devboard attached)
```bash
# Set Flipper IP
export FLIPPER_IP=192.168.x.x
ufbt launch_remote
```

## Code Organization

### Module Responsibilities

#### `flip_bitchat.c` - Main Application
- App lifecycle (alloc/free)
- ViewDispatcher initialization
- SceneManager setup
- Top-level event handling

#### `ble/` - Bluetooth Mesh Networking
- `ble_manager.h/.c`: BLE abstraction layer
  - Advertising and scanning
  - Peer discovery and tracking
  - GATT characteristic I/O
  - Message send/receive callbacks

#### `crypto/` - Cryptographic Operations
- `crypto.h/.c`: Crypto primitives using mbedtls
  - ChaCha20-Poly1305 AEAD encryption
  - Curve25519 key exchange (ECDH)
  - SHA-256 hashing
  - Random number generation

#### `gui/` - User Interface
- `views.h`: View API declarations
- `chat_view.c`: Main chat display
  - Canvas drawing for 128x64 LCD
  - Message list rendering
  - Scroll handling
- Future: `peer_list_view.c`, `settings_view.c`

#### `protocol/` - BitChat Protocol
- `packet.h/.c`: Packet structure and serialization
  - Binary packet framing
  - Message serialization/deserialization
  - TTL and routing logic
  - Fragmentation handling

## Development Workflow

### Iterative Development

1. **Make Changes**
   ```bash
   # Edit source files
   vim flip_bitchat.c
   ```

2. **Build**
   ```bash
   ufbt
   # Watch for compilation errors/warnings
   ```

3. **Flash & Test**
   ```bash
   ufbt launch
   # App installs and runs on Flipper
   ```

4. **View Logs**
   ```bash
   ufbt cli
   # Serial console shows FURI_LOG_* output
   ```

### Debugging

#### Using GDB
```bash
# Launch with debugger attached
ufbt debug

# In GDB:
(gdb) break flip_bitchat_app
(gdb) continue
(gdb) backtrace
```

#### Log Levels
Use Furi's logging macros:
```c
#define TAG "MyModule"

FURI_LOG_E(TAG, "Error: %s", error_msg);    // Error
FURI_LOG_W(TAG, "Warning: %d", value);       // Warning
FURI_LOG_I(TAG, "Info: initialized");        // Info
FURI_LOG_D(TAG, "Debug: state=%d", state);   // Debug
FURI_LOG_T(TAG, "Trace: entering function"); // Trace
```

View logs via serial console:
```bash
ufbt cli
```

### Memory Debugging

Monitor memory usage with Furi's built-in tools:
```c
// Check free heap
size_t free_heap = memmgr_get_free_heap();
FURI_LOG_I(TAG, "Free heap: %zu bytes", free_heap);

// Allocate with tracking
void* ptr = malloc(size);
furi_check(ptr != NULL);  // Asserts on OOM
```

## BitChat Protocol Implementation

### Implementing Noise Protocol Handshake

The `XX` pattern handshake requires three messages:

```c
// Initiator side
NoiseState* initiator = noise_state_init(NOISE_XX, true);
noise_state_set_keypair(initiator, static_private, static_public);

// Message 1: -> e
uint8_t msg1[64];
size_t msg1_len = noise_write_message(initiator, NULL, 0, msg1, sizeof(msg1));
ble_manager_send(ble, peer_id, msg1, msg1_len);

// Receive Message 2: <- e, ee, s, es
uint8_t msg2[128];
noise_read_message(initiator, msg2, msg2_len, decrypted, &decrypted_len);

// Message 3: -> s, se
uint8_t msg3[128];
size_t msg3_len = noise_write_message(initiator, NULL, 0, msg3, sizeof(msg3));
ble_manager_send(ble, peer_id, msg3, msg3_len);

// Handshake complete - split into tx/rx ciphers
NoiseCipherState* tx = noise_state_get_tx_cipher(initiator);
NoiseCipherState* rx = noise_state_get_rx_cipher(initiator);
```

### BLE GATT Service Design

Define custom BitChat GATT service:

```c
// Service UUID: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E (Nordic UART)
// Or create custom UUID for BitChat

// TX Characteristic: Write messages
#define BLE_TX_CHAR_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

// RX Characteristic: Receive messages (notifications)
#define BLE_RX_CHAR_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// In ble_manager.c:
static void ble_rx_callback(const uint8_t* data, size_t len) {
    // Deserialize packet
    BitchatPacket* packet = packet_deserialize(data, len);
    
    // Invoke message callback
    if(manager->message_callback) {
        manager->message_callback(
            manager->callback_context,
            packet->payload,
            packet->header.payload_len,
            packet->sender_id);
    }
    
    packet_free(packet);
}
```

### Message Routing Logic

Implement mesh forwarding with TTL and bloom filter:

```c
typedef struct {
    uint8_t seen_packet_ids[256][8];  // Bloom filter
    size_t filter_index;
} MeshRouter;

bool router_should_forward(MeshRouter* router, BitchatPacket* packet) {
    // Check if packet seen before (bloom filter)
    for(size_t i = 0; i < 256; i++) {
        if(memcmp(router->seen_packet_ids[i], packet->sender_id, 8) == 0) {
            return false;  // Already seen
        }
    }
    
    // Add to bloom filter
    memcpy(
        router->seen_packet_ids[router->filter_index],
        packet->sender_id,
        8);
    router->filter_index = (router->filter_index + 1) % 256;
    
    // Check TTL
    if(packet->header.ttl == 0) {
        return false;  // Do not forward
    }
    
    return true;
}

void router_forward_packet(MeshRouter* router, BitchatPacket* packet) {
    // Decrement TTL
    packet->header.ttl--;
    
    // Serialize and broadcast
    uint8_t buffer[512];
    size_t len = packet_serialize(packet, buffer, sizeof(buffer));
    ble_manager_broadcast(ble, buffer, len);
}
```

## GUI Development

### Canvas Drawing

Flipper uses a 128x64 monochrome display:

```c
static void my_view_draw_callback(Canvas* canvas, void* model) {
    canvas_clear(canvas);
    
    // Set font
    canvas_set_font(canvas, FontPrimary);    // Large font
    canvas_set_font(canvas, FontSecondary);  // Small font
    
    // Draw text
    canvas_draw_str(canvas, x, y, "Hello");
    
    // Draw aligned text
    canvas_draw_str_aligned(
        canvas,
        x, y,
        AlignCenter,    // Horizontal: AlignLeft, AlignCenter, AlignRight
        AlignTop,       // Vertical: AlignTop, AlignCenter, AlignBottom
        "Centered Text");
    
    // Draw shapes
    canvas_draw_line(canvas, x1, y1, x2, y2);
    canvas_draw_box(canvas, x, y, width, height);
    canvas_draw_frame(canvas, x, y, width, height);
    canvas_draw_circle(canvas, x, y, radius);
}
```

### Input Handling

Handle Flipper's 5-way navigation:

```c
static bool my_view_input_callback(InputEvent* event, void* context) {
    if(event->type == InputTypeShort) {
        switch(event->key) {
            case InputKeyUp:
                // Navigate up
                return true;
            case InputKeyDown:
                // Navigate down
                return true;
            case InputKeyLeft:
                // Navigate left
                return true;
            case InputKeyRight:
                // Navigate right
                return true;
            case InputKeyOk:
                // Select/confirm
                return true;
            case InputKeyBack:
                // Go back (return false to let ViewDispatcher handle)
                return false;
        }
    } else if(event->type == InputTypeLong) {
        // Handle long press
    }
    
    return false;
}
```

### Text Input

Use Flipper's built-in TextInput view:

```c
// In app context
TextInput* text_input = text_input_alloc();

// Set header
text_input_set_header_text(text_input, "Enter message:");

// Set result callback
text_input_set_result_callback(
    text_input,
    text_input_callback,
    app,          // Context
    message_buf,  // Buffer to fill
    MAX_MSG_LEN,  // Buffer size
    true);        // Clear default text

// Add to ViewDispatcher
view_dispatcher_add_view(
    app->view_dispatcher,
    ViewIdTextInput,
    text_input_get_view(text_input));
```

## Testing

### Unit Testing

While Flipper doesn't have built-in unit testing, you can:

1. Test protocol code on desktop:
   ```bash
   gcc -o test_packet packet.c packet_test.c
   ./test_packet
   ```

2. Use assertions:
   ```c
   furi_assert(pointer != NULL);
   furi_check(condition);
   ```

### Integration Testing

Test with multiple Flipper devices:

1. Load app on 2+ Flippers
2. Verify peer discovery
3. Send messages between devices
4. Test multi-hop relay (device A -> B -> C)
5. Verify encryption (sniff BLE traffic, ensure encrypted)

### Performance Testing

Monitor resources:

```c
// Memory
size_t free = memmgr_get_free_heap();
FURI_LOG_I(TAG, "Free heap: %zu / 256KB", free);

// Timing
uint32_t start = furi_get_tick();
do_operation();
uint32_t elapsed = furi_get_tick() - start;
FURI_LOG_I(TAG, "Operation took %lu ms", elapsed);
```

## Best Practices

### Memory Management

- Prefer stack allocation for small, short-lived objects
- Always free malloc'd memory
- Use `furi_check(ptr != NULL)` after malloc
- Minimize allocations in hot paths (message processing)

### Concurrency

- Use `FuriMutex` to protect shared state:
  ```c
  FuriMutex* mutex = furi_mutex_alloc(FuriMutexTypeNormal);
  furi_mutex_acquire(mutex, FuriWaitForever);
  // Critical section
  furi_mutex_release(mutex);
  ```

- Use `FuriMessageQueue` for async events:
  ```c
  FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(Event));
  Event event = {.type = EventTypeMessage};
  furi_message_queue_put(queue, &event, FuriWaitForever);
  ```

### Error Handling

- Check return values from all APIs
- Use FURI_LOG_E for errors
- Gracefully degrade on failure:
  ```c
  if(!crypto_encrypt(...)) {
      FURI_LOG_E(TAG, "Encryption failed");
      // Fallback: don't send message
      return false;
  }
  ```

### Code Style

Follow Flipper conventions:
- 4-space indentation (no tabs)
- Snake case: `my_function_name`
- Pointer asterisk on variable: `void* ptr`
- Brace on same line: `if(cond) {`
- Avoid global variables (use app context)

## Resources

- [Flipper Apps Examples](https://github.com/flipperdevices/flipperzero-firmware/tree/dev/applications/examples)
- [Flipper API Documentation](https://developer.flipper.net/flipperzero/doxygen/)
- [mbedtls Documentation](https://mbed-tls.readthedocs.io/)
- [BitChat Whitepaper](../WHITEPAPER.md)

## Troubleshooting

### App Crashes on Launch
- Check logs via `ufbt cli`
- Verify all allocations have corresponding frees
- Check for NULL pointer dereferences

### BLE Not Working
- Ensure Bluetooth is enabled on Flipper
- Check if another app is using BLE
- Verify GATT service UUIDs are correct

### Build Errors
- Update ufbt: `pip3 install --upgrade ufbt`
- Update SDK: `ufbt update`
- Clean build: `ufbt clean && ufbt`

### Out of Memory
- Reduce message history size
- Free unused resources
- Check for memory leaks with logs

## Next Steps

1. Implement actual BLE GATT service
2. Integrate mbedtls for real encryption
3. Complete Noise handshake
4. Add message composition UI
5. Test with multiple devices
6. Optimize for battery life
7. Add persistent settings storage

Happy coding! 🐬
