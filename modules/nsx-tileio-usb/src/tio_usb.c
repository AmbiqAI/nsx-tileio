#include "tio_usb.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "nsx_core.h"
#include "nsx_mem.h"
#include "nsx_usb.h"

#define TIO_USB_START_IDX 0u
#define TIO_USB_START_VAL 0x55u
#define TIO_USB_SLOT_IDX 1u
#define TIO_USB_TYPE_IDX 2u
#define TIO_USB_DLEN_IDX 3u
#define TIO_USB_DLEN_LEN 2u
#define TIO_USB_DATA_IDX 5u
#define TIO_USB_DATA_LEN 248u
#define TIO_USB_CRC_IDX 253u
#define TIO_USB_CRC_LEN 2u
#define TIO_USB_STOP_IDX 255u
#define TIO_USB_STOP_VAL 0xAAu
#define TIO_USB_UIO_BUF_LEN 8u

#define TIO_USB_RX_BUFSIZE 4096u
#define TIO_USB_TX_BUFSIZE 4096u
#define TIO_USB_RING_BUFSIZE 4096u
#define TIO_USB_VENDOR_READ_CHUNK 256u

/*
 * NOTE on host->device write framing: real hosts (the production TileIO
 * web dashboard, api/usb.ts's setUioState()) send each logical write as a
 * single raw WebUSB transferOut() of the full packed TileIO packet, with no
 * application-level chunking or per-transfer header -- WebUSB automatically
 * splits it into as many wMaxPacketSize (64-byte) USB transactions as
 * needed, exactly mirroring how the device->host read direction already
 * works (tud_vendor_write() + tud_vendor_read()/transferIn() reassemble
 * arbitrary-length transfers transparently, no headers required in either
 * direction).
 *
 * An earlier version of the web app instead split writes into 62-byte
 * payloads with a 2-byte "NS frame header" prepended to every individual
 * 64-byte transfer (a convention inherited from an older neuralSPOT sample
 * that multiplexed two different message types over one endpoint via that
 * header). TileIO's packets are already self-delimited (start/stop markers
 * + CRC16 + fixed length) and only ever carry one kind of data, so that
 * header was pure legacy overhead: 25% larger on the wire (5x 64-byte
 * transfers to send one 256-byte packet) and 5 separate transferOut() calls
 * instead of 1 for a single logical write -- and, worse, this firmware
 * never stripped it, so real web-app UIO writes never reconstructed into a
 * valid packet at all (every 62 bytes of real data got 2 extra header
 * bytes spliced in, destroying the START/STOP framing alignment). Both
 * sides have been fixed together: the web app now sends one unframed
 * write, and this handler expects (only) a plain, unframed byte stream.
 */

typedef struct {
    uint8_t *buffer;
    uint32_t size;
    uint32_t head;
    uint32_t tail;
} tio_usb_ring_t;

static NSX_MEM_SRAM_BSS uint8_t g_tio_usb_rx_dma[TIO_USB_RX_BUFSIZE];
static NSX_MEM_SRAM_BSS uint8_t g_tio_usb_tx_dma[TIO_USB_TX_BUFSIZE];
static uint8_t g_tio_usb_ring_data[TIO_USB_RING_BUFSIZE];
static uint8_t g_tio_usb_read_chunk[TIO_USB_VENDOR_READ_CHUNK];

static tio_usb_ring_t g_tio_usb_ring = {
    .buffer = g_tio_usb_ring_data,
    .size = TIO_USB_RING_BUFSIZE,
    .head = 0u,
    .tail = 0u,
};

static tio_usb_context_t *g_tio_usb_ctx = NULL;
static nsx_usb_device_desc_t g_tio_usb_desc = {0};
static nsx_usb_config_t g_tio_usb_cfg = {
    .tx_buffer = g_tio_usb_tx_dma,
    .tx_buffer_len = sizeof(g_tio_usb_tx_dma),
    .rx_buffer = g_tio_usb_rx_dma,
    .rx_buffer_len = sizeof(g_tio_usb_rx_dma),
    .poll_interval_us = NSX_USB_DEFAULT_POLL_US,
    .timeout_ms = NSX_USB_DEFAULT_TIMEOUT_MS,
    .rx_cb = NULL,
    .vendor_rx_cb = NULL,
    .device_desc = &g_tio_usb_desc,
    .user_ctx = NULL,
    ._initialized = 0u,
    ._rx_ready = 0u,
    ._vendor_rx_ready = 0u,
    ._vendor_connected = 0u,
};

static uint32_t tio_usb_ring_len(const tio_usb_ring_t *ring) {
    if (ring->head >= ring->tail) {
        return ring->head - ring->tail;
    }
    return ring->size - ring->tail + ring->head;
}

static uint32_t tio_usb_ring_space(const tio_usb_ring_t *ring) {
    return ring->size - tio_usb_ring_len(ring);
}

static void tio_usb_ring_flush(tio_usb_ring_t *ring) {
    ring->head = 0u;
    ring->tail = 0u;
}

static uint32_t tio_usb_ring_seek(tio_usb_ring_t *ring, uint32_t len) {
    uint32_t available = tio_usb_ring_len(ring);
    uint32_t amount = (len < available) ? len : available;
    ring->tail = (ring->tail + amount) % ring->size;
    return amount;
}

static uint32_t tio_usb_ring_push(
    tio_usb_ring_t *ring, const uint8_t *data, uint32_t len) {
    uint32_t pushed = 0u;

    while (pushed < len && tio_usb_ring_space(ring) > 0u) {
        ring->buffer[ring->head] = data[pushed];
        ring->head = (ring->head + 1u) % ring->size;
        pushed++;
    }

    return pushed;
}

static uint32_t tio_usb_ring_peek(
    const tio_usb_ring_t *ring, uint8_t *data, uint32_t len) {
    uint32_t available = tio_usb_ring_len(ring);
    uint32_t amount = (len < available) ? len : available;
    uint32_t tail = ring->tail;

    for (uint32_t i = 0u; i < amount; ++i) {
        data[i] = ring->buffer[tail];
        tail = (tail + 1u) % ring->size;
    }

    return amount;
}

static uint16_t tio_usb_compute_crc16(const uint8_t *data, uint32_t length) {
    uint32_t crc = 0xEF4Au;

    for (uint32_t j = 0u; j < length; ++j) {
        crc ^= ((uint32_t)data[j]) << 8;
        for (uint32_t i = 0u; i < 8u; ++i) {
            uint32_t temp = crc << 1;
            if ((crc & 0x8000u) != 0u) {
                temp ^= 0x1021u;
            }
            crc = temp;
        }
    }

    return (uint16_t)crc;
}

static uint32_t tio_usb_validate_packet(
    const uint8_t *packet, uint32_t length) {
    if (packet == NULL || length != TIO_USB_PACKET_LEN) {
        return NSX_STATUS_INVALID_CONFIG;
    }

    if (packet[TIO_USB_START_IDX] != TIO_USB_START_VAL ||
        packet[TIO_USB_STOP_IDX] != TIO_USB_STOP_VAL) {
        return NSX_STATUS_FAILURE;
    }

    uint8_t slot_type = packet[TIO_USB_TYPE_IDX];
    uint16_t data_len =
        (uint16_t)packet[TIO_USB_DLEN_IDX] |
        ((uint16_t)packet[TIO_USB_DLEN_IDX + 1u] << 8);
    if (data_len > TIO_USB_DATA_LEN) {
        return NSX_STATUS_FAILURE;
    }
    if (slot_type == 2u && data_len != TIO_USB_UIO_BUF_LEN) {
        return NSX_STATUS_FAILURE;
    }
    if (slot_type > 2u) {
        return NSX_STATUS_FAILURE;
    }

    uint16_t packet_crc =
        (uint16_t)packet[TIO_USB_CRC_IDX] |
        ((uint16_t)packet[TIO_USB_CRC_IDX + 1u] << 8);
    uint16_t computed_crc = tio_usb_compute_crc16(
        packet + TIO_USB_DLEN_IDX, data_len + TIO_USB_DLEN_LEN);
    if (packet_crc != computed_crc) {
        return NSX_STATUS_FAILURE;
    }

    return NSX_STATUS_SUCCESS;
}

static void tio_usb_dispatch_packet(
    tio_usb_context_t *ctx, const uint8_t *packet) {
    uint8_t slot = packet[TIO_USB_SLOT_IDX];
    uint8_t slot_type = packet[TIO_USB_TYPE_IDX];
    uint16_t data_len =
        (uint16_t)packet[TIO_USB_DLEN_IDX] |
        ((uint16_t)packet[TIO_USB_DLEN_IDX + 1u] << 8);
    const uint8_t *data = packet + TIO_USB_DATA_IDX;

    if (slot_type <= 1u) {
        if (ctx->slot_update_cb != NULL) {
            ctx->slot_update_cb(slot, slot_type, data, data_len);
        }
        return;
    }

    if (slot_type == 2u && ctx->uio_update_cb != NULL) {
        ctx->uio_update_cb(data, data_len);
    }
}

static void tio_usb_process_rx_ring(void) {
    uint8_t packet[TIO_USB_PACKET_LEN];

    if (g_tio_usb_ctx == NULL) {
        tio_usb_ring_flush(&g_tio_usb_ring);
        return;
    }

    while (tio_usb_ring_len(&g_tio_usb_ring) >= TIO_USB_PACKET_LEN) {
        if (tio_usb_ring_peek(&g_tio_usb_ring, packet, sizeof(packet)) !=
            TIO_USB_PACKET_LEN) {
            break;
        }

        if (tio_usb_validate_packet(packet, sizeof(packet)) !=
            NSX_STATUS_SUCCESS) {
            tio_usb_ring_seek(&g_tio_usb_ring, 1u);
            continue;
        }

        tio_usb_dispatch_packet(g_tio_usb_ctx, packet);
        tio_usb_ring_seek(&g_tio_usb_ring, TIO_USB_PACKET_LEN);
    }
}

static void tio_usb_vendor_rx_handler(nsx_usb_config_t *cfg) {
    uint32_t bytes_read = 0u;

    if (cfg == NULL) {
        return;
    }

    /* PyUSB/libusb hosts can push valid bulk OUT traffic without ever issuing
     * the WebUSB SET_CONTROL_LINE_STATE request. Once we observe vendor RX,
     * treat the transport as connected so TileIO can send responses/telemetry
     * back on the same session. */
    cfg->_vendor_connected = 1u;

    do {
        bytes_read = 0u;
        (void)nsx_usb_vendor_read_nb(
            cfg, g_tio_usb_read_chunk, sizeof(g_tio_usb_read_chunk),
            &bytes_read);
        if (bytes_read == 0u) {
            break;
        }

        uint32_t pushed = tio_usb_ring_push(
            &g_tio_usb_ring, g_tio_usb_read_chunk, bytes_read);
        if (pushed != bytes_read) {
            tio_usb_ring_flush(&g_tio_usb_ring);
        }
        tio_usb_process_rx_ring();
    } while (nsx_usb_vendor_data_available(cfg));
}

uint32_t tio_usb_init(tio_usb_context_t *ctx) {
    if (ctx == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    g_tio_usb_ctx = ctx;
    tio_usb_ring_flush(&g_tio_usb_ring);

    memset(&g_tio_usb_desc, 0, sizeof(g_tio_usb_desc));
    g_tio_usb_desc.vid = (ctx->vid != 0u) ? ctx->vid : TIO_USB_VENDOR_ID;
    g_tio_usb_desc.pid = (ctx->pid != 0u) ? ctx->pid : TIO_USB_PRODUCT_ID;
    g_tio_usb_desc.manufacturer = ctx->manufacturer;
    g_tio_usb_desc.product = ctx->product;
    g_tio_usb_desc.serial = ctx->serial;
    g_tio_usb_desc.cdc_interface = ctx->cdc_interface;
    g_tio_usb_desc.vendor_interface = ctx->vendor_interface;
    g_tio_usb_desc.webusb_url = ctx->webusb_url;

    g_tio_usb_cfg.poll_interval_us =
        (ctx->poll_interval_us != 0u) ? ctx->poll_interval_us
                                      : NSX_USB_DEFAULT_POLL_US;
    g_tio_usb_cfg.timeout_ms =
        (ctx->timeout_ms != 0u) ? ctx->timeout_ms : NSX_USB_DEFAULT_TIMEOUT_MS;
    g_tio_usb_cfg.vendor_rx_cb = tio_usb_vendor_rx_handler;
    g_tio_usb_cfg.user_ctx = ctx;

    return nsx_usb_init(&g_tio_usb_cfg);
}

uint32_t tio_usb_tx_available(void) {
    if (!g_tio_usb_cfg._initialized) {
        return 0u;
    }
    return nsx_usb_vendor_connected(&g_tio_usb_cfg) ? 1u : 0u;
}

uint32_t tio_usb_pack_slot_data(
    uint8_t slot, uint8_t slot_type, const uint8_t *data, uint32_t length,
    uint8_t *packet) {
    if (packet == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }
    if (slot_type > 2u) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    if (length > TIO_USB_DATA_LEN) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    if (slot_type == 2u && length != TIO_USB_UIO_BUF_LEN) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    if (length > 0u && data == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }

    memset(packet, 0, TIO_USB_PACKET_LEN);
    packet[TIO_USB_START_IDX] = TIO_USB_START_VAL;
    packet[TIO_USB_SLOT_IDX] = slot;
    packet[TIO_USB_TYPE_IDX] = slot_type;
    packet[TIO_USB_DLEN_IDX] = (uint8_t)(length & 0xFFu);
    packet[TIO_USB_DLEN_IDX + 1u] = (uint8_t)((length >> 8) & 0xFFu);
    if (length > 0u) {
        memcpy(packet + TIO_USB_DATA_IDX, data, length);
    }

    uint16_t crc = tio_usb_compute_crc16(
        packet + TIO_USB_DLEN_IDX, length + TIO_USB_DLEN_LEN);
    packet[TIO_USB_CRC_IDX] = (uint8_t)(crc & 0xFFu);
    packet[TIO_USB_CRC_IDX + 1u] = (uint8_t)((crc >> 8) & 0xFFu);
    packet[TIO_USB_STOP_IDX] = TIO_USB_STOP_VAL;
    return NSX_STATUS_SUCCESS;
}

uint32_t tio_usb_send_slot_packet(uint8_t *buffer, uint32_t length) {
    uint32_t bytes_sent = 0u;

    if (!g_tio_usb_cfg._initialized) {
        return NSX_STATUS_INVALID_HANDLE;
    }
    if (buffer == NULL) {
        return NSX_STATUS_INVALID_HANDLE;
    }
    if (length != TIO_USB_PACKET_LEN) {
        return NSX_STATUS_INVALID_CONFIG;
    }
    if (!nsx_usb_vendor_connected(&g_tio_usb_cfg)) {
        return NSX_USB_STATUS_NOT_CONNECTED;
    }

    uint32_t status = nsx_usb_vendor_send(
        &g_tio_usb_cfg, buffer, length, &bytes_sent);
    if (status != NSX_STATUS_SUCCESS) {
        return status;
    }
    if (bytes_sent != length) {
        return NSX_USB_STATUS_PARTIAL;
    }
    return NSX_STATUS_SUCCESS;
}

uint32_t tio_usb_send_slot_data(
    uint8_t slot, uint8_t slot_type, const uint8_t *data, uint32_t length) {
    uint8_t packet[TIO_USB_PACKET_LEN];
    uint32_t status = tio_usb_pack_slot_data(
        slot, slot_type, data, length, packet);
    if (status != NSX_STATUS_SUCCESS) {
        return status;
    }
    return tio_usb_send_slot_packet(packet, sizeof(packet));
}

uint32_t tio_usb_send_uio_state(const uint8_t *data, uint32_t length) {
    return tio_usb_send_slot_data(0u, 2u, data, length);
}
