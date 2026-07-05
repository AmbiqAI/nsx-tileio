/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2026, Ambiq */
#include "tio_ble.h"

#include "nsx_core.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define TIO_BLE_SLOT_TYPE_SIGNAL 0u
#define TIO_BLE_SLOT_TYPE_METRIC 1u
#define TIO_BLE_SLOT_TYPE_UIO    2u

#define TIO_SLOT_SVC_UUID       "eecb7db88b2d402cb995825538b49328"
#define TIO_SLOT0_SIG_CHAR_UUID "5bca2754ac7e4a27a1270f328791057a"
#define TIO_SLOT1_SIG_CHAR_UUID "45415793a0e94740bca4ce90bd61839f"
#define TIO_SLOT2_SIG_CHAR_UUID "dd19792c63f1420f920cc58bada8efb9"
#define TIO_SLOT3_SIG_CHAR_UUID "f1f691580bd64cab90a8528baf74cc74"
#define TIO_SLOT0_MET_CHAR_UUID "44a3a7b8d7c849329a10d99dd63775ae"
#define TIO_SLOT1_MET_CHAR_UUID "e64fa683462848c5bede824aaa7c3f5b"
#define TIO_SLOT2_MET_CHAR_UUID "b9d28f5365f04392afbcc602f9dc3c8b"
#define TIO_SLOT3_MET_CHAR_UUID "917c9eb43dbc4cb3bba2ec4e288083f4"
#define TIO_UIO_CHAR_UUID       "b9488d48069b47f794f0387f7fbfd1fa"

typedef struct {
    bool initialized;
    tio_ble_context_t *ctx;
    ns_ble_service_t service;
    ns_ble_characteristic_t slot_sig[TIO_BLE_SLOT_COUNT];
    ns_ble_characteristic_t slot_met[TIO_BLE_SLOT_COUNT];
    ns_ble_characteristic_t uio;
    uint8_t slot_sig_value[TIO_BLE_SLOT_COUNT][TIO_BLE_SLOT_SIG_BUF_LEN];
    uint8_t slot_met_value[TIO_BLE_SLOT_COUNT][TIO_BLE_SLOT_MET_BUF_LEN];
    uint8_t uio_value[TIO_BLE_UIO_BUF_LEN];
} tio_ble_runtime_t;

static tio_ble_runtime_t g_tio_ble = {0};

static const char *const g_tio_ble_slot_sig_uuids[TIO_BLE_SLOT_COUNT] = {
    TIO_SLOT0_SIG_CHAR_UUID,
    TIO_SLOT1_SIG_CHAR_UUID,
    TIO_SLOT2_SIG_CHAR_UUID,
    TIO_SLOT3_SIG_CHAR_UUID,
};

static const char *const g_tio_ble_slot_met_uuids[TIO_BLE_SLOT_COUNT] = {
    TIO_SLOT0_MET_CHAR_UUID,
    TIO_SLOT1_MET_CHAR_UUID,
    TIO_SLOT2_MET_CHAR_UUID,
    TIO_SLOT3_MET_CHAR_UUID,
};

static int tio_ble_read_handler(
    ns_ble_service_t *service, ns_ble_characteristic_t *characteristic, void *dest) {
    (void)service;
    if (dest == NULL || characteristic == NULL || characteristic->applicationValue == NULL) {
        return NS_STATUS_FAILURE;
    }
    memcpy(dest, characteristic->applicationValue, characteristic->valueLen);
    return NS_STATUS_SUCCESS;
}

static int tio_ble_notify_handler(ns_ble_service_t *service, ns_ble_characteristic_t *characteristic) {
    (void)service;
    (void)characteristic;
    return NS_STATUS_SUCCESS;
}

static int tio_ble_uio_write_handler(
    ns_ble_service_t *service, ns_ble_characteristic_t *characteristic, void *src) {
    (void)service;
    if (characteristic == NULL || characteristic->applicationValue == NULL || src == NULL) {
        return NS_STATUS_FAILURE;
    }
    if (characteristic->valueLen != TIO_BLE_UIO_BUF_LEN) {
        return NS_STATUS_FAILURE;
    }

    memcpy(characteristic->applicationValue, src, characteristic->valueLen);
    if (g_tio_ble.ctx != NULL && g_tio_ble.ctx->uio_update_cb != NULL) {
        g_tio_ble.ctx->uio_update_cb((const uint8_t *)characteristic->applicationValue,
                                     characteristic->valueLen);
    }
    return NS_STATUS_SUCCESS;
}

static uint16_t tio_ble_notify_period_ms(const tio_ble_context_t *ctx) {
    if (ctx != NULL && ctx->notify_period_ms != 0u) {
        return ctx->notify_period_ms;
    }
    return TIO_BLE_DEFAULT_NOTIFY_PERIOD_MS;
}

static uint16_t tio_ble_base_handle(const tio_ble_context_t *ctx) {
    if (ctx != NULL && ctx->base_handle != 0u) {
        return ctx->base_handle;
    }
    return TIO_BLE_DEFAULT_BASE_HANDLE;
}

static const char *tio_ble_service_name(const tio_ble_context_t *ctx) {
    if (ctx != NULL && ctx->service_name != NULL && ctx->service_name[0] != '\0') {
        return ctx->service_name;
    }
    return "Tileio";
}

static void tio_ble_fix_fixed_length_value(ns_ble_characteristic_t *characteristic) {
    characteristic->value.settings &= (uint8_t)~ATTS_SET_VARIABLE_LEN;
}

static uint32_t tio_ble_create_slot_characteristic(
    ns_ble_characteristic_t *characteristic, const char *uuid, uint8_t *value,
    ns_ble_characteristic_read_handler_t read_handler, uint16_t *attribute_count,
    uint16_t notify_period_ms) {
    uint32_t status = (uint32_t)ns_ble_create_characteristic(
        characteristic, uuid, value, TIO_BLE_SLOT_SIG_BUF_LEN, NS_BLE_READ | NS_BLE_NOTIFY,
        read_handler, NULL, tio_ble_notify_handler, notify_period_ms, true, attribute_count);
    if (status != NS_STATUS_SUCCESS) {
        return status;
    }
    tio_ble_fix_fixed_length_value(characteristic);
    return NS_STATUS_SUCCESS;
}

static uint32_t tio_ble_create_service_objects(tio_ble_context_t *ctx) {
    const char *service_name = tio_ble_service_name(ctx);
    size_t service_name_len = strlen(service_name);
    uint16_t notify_period_ms = tio_ble_notify_period_ms(ctx);
    uint32_t status;
    uint8_t slot;

    if (service_name_len > sizeof(g_tio_ble.service.name) - 1u) {
        return NS_STATUS_FAILURE;
    }

    status = (uint32_t)ns_ble_char2uuid(TIO_SLOT_SVC_UUID, &g_tio_ble.service.uuid128);
    if (status != NS_STATUS_SUCCESS) {
        return status;
    }

    memcpy(g_tio_ble.service.name, service_name, service_name_len);
    g_tio_ble.service.name[service_name_len] = '\0';
    g_tio_ble.service.nameLen = (uint8_t)service_name_len;
    g_tio_ble.service.baseHandle = tio_ble_base_handle(ctx);
    g_tio_ble.service.poolConfig = ctx->pool_config;
    g_tio_ble.service.numAttributes = 0u;

    if (ctx->device_info != NULL) {
        status = (uint32_t)ns_ble_service_set_device_info(&g_tio_ble.service, ctx->device_info);
        if (status != NS_STATUS_SUCCESS) {
            return status;
        }
    }
    if (ctx->connection_config != NULL) {
        status = (uint32_t)ns_ble_service_set_connection_config(
            &g_tio_ble.service, ctx->connection_config);
        if (status != NS_STATUS_SUCCESS) {
            return status;
        }
    }
    if (ctx->event_handler != NULL) {
        ns_ble_service_set_event_handler(
            &g_tio_ble.service, ctx->event_handler, ctx->event_context);
    }

    for (slot = 0u; slot < TIO_BLE_SLOT_COUNT; ++slot) {
        status = tio_ble_create_slot_characteristic(
            &g_tio_ble.slot_sig[slot], g_tio_ble_slot_sig_uuids[slot],
            g_tio_ble.slot_sig_value[slot], tio_ble_read_handler,
            &g_tio_ble.service.numAttributes, notify_period_ms);
        if (status != NS_STATUS_SUCCESS) {
            return status;
        }

        status = (uint32_t)ns_ble_create_characteristic(
            &g_tio_ble.slot_met[slot], g_tio_ble_slot_met_uuids[slot],
            g_tio_ble.slot_met_value[slot], TIO_BLE_SLOT_MET_BUF_LEN,
            NS_BLE_READ | NS_BLE_NOTIFY, tio_ble_read_handler, NULL,
            tio_ble_notify_handler, notify_period_ms, true,
            &g_tio_ble.service.numAttributes);
        if (status != NS_STATUS_SUCCESS) {
            return status;
        }
        tio_ble_fix_fixed_length_value(&g_tio_ble.slot_met[slot]);
    }

    status = (uint32_t)ns_ble_create_characteristic(
        &g_tio_ble.uio, TIO_UIO_CHAR_UUID, g_tio_ble.uio_value, TIO_BLE_UIO_BUF_LEN,
        NS_BLE_READ | NS_BLE_WRITE | NS_BLE_NOTIFY, tio_ble_read_handler,
        tio_ble_uio_write_handler, tio_ble_notify_handler, notify_period_ms, true,
        &g_tio_ble.service.numAttributes);
    if (status != NS_STATUS_SUCCESS) {
        return status;
    }
    tio_ble_fix_fixed_length_value(&g_tio_ble.uio);

    g_tio_ble.service.numCharacteristics = (TIO_BLE_SLOT_COUNT * 2u) + 1u;
    return NS_STATUS_SUCCESS;
}

uint32_t tio_ble_init(tio_ble_context_t *ctx) {
    uint32_t status;
    uint8_t slot;

    if (ctx == NULL || ctx->pool_config == NULL) {
        return NS_STATUS_FAILURE;
    }
    if (g_tio_ble.initialized) {
        return NS_STATUS_FAILURE;
    }

    memset(&g_tio_ble, 0, sizeof(g_tio_ble));
    g_tio_ble.ctx = ctx;

    status = tio_ble_create_service_objects(ctx);
    if (status != NS_STATUS_SUCCESS) {
        return status;
    }

    status = (uint32_t)ns_ble_create_service(&g_tio_ble.service);
    if (status != NS_STATUS_SUCCESS) {
        return status;
    }

    for (slot = 0u; slot < TIO_BLE_SLOT_COUNT; ++slot) {
        status = (uint32_t)ns_ble_add_characteristic(&g_tio_ble.service, &g_tio_ble.slot_sig[slot]);
        if (status != NS_STATUS_SUCCESS) {
            return status;
        }
        status = (uint32_t)ns_ble_add_characteristic(&g_tio_ble.service, &g_tio_ble.slot_met[slot]);
        if (status != NS_STATUS_SUCCESS) {
            return status;
        }
    }

    status = (uint32_t)ns_ble_add_characteristic(&g_tio_ble.service, &g_tio_ble.uio);
    if (status != NS_STATUS_SUCCESS) {
        return status;
    }

    status = (uint32_t)ns_ble_start_service(&g_tio_ble.service);
    if (status != NS_STATUS_SUCCESS) {
        return status;
    }

    g_tio_ble.initialized = true;
    return NS_STATUS_SUCCESS;
}

uint32_t tio_ble_send_slot_data(
    uint8_t slot, uint8_t slot_type, const uint8_t *data, uint32_t length) {
    uint8_t *buffer;
    ns_ble_characteristic_t *characteristic;

    if (!g_tio_ble.initialized || data == NULL || length > TIO_BLE_SLOT_DATA_MAX_LEN) {
        return NS_STATUS_FAILURE;
    }
    if (slot >= TIO_BLE_SLOT_COUNT) {
        return NS_STATUS_FAILURE;
    }
    if (slot_type == TIO_BLE_SLOT_TYPE_SIGNAL) {
        buffer = g_tio_ble.slot_sig_value[slot];
        characteristic = &g_tio_ble.slot_sig[slot];
    } else if (slot_type == TIO_BLE_SLOT_TYPE_METRIC) {
        buffer = g_tio_ble.slot_met_value[slot];
        characteristic = &g_tio_ble.slot_met[slot];
    } else {
        return NS_STATUS_FAILURE;
    }

    buffer[0] = (uint8_t)(length & 0xFFu);
    buffer[1] = (uint8_t)((length >> 8) & 0xFFu);
    memset(buffer + 2u, 0, TIO_BLE_SLOT_DATA_MAX_LEN);
    memcpy(buffer + 2u, data, length);

    return (uint32_t)ns_ble_send_value(characteristic, NULL);
}

uint32_t tio_ble_send_uio_state(const uint8_t *data, uint32_t length) {
    if (!g_tio_ble.initialized || data == NULL || length != TIO_BLE_UIO_BUF_LEN) {
        return NS_STATUS_FAILURE;
    }

    memcpy(g_tio_ble.uio_value, data, length);
    return (uint32_t)ns_ble_send_value(&g_tio_ble.uio, NULL);
}
