#include "fragmenter.h"
#include "packet.h"
#include "../flip_bitchat.h"
#include <furi.h>
#include <string.h>

#define TAG "Fragmenter"
#define MAX_ASSEMBLIES 8

struct MessageFragmenter {
    FragmentAssembly assemblies[MAX_ASSEMBLIES];
    size_t assembly_count;
    FuriMutex* mutex;
};

MessageFragmenter* message_fragmenter_alloc(void) {
    MessageFragmenter* fragmenter = malloc(sizeof(MessageFragmenter));
    
    fragmenter->assembly_count = 0;
    fragmenter->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    
    FURI_LOG_I(TAG, "Message fragmenter allocated");
    
    return fragmenter;
}

void message_fragmenter_free(MessageFragmenter* fragmenter) {
    furi_assert(fragmenter);
    
    furi_mutex_acquire(fragmenter->mutex, FuriWaitForever);
    
    // Free all assemblies
    for(size_t i = 0; i < fragmenter->assembly_count; i++) {
        for(size_t j = 0; j < MAX_FRAGMENTS_PER_MESSAGE; j++) {
            if(fragmenter->assemblies[i].fragments[j]) {
                free(fragmenter->assemblies[i].fragments[j]);
            }
        }
    }
    
    furi_mutex_release(fragmenter->mutex);
    furi_mutex_free(fragmenter->mutex);
    
    free(fragmenter);
    
    FURI_LOG_I(TAG, "Message fragmenter freed");
}

size_t message_fragmenter_create_fragments(
    MessageFragmenter* fragmenter,
    const uint8_t* data,
    size_t data_len,
    const uint8_t* sender_id,
    const uint8_t* recipient_id,
    BitchatPacket** packets,
    size_t max_packets) {
    
    furi_assert(fragmenter);
    furi_assert(data);
    furi_assert(sender_id);
    furi_assert(packets);
    
    // Calculate number of fragments needed
    size_t num_fragments = (data_len + FRAGMENT_MAX_SIZE - 1) / FRAGMENT_MAX_SIZE;
    
    if(num_fragments > max_packets || num_fragments > MAX_FRAGMENTS_PER_MESSAGE) {
        FURI_LOG_E(TAG, "Too many fragments needed: %zu", num_fragments);
        return 0;
    }
    
    // Generate message ID
    uint8_t message_id[8];
    uint64_t timestamp = furi_get_tick();
    memcpy(message_id, &timestamp, 8);
    
    FURI_LOG_D(TAG, "Creating %zu fragments for %zu bytes", num_fragments, data_len);
    
    size_t offset = 0;
    for(size_t i = 0; i < num_fragments; i++) {
        size_t fragment_size = data_len - offset;
        if(fragment_size > FRAGMENT_MAX_SIZE) {
            fragment_size = FRAGMENT_MAX_SIZE;
        }
        
        // Create fragment payload: message_id (8) + fragment_index (1) + total_fragments (1) + data
        uint8_t fragment_payload[FRAGMENT_MAX_SIZE + 10];
        memcpy(fragment_payload, message_id, 8);
        fragment_payload[8] = (uint8_t)i;
        fragment_payload[9] = (uint8_t)num_fragments;
        memcpy(fragment_payload + 10, data + offset, fragment_size);
        
        // Determine packet type
        PacketType type;
        if(i == 0) {
            type = PacketTypeFragmentStart;
        } else if(i == num_fragments - 1) {
            type = PacketTypeFragmentEnd;
        } else {
            type = PacketTypeFragmentContinue;
        }
        
        // Create packet
        packets[i] = packet_create(
            type,
            BLE_MESH_TTL,
            sender_id,
            recipient_id,
            fragment_payload,
            10 + fragment_size);
        
        offset += fragment_size;
    }
    
    return num_fragments;
}

bool message_fragmenter_process_fragment(
    MessageFragmenter* fragmenter,
    const BitchatPacket* packet,
    uint8_t* assembled_data,
    size_t assembled_len,
    size_t* written) {
    
    furi_assert(fragmenter);
    furi_assert(packet);
    furi_assert(assembled_data);
    furi_assert(written);
    
    // Verify this is a fragment packet
    if(packet->header.type != PacketTypeFragmentStart &&
       packet->header.type != PacketTypeFragmentContinue &&
       packet->header.type != PacketTypeFragmentEnd) {
        return false;
    }
    
    if(packet->header.payload_len < 10) {
        FURI_LOG_E(TAG, "Fragment payload too small");
        return false;
    }
    
    // Parse fragment header
    uint8_t message_id[8];
    memcpy(message_id, packet->payload, 8);
    uint8_t fragment_index = packet->payload[8];
    uint8_t total_fragments = packet->payload[9];
    
    if(fragment_index >= total_fragments || total_fragments > MAX_FRAGMENTS_PER_MESSAGE) {
        FURI_LOG_E(TAG, "Invalid fragment indices: %d/%d", fragment_index, total_fragments);
        return false;
    }
    
    furi_mutex_acquire(fragmenter->mutex, FuriWaitForever);
    
    // Find or create assembly
    FragmentAssembly* assembly = NULL;
    for(size_t i = 0; i < fragmenter->assembly_count; i++) {
        if(memcmp(fragmenter->assemblies[i].message_id, message_id, 8) == 0) {
            assembly = &fragmenter->assemblies[i];
            break;
        }
    }
    
    if(!assembly) {
        // Create new assembly
        if(fragmenter->assembly_count >= MAX_ASSEMBLIES) {
            FURI_LOG_W(TAG, "Too many active assemblies");
            furi_mutex_release(fragmenter->mutex);
            return false;
        }
        
        assembly = &fragmenter->assemblies[fragmenter->assembly_count];
        fragmenter->assembly_count++;
        
        memcpy(assembly->message_id, message_id, 8);
        memset(assembly->fragments, 0, sizeof(assembly->fragments));
        memset(assembly->fragment_received, 0, sizeof(assembly->fragment_received));
        assembly->total_fragments = total_fragments;
        assembly->received_count = 0;
        assembly->first_fragment_time = furi_get_tick();
    }
    
    // Store fragment if not already received
    if(!assembly->fragment_received[fragment_index]) {
        size_t fragment_data_size = packet->header.payload_len - 10;
        assembly->fragments[fragment_index] = malloc(fragment_data_size);
        memcpy(assembly->fragments[fragment_index], packet->payload + 10, fragment_data_size);
        assembly->fragment_sizes[fragment_index] = fragment_data_size;
        assembly->fragment_received[fragment_index] = true;
        assembly->received_count++;
        
        FURI_LOG_D(TAG, "Fragment %d/%d received", fragment_index + 1, total_fragments);
    }
    
    // Check if all fragments received
    bool complete = (assembly->received_count == assembly->total_fragments);
    
    if(complete) {
        // Assemble message
        size_t offset = 0;
        for(size_t i = 0; i < assembly->total_fragments; i++) {
            if(offset + assembly->fragment_sizes[i] > assembled_len) {
                FURI_LOG_E(TAG, "Assembled message too large");
                complete = false;
                break;
            }
            
            memcpy(assembled_data + offset, assembly->fragments[i], assembly->fragment_sizes[i]);
            offset += assembly->fragment_sizes[i];
        }
        
        if(complete) {
            *written = offset;
            FURI_LOG_I(TAG, "Message assembled: %zu bytes", offset);
        }
        
        // Clean up assembly
        for(size_t i = 0; i < MAX_FRAGMENTS_PER_MESSAGE; i++) {
            if(assembly->fragments[i]) {
                free(assembly->fragments[i]);
                assembly->fragments[i] = NULL;
            }
        }
        
        // Remove from array
        for(size_t i = 0; i < fragmenter->assembly_count; i++) {
            if(&fragmenter->assemblies[i] == assembly) {
                for(size_t j = i; j < fragmenter->assembly_count - 1; j++) {
                    fragmenter->assemblies[j] = fragmenter->assemblies[j + 1];
                }
                fragmenter->assembly_count--;
                break;
            }
        }
    }
    
    furi_mutex_release(fragmenter->mutex);
    
    return complete;
}

void message_fragmenter_cleanup(MessageFragmenter* fragmenter) {
    furi_assert(fragmenter);
    
    furi_mutex_acquire(fragmenter->mutex, FuriWaitForever);
    
    uint32_t now = furi_get_tick();
    
    for(size_t i = 0; i < fragmenter->assembly_count; i++) {
        FragmentAssembly* assembly = &fragmenter->assemblies[i];
        
        if((now - assembly->first_fragment_time) > FRAGMENT_TIMEOUT_MS) {
            FURI_LOG_W(TAG, "Fragment assembly timed out");
            
            // Free fragments
            for(size_t j = 0; j < MAX_FRAGMENTS_PER_MESSAGE; j++) {
                if(assembly->fragments[j]) {
                    free(assembly->fragments[j]);
                    assembly->fragments[j] = NULL;
                }
            }
            
            // Remove from array
            for(size_t j = i; j < fragmenter->assembly_count - 1; j++) {
                fragmenter->assemblies[j] = fragmenter->assemblies[j + 1];
            }
            fragmenter->assembly_count--;
            i--;
        }
    }
    
    furi_mutex_release(fragmenter->mutex);
}
