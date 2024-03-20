/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>

#include <nfc_t2t_lib.h>
#include <nfc/ndef/msg.h>
#include <nfc/ndef/text_rec.h>

#include <dk_buttons_and_leds.h>

#define MAX_REC_COUNT 3
#define NDEF_MSG_BUF_SIZE 128

#define NFC_FIELD_LED DK_LED1

// struct nfc_ndef_record_desc geotag_record_ndef_generic_record_desc = {
// 	.tnf = 0x1,
// 	.id_length = 0,
// 	.id = 0,
// 	.type_length = 1,
// 	.type = "U",
// 	.payload_constructor = (payload_constructor_t)make_payload_geotag,
// 	.payload_descriptor = (void *)NULL};

static const uint8_t geo_payload[] = {
	0, 'g', 'e', 'o', ':', '3', '8', '.', '0', '3', '6', '2', '4', '1', ',', '-', '7', '8', '.', '5', '0', '5', '9', '7', '8'}; // lat and long of clem

static int make_payload_geotag(void *payload_descriptor, uint8_t *buf, uint32_t *len)
{
	memcpy(buf, geo_payload, sizeof(geo_payload));
	*len = sizeof(geo_payload);
	return 0;
}

/* Text message in English with its language code. */
static const uint8_t en_payload[] = {
	'C', 'L', 'E', 'M', ' ', 'L', 'I', 'B', '!'}; // updated to our group
static const uint8_t en_code[] = {'e', 'n'};

/* Buffer used to hold an NFC NDEF message. */
static uint8_t ndef_msg_buf[NDEF_MSG_BUF_SIZE];

static void nfc_callback(void *context,
						 nfc_t2t_event_t event,
						 const uint8_t *data,
						 size_t data_length)
{
	ARG_UNUSED(context);
	ARG_UNUSED(data);
	ARG_UNUSED(data_length);

	switch (event)
	{
	case NFC_T2T_EVENT_FIELD_ON:
		dk_set_led_on(NFC_FIELD_LED);
		break;
	case NFC_T2T_EVENT_FIELD_OFF:
		dk_set_led_off(NFC_FIELD_LED);
		break;
	default:
		break;
	}
}

/**
 * @brief Function for encoding the NDEF text message.
 */
static int welcome_msg_encode(uint8_t *buffer, uint32_t *len)
{
	int err;

	struct nfc_ndef_record_desc geotag_record_ndef_generic_record_desc = {
		.tnf = 0x1,
		.id_length = 0,
		.id = 0,
		.type_length = 1,
		.type = "U",
		.payload_constructor = (payload_constructor_t)make_payload_geotag,
		.payload_descriptor = (void *)NULL};

	/* Create NFC NDEF text record description in English */
	NFC_NDEF_TEXT_RECORD_DESC_DEF(nfc_en_text_rec,
								  UTF_8,
								  en_code,
								  sizeof(en_code),
								  en_payload,
								  sizeof(en_payload));

	/* Create NFC NDEF message description, capacity - MAX_REC_COUNT
	 * records
	 */
	// NFC_NDEF_MSG_DEF(nfc_geo_record, MAX_REC_COUNT);
	NFC_NDEF_MSG_DEF(nfc_msg, MAX_REC_COUNT);

	err = nfc_ndef_msg_record_add(&NFC_NDEF_MSG(nfc_msg),
								  &NFC_NDEF_TEXT_RECORD_DESC(geotag_record));
	if (err < 0)
	{
		printk("Cannot add first record!\n");
		return err;
	}
	/* Add text records to NDEF text message */
	err = nfc_ndef_msg_record_add(&NFC_NDEF_MSG(nfc_msg),
								  &NFC_NDEF_TEXT_RECORD_DESC(nfc_en_text_rec));
	if (err < 0)
	{
		printk("Cannot add second record!\n");
		return err;
	}

	err = nfc_ndef_msg_encode(&NFC_NDEF_MSG(nfc_msg),
							  buffer,
							  len);

	if (err < 0)
	{
		printk("Cannot text encode message!\n");
	}

	return err;
}

int main(void)
{
	uint32_t len = sizeof(ndef_msg_buf);

	printk("Starting NFC Text Record example\n");

	/* Configure LED-pins as outputs */
	if (dk_leds_init() < 0)
	{
		printk("Cannot init LEDs!\n");
		goto fail;
	}

	/* Set up NFC */
	if (nfc_t2t_setup(nfc_callback, NULL) < 0)
	{
		printk("Cannot setup NFC T2T library!\n");
		goto fail;
	}

	/* Encode welcome message */
	if (welcome_msg_encode(ndef_msg_buf, &len) < 0)
	{
		printk("Cannot encode message!\n");
		goto fail;
	}

	/* Set created message as the NFC payload */
	if (nfc_t2t_payload_set(ndef_msg_buf, len) < 0)
	{
		printk("Cannot set payload!\n");
		goto fail;
	}

	/* Start sensing NFC field */
	if (nfc_t2t_emulation_start() < 0)
	{
		printk("Cannot start emulation!\n");
		goto fail;
	}
	printk("NFC configuration done\n");

	return 0;

fail:
#if CONFIG_REBOOT
	sys_reboot(SYS_REBOOT_COLD);
#endif /* CONFIG_REBOOT */

	return -EIO;
}
