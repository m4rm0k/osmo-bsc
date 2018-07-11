/* Handover FSM API for intra-BSC and inter-BSC MT (to this BSC) Handover.
 * (For inter-BSC MO, from this BSC, see handover_inter_mo_fsm.c) */
#pragma once

#include <osmocom/bsc/debug.h>
#include <osmocom/bsc/handover.h>

const char *handover_status(struct gsm_subscriber_connection *conn);

/* This macro automatically includes a final \n, if omitted. */
#define LOG_HO(conn, level, fmt, args...) do { \
	if (conn->ho.fi) \
		LOGPFSML(conn->ho.fi, level, "%s: " fmt "%s", \
			 handover_status(conn), \
			 ## args, LOG_ADD_NEWLINE(fmt)); \
	else \
		LOGP(DHODEC, level, "%s: " fmt "%s", \
		     handover_status(conn), \
		     ## args, LOG_ADD_NEWLINE(fmt)); \
	} while(0)

/* Terminology:
 * Intra-Cell: stays within one BTS, this should actually be an Assignment.
 * Intra-BSC: stays within one BSC, but moves between BTSes.
 * Inter-BSC: moves between BSCs.
 * Inter-BSC MO: Mobile Originated: move away from this BSC to another one.
 * Inter-BSC MT: Mobile Terminated: move from another BSC to this one.
 */

enum handover_fsm_state {
	HO_ST_NOT_STARTED,

	HO_ST_WAIT_LCHAN_ACTIVE,
	HO_ST_WAIT_RR_HO_DETECT,
	HO_ST_WAIT_RR_HO_COMPLETE,
	HO_ST_WAIT_LCHAN_ESTABLISHED,
	HO_ST_WAIT_MGW_ENDPOINT_TO_MSC,

	/* The inter-BSC MO Handover FSM has completely separate states, but since it makes sense for it
	 * to also live in conn->ho.fi, it should share the same event enum. From there it is merely
	 * cosmetic to just include the separate fairly trivial states in the same FSM definition.
	 * An inter-MO FSM is almost unnecessary. The sole reason is to wait whether the MSC indeed
	 * clears the conn, and if not to log and count a failed handover attempt. */
	MOHO_ST_WAIT_HO_COMMAND,
	MOHO_ST_WAIT_CLEAR,
};

enum handover_fsm_event {
	HO_EV_LCHAN_ACTIVE,
	HO_EV_LCHAN_ESTABLISHED,
	HO_EV_LCHAN_ERROR,
	HO_EV_RR_HO_DETECT,
	HO_EV_RR_HO_COMPLETE,
	HO_EV_RR_HO_FAIL,
	HO_EV_MSC_MGW_OK,
	HO_EV_MSC_MGW_FAIL,
	HO_EV_CONN_RELEASING,

	MOHO_EV_BSSMAP_HO_COMMAND,
};

struct moho_rx_bssmap_ho_command {
	const uint8_t *l3_info;
	const uint8_t l3_info_len;
};

/* To be sent along with the HO_EV_RR_HO_DETECT */
struct handover_rr_detect_data {
	struct msgb *msg;
	const uint8_t *access_delay;
};

void handover_fsm_init();

void handover_request(struct handover_mo_req *req);
void handover_start(struct handover_mo_req *req);
void handover_start_inter_bsc_mt(struct gsm_subscriber_connection *conn,
				 struct msgb *ho_request_msg);
void handover_end(struct gsm_subscriber_connection *conn, enum handover_result result);

const char *handover_status(struct gsm_subscriber_connection *conn);
bool handover_is_sane(struct gsm_subscriber_connection *conn, struct gsm_lchan *old_lchan,
		      struct gsm_lchan *new_lchan);