/* SPDX-License-Identifier: Apache-2.0 */

/**
 * J1939: Electronic Control Unit (ECU) holding one or more
 *        Controller Applications (CAs).
 */
#include <stdbool.h>
#include <stdint.h>
#include "compiler.h"
#include "j1939.h"

#define CONN_MODE_RTS 0x10u
#define CONN_MODE_CTS 0x11u
#define CONN_MODE_EOM_ACK 0x13u
#define CONN_MODE_BAM 0x20u
#define CONN_MODE_ABORT 0xFFu

static const struct j1939_pgn TP_CM = J1939_INIT_PGN(0x00u, 0xECu, 0x00u);
static const struct j1939_pgn TP_DT = J1939_INIT_PGN(0x00u, 0xEBu, 0x00u);

static int send_tp_rts(uint8_t priority, uint8_t src, uint8_t dst,
		       uint16_t size, uint8_t num_packets)
{
	uint8_t data[8] = {
		CONN_MODE_RTS,
		size & 0xFF,
		size >> 8,
		num_packets,
		0xFF,
		TP_CM.pdu_specific,
		TP_CM.pdu_format,
		TP_CM.data_page,
	};

	return j1939_send(&TP_CM, priority, src, dst, data, 8);
}

static int send_tp_dt(const uint8_t src, const uint8_t dst, uint8_t *data,
		      const uint32_t len)
{
	return j1939_send(&TP_DT, J1939_PRIORITY_LOW, src, dst, data, len);
}

static int send_tp_bam(const uint8_t priority, const uint8_t src,
		       const uint16_t size, const uint8_t num_packets)
{
	uint8_t data[8] = {
		CONN_MODE_BAM,
		size & 0x00FF,
		size >> 8,
		num_packets,
		0xff,
		TP_CM.pdu_specific,
		TP_CM.pdu_format,
		TP_CM.data_page,
	};

	return j1939_send(&TP_CM, priority, src, ADDRESS_GLOBAL, data,
			  ARRAY_SIZE(data));
}

static int send_abort(const uint8_t src, const uint8_t dst,
		      const uint8_t reason)
{
	uint8_t data[8] = {
		CONN_MODE_ABORT,
		reason,
		0xFF,
		0xFF,
		0xFF,
		TP_CM.pdu_specific,
		TP_CM.pdu_format,
		TP_CM.data_page,
	};
	return j1939_send(&TP_DT, J1939_PRIORITY_LOW, src, dst, data,
			  ARRAY_SIZE(data));
}

static int send_tp_cts(const uint8_t src, const uint8_t dst,
		       const uint8_t num_packets, const uint8_t next_packet)
{
	uint8_t data[8] = {
		CONN_MODE_CTS,
		num_packets,
		next_packet,
		0xFF,
		0xFF,
		TP_CM.pdu_specific,
		TP_CM.pdu_format,
		TP_CM.data_page,
	};
	return j1939_send(&TP_DT, J1939_PRIORITY_LOW, src, dst, data,
			  ARRAY_SIZE(data));
}

static int send_tp_eom_ack(const uint8_t src, const uint8_t dst,
			   const uint16_t size, const uint8_t num_packets)
{
	uint8_t data[8] = {
		CONN_MODE_EOM_ACK,
		size & 0x00FF,
		(size >> 8),
		num_packets,
		0xFF,
		TP_CM.pdu_specific,
		TP_CM.pdu_format,
		TP_CM.data_page,
	};
	return j1939_send(&TP_CM, J1939_PRIORITY_LOW, src, dst, data,
			  ARRAY_SIZE(data));
}

int j1939_tp(struct j1939_pgn *pgn, const uint8_t priority, const uint8_t src,
	     const uint8_t dst, uint8_t *data, const uint16_t len)
{
	int ret;
	uint8_t *block = data;
	uint16_t num_packets = len / 8u;
	uint16_t odd_packet = len % 8u;

	ret = send_tp_rts(priority, src, dst, len, num_packets);
	if (unlikely(ret < 0)) {
		return ret;
	}

	while (num_packets > 0) {
		ret = send_tp_dt(src, dst, block, 8);
		if (unlikely(ret < 0)) {
			return ret;
		}
		num_packets--;
		block += 8;
	}

	if (odd_packet) {
		ret = send_tp_dt(src, dst, block, odd_packet);
		if (unlikely(ret < 0)) {
			return ret;
		}
	}

	return send_tp_eom_ack(src, dst, 8, num_packets + odd_packet);
}
