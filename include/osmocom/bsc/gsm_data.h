#ifndef _GSM_DATA_H
#define _GSM_DATA_H

#include <stdint.h>
#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>

#include <osmocom/core/timer.h>
#include <osmocom/core/rate_ctr.h>
#include <osmocom/core/select.h>
#include <osmocom/core/stats.h>
#include <osmocom/core/stat_item.h>
#include <osmocom/gsm/bts_features.h>
#include <osmocom/gsm/protocol/gsm_08_08.h>
#include <osmocom/gsm/protocol/gsm_48_049.h>
#include <osmocom/gsm/gsm0808.h>
#include <osmocom/gsm/gsm48.h>
#include <osmocom/core/fsm.h>
#include <osmocom/core/tdef.h>

#include <osmocom/crypt/auth.h>

#include <osmocom/bsc/rest_octets.h>

#include <osmocom/core/bitvec.h>
#include <osmocom/gsm/gsm_utils.h>
#include <osmocom/gsm/rxlev_stat.h>
#include <osmocom/gsm/protocol/gsm_08_58.h>
#include <osmocom/gsm/protocol/gsm_12_21.h>
#include <osmocom/abis/e1_input.h>
#include <osmocom/bsc/meas_rep.h>
#include <osmocom/bsc/acc_ramp.h>
#include <osmocom/bsc/neighbor_ident.h>
#include <osmocom/bsc/osmux.h>

#define GSM_T3122_DEFAULT 10

struct mgcp_client_conf;
struct mgcp_client;
struct gsm0808_cell_id;
struct osmo_mgcpc_ep;

/** annotations for msgb ownership */
#define __uses

#define OBSC_NM_W_ACK_CB(__msgb) (__msgb)->cb[3]

struct bsc_subscr;
struct gprs_ra_id;
struct handover;

#define OBSC_LINKID_CB(__msgb)	(__msgb)->cb[3]

#define tmsi_from_string(str) strtoul(str, NULL, 10)

/* 3-bit long values */
#define EARFCN_PRIO_INVALID 8
#define EARFCN_MEAS_BW_INVALID 8
/* 5-bit long values */
#define EARFCN_QRXLV_INVALID 32
#define EARFCN_THRESH_LOW_INVALID 32

struct msgb;
typedef int gsm_cbfn(unsigned int hooknum,
		     unsigned int event,
		     struct msgb *msg,
		     void *data, void *param);

/* Maximum number of neighbor cells whose average we track */
#define MAX_NEIGH_MEAS		10
/* Maximum size of the averaging window for neighbor cells */
#define MAX_WIN_NEIGH_AVG	10
/* Maximum number of report history we store */
#define MAX_MEAS_REP		10

/* processed neighbor measurements for one cell */
struct neigh_meas_proc {
	uint16_t arfcn;
	uint8_t bsic;
	uint8_t rxlev[MAX_WIN_NEIGH_AVG];
	unsigned int rxlev_cnt;
	uint8_t last_seen_nr;
};

struct gsm_classmark {
	bool classmark1_set;
	struct gsm48_classmark1 classmark1;
	uint8_t classmark2_len;
	uint8_t classmark2[3];
	uint8_t classmark3_len;
	uint8_t classmark3[14]; /* if cm3 gets extended by spec, it will be truncated */
};

enum subscr_sccp_state {
	SUBSCR_SCCP_ST_NONE,
	SUBSCR_SCCP_ST_WAIT_CONN_CONF,
	SUBSCR_SCCP_ST_CONNECTED
};

enum channel_rate {
	CH_RATE_SDCCH,
	CH_RATE_HALF,
	CH_RATE_FULL,
};

struct channel_mode_and_rate {
	enum gsm48_chan_mode chan_mode;
	enum channel_rate chan_rate;
	uint16_t s15_s0;
};

/* Information retrieved during an Assignment Request from the MSC. This is storage of the Assignment instructions
 * parsed from the Assignment Request message, to pass on until the gscon and assignment FSMs have decided whether an
 * Assignment is actually going to be carried out. Should remain unchanged after initial decoding. */
struct assignment_request {
	bool aoip;

	uint16_t msc_assigned_cic;

	char msc_rtp_addr[INET_ADDRSTRLEN];
	uint16_t msc_rtp_port;
	bool use_osmux;
	uint8_t osmux_cid;

	/* Rate/codec setting in preference order (need at least 1 !) */
	int n_ch_mode_rate;
	struct channel_mode_and_rate ch_mode_rate[3];
};

/* State of an ongoing Assignment, while the assignment_fsm is still busy. This serves as state separation to keep the
 * currently used lchan and gscon unmodified until the outcome of an Assignment is known. If the Assignment fails, this
 * state is simply discarded, and the gscon carries on with the original lchan remaining unchanged. */
struct assignment_fsm_data {
	struct assignment_request req;
	bool requires_voice_stream;

	struct osmo_fsm_inst *fi;
	struct gsm_lchan *new_lchan;

	/* Whether this assignment triggered creation of the MGW endpoint: if the assignment
	 * fails, we will release that again as soon as possible. (If false, the endpoint already
	 * existed before or isn't needed at all.)*/
	struct osmo_mgcpc_ep_ci *created_ci_for_msc;

	enum gsm0808_cause failure_cause;
	enum gsm48_rr_cause rr_cause;

	bool result_rate_ctr_done;
};

enum hodec_id {
	HODEC_NONE,
	HODEC1 = 1,
	HODEC2 = 2,
	HODEC_USER,
	HODEC_REMOTE,
};

/* For example, to count specific kinds of ongoing handovers, it is useful to be able to OR-combine
 * scopes. */
enum handover_scope {
	HO_NO_HANDOVER = 0,
	HO_INTRA_CELL = 0x1,
	HO_INTRA_BSC = 0x2,
	HO_INTER_BSC_OUT = 0x4,
	HO_INTER_BSC_IN = 0x8,
	HO_SCOPE_ALL = 0xffff,
};

extern const struct value_string handover_scope_names[];
inline static const char *handover_scope_name(enum handover_scope val)
{ return get_value_string(handover_scope_names, val); }

struct handover_out_req {
	enum hodec_id from_hodec_id;
	struct gsm_lchan *old_lchan;
	struct neighbor_ident_key target_nik;
	enum gsm_chan_t new_lchan_type; /*< leave GSM_LCHAN_NONE to use same as old_lchan */
};

struct handover_in_req {
	struct gsm0808_channel_type ct;
	struct gsm0808_speech_codec_list scl;
	struct gsm0808_encrypt_info ei;
	struct gsm_classmark classmark;
	/* chosen_encr_alg reflects the encoded value as in RSL_ENC_ALG_A5(a5_numer):
	 * chosen_encr_alg == 1 means A5/0 i.e. no encryption, chosen_encr_alg == 4 means A5/3.
	 * chosen_encr_alg == 0 means no such IE was present. */
	uint8_t chosen_encr_alg;
	struct gsm0808_cell_id cell_id_serving;
	char cell_id_serving_name[64];
	struct gsm0808_cell_id cell_id_target;
	char cell_id_target_name[64];
	uint16_t msc_assigned_cic;
	char msc_assigned_rtp_addr[INET_ADDRSTRLEN];
	uint16_t msc_assigned_rtp_port;
};

struct handover {
	struct osmo_fsm_inst *fi;

	enum hodec_id from_hodec_id;
	enum handover_scope scope;
	enum gsm_chan_t new_lchan_type;
	struct neighbor_ident_key target_cell;

	uint8_t ho_ref;
	struct gsm_bts *new_bts;
	struct gsm_lchan *new_lchan;
	bool async;
	struct handover_in_req inter_bsc_in;
	struct osmo_mgcpc_ep_ci *created_ci_for_msc;
};

/* active radio connection of a mobile subscriber */
struct gsm_subscriber_connection {
	/* global linked list of subscriber_connections */
	struct llist_head entry;

	/* FSM instance to control the subscriber connection state (RTP, A) */
	struct osmo_fsm_inst *fi;

	/* libbsc subscriber information (if available) */
	struct bsc_subscr *bsub;

	/* back pointers */
	struct gsm_network *network;

	/* the primary / currently active lchan to the BTS/subscriber. During Assignment and Handover, separate lchans
	 * are kept in the .assignment or .handover sub-structs, respectively, so that this lchan remains unaffected
	 * until Assignment or Handover have actually succeeded. */
	struct gsm_lchan *lchan;

	/* Only valid during an ongoing Assignment; might be overwritten at any time by a failed Assignment attempt.
	 * Once an Assignment was successful, all relevant state must be copied out of this sub-struct. */
	struct assignment_fsm_data assignment;

	/* handover information, if a handover is pending for this conn. Valid only during an ongoing Handover
	 * operation. If a Handover was successful, all relevant state must be copied out of this sub-struct. */
	struct handover ho;

	/* Queue DTAP messages during handover/assignment (msgb_enqueue()/msgb_dequeue())*/
	struct llist_head dtap_queue;
	unsigned int dtap_queue_len;

	struct {
		int failures;
		struct penalty_timers *penalty_timers;
	} hodec2;

	/* "Codec List (MSC Preferred)" as received by the BSSAP Assignment Request. 3GPP 48.008
	 * 3.2.2.103 says:
	 *         The "Codec List (MSC Preferred)" shall not include codecs
	 *         that are not supported by the MS.
	 * i.e. by heeding the "Codec list (MSC Preferred)", we inherently heed the MS bearer
	 * capabilities, which the MSC is required to translate into the codec list. */
	struct gsm0808_speech_codec_list codec_list;

	/* flag to prevent multiple simultaneous ciphering commands */
	int ciphering_handled;

	/* SCCP connection associatd with this subscriber_connection */
	struct {
		/* for advanced ping/pong */
		int send_ping;

		/* SCCP connection related */
		struct bsc_msc_data *msc;

		/* Sigtran connection ID */
		int conn_id;
		enum subscr_sccp_state state;
	} sccp;

	/* for audio handling */
	struct {
		uint16_t msc_assigned_cic;

		/* RTP address where the MSC expects us to send the RTP stream coming from the BTS. */
		char msc_assigned_rtp_addr[INET_ADDRSTRLEN];
		uint16_t msc_assigned_rtp_port;

		/* The endpoint at the MGW used to join both BTS and MSC side connections, e.g.
		 * "rtpbridge/23@mgw". */
		struct osmo_mgcpc_ep *mgw_endpoint;

		/* The connection identifier of the osmo_mgcpc_ep used to transceive RTP towards the MSC.
		 * (The BTS side CI is handled by struct gsm_lchan and the lchan_fsm.) */
		struct osmo_mgcpc_ep_ci *mgw_endpoint_ci_msc;
	} user_plane;

	/* LCLS (local call, local switch) related state */
	struct {
		uint8_t global_call_ref[15];
		uint8_t global_call_ref_len; /* length of global_call_ref */
		enum gsm0808_lcls_config config;	/* TS 48.008 3.2.2.116 */
		enum gsm0808_lcls_control control;	/* TS 48.008 3.2.2.117 */
		/* LCLS FSM */
		struct osmo_fsm_inst *fi;
		/* pointer to "other" connection, if Call Leg Relocation was successful */
		struct gsm_subscriber_connection *other;
	} lcls;

	/* MS Power Class, TS 05.05 sec 4.1.1 "Mobile station". 0 means unset. */
	uint8_t ms_power_class:3;
};


/* 16 is the max. number of SI2quater messages according to 3GPP TS 44.018 Table 10.5.2.33b.1:
   4-bit index is used (2#1111 = 10#15) */
#define SI2Q_MAX_NUM 16
/* length in bits (for single SI2quater message) */
#define SI2Q_MAX_LEN 160
#define SI2Q_MIN_LEN 18

struct osmo_bsc_data;

struct osmo_bsc_sccp_con;

/* Channel Request reason */
enum gsm_chreq_reason_t {
	GSM_CHREQ_REASON_EMERG,
	GSM_CHREQ_REASON_PAG,
	GSM_CHREQ_REASON_CALL,
	GSM_CHREQ_REASON_LOCATION_UPD,
	GSM_CHREQ_REASON_OTHER,
	GSM_CHREQ_REASON_PDCH,
};

/* lchans 0..3 are SDCCH in combined channel configuration,
   use 4 as magic number for BCCH hack - see osmo-bts-../oml.c:opstart_compl() */
#define CCCH_LCHAN 4

#define TRX_NR_TS	8
#define TS_MAX_LCHAN	8

#define HARDCODED_ARFCN 123
#define HARDCODED_BSIC	0x3f	/* NCC = 7 / BCC = 7 */

/* for multi-drop config */
#define HARDCODED_BTS0_TS	1
#define HARDCODED_BTS1_TS	6
#define HARDCODED_BTS2_TS	11

#define MAX_VERSION_LENGTH 64

enum gsm_hooks {
	GSM_HOOK_NM_SWLOAD,
	GSM_HOOK_RR_PAGING,
	GSM_HOOK_RR_SECURITY,
};

enum bts_gprs_mode {
	BTS_GPRS_NONE = 0,
	BTS_GPRS_GPRS = 1,
	BTS_GPRS_EGPRS = 2,
};

struct gsm_lchan;
struct osmo_rtp_socket;
struct rtp_socket;

/* Network Management State */
struct gsm_nm_state {
	uint8_t operational;
	uint8_t administrative;
	uint8_t availability;
};

struct gsm_abis_mo {
	uint8_t obj_class;
	uint8_t procedure_pending;
	struct abis_om_obj_inst obj_inst;
	const char *name;
	struct gsm_nm_state nm_state;
	struct tlv_parsed *nm_attr;
	struct gsm_bts *bts;
};

/* Ericsson OM2000 Managed Object */
struct abis_om2k_mo {
	uint8_t class;
	uint8_t bts;
	uint8_t assoc_so;
	uint8_t inst;
} __attribute__ ((packed));

struct om2k_mo {
	struct abis_om2k_mo addr;
	struct osmo_fsm_inst *fsm;
};

#define A38_XOR_MIN_KEY_LEN	12
#define A38_XOR_MAX_KEY_LEN	16
#define A38_COMP128_KEY_LEN	16
#define RSL_ENC_ALG_A5(x)	(x+1)
#define MAX_EARFCN_LIST 32

/* is the data link established? who established it? */
#define LCHAN_SAPI_UNUSED	0
#define LCHAN_SAPI_MS		1
#define LCHAN_SAPI_NET		2

/* BTS ONLY */
#define MAX_NUM_UL_MEAS	104
#define LC_UL_M_F_L1_VALID	(1 << 0)
#define LC_UL_M_F_RES_VALID	(1 << 1)

struct bts_ul_meas {
	/* BER in units of 0.01%: 10.000 == 100% ber, 0 == 0% ber */
	uint16_t ber10k;
	/* timing advance offset (in quarter bits) */
	int16_t ta_offs_qbits;
	/* C/I ratio in dB */
	float c_i;
	/* flags */
	uint8_t is_sub:1;
	/* RSSI in dBm * -1 */
	uint8_t inv_rssi;
};

struct bts_codec_conf {
	uint8_t hr;
	uint8_t efr;
	uint8_t amr;
};

struct amr_mode {
	uint8_t mode;
	uint8_t threshold;
	uint8_t hysteresis;
};

struct amr_multirate_conf {
	uint8_t gsm48_ie[2];
	struct amr_mode ms_mode[4];
	struct amr_mode bts_mode[4];
	uint8_t num_modes;
};
/* /BTS ONLY */

enum lchan_csd_mode {
	LCHAN_CSD_M_NT,
	LCHAN_CSD_M_T_1200_75,
	LCHAN_CSD_M_T_600,
	LCHAN_CSD_M_T_1200,
	LCHAN_CSD_M_T_2400,
	LCHAN_CSD_M_T_9600,
	LCHAN_CSD_M_T_14400,
	LCHAN_CSD_M_T_29000,
	LCHAN_CSD_M_T_32000,
};

/* State of the SAPIs in the lchan */
enum lchan_sapi_state {
	LCHAN_SAPI_S_NONE,
	LCHAN_SAPI_S_REQ,
	LCHAN_SAPI_S_ASSIGNED,
	LCHAN_SAPI_S_REL,
	LCHAN_SAPI_S_ERROR,
};

#define MAX_A5_KEY_LEN	(128/8)

struct gsm_encr {
	uint8_t alg_id;
	uint8_t key_len;
	uint8_t key[MAX_A5_KEY_LEN];
};

#define LOGPLCHAN(lchan, ss, level, fmt, args...) \
	LOGP(ss, level, "%s (ss=%d,%s) (%s) " fmt, \
	     lchan ? gsm_ts_and_pchan_name(lchan->ts) : "-", \
	     lchan ? lchan->nr : 0, \
	     lchan ? gsm_lchant_name(lchan->type) : "-", \
	     bsc_subscr_name(lchan && lchan->conn ? lchan->conn->bsub : NULL), \
	     ## args)

/* Iterate lchans that have an FSM allocated based based on explicit pchan kind
 * (GSM_PCHAN_* constant).
 * Remark: PDCH related lchans are not handled in BSC but in PCU, so trying to
 * 	  iterate through GSM_PCHAN_PDCH is considered a void loop.
 */
#define ts_as_pchan_for_each_lchan(lchan, ts, as_pchan) \
	for (lchan = (ts)->lchan; \
	     ((lchan - (ts)->lchan) < ARRAY_SIZE((ts)->lchan)) \
	     && lchan->fi \
	     && lchan->nr < pchan_subslots(as_pchan); \
	     lchan++)

/* Iterate lchans that have an FSM allocated based on current PCHAN
 * mode set in \ref ts.
 * usage:
 * struct gsm_lchan *lchan;
 * struct gsm_bts_trx_ts *ts = get_some_timeslot();
 * ts_for_each_lchan(lchan, ts) {
 * 	LOGPLCHAN(DMAIN, LOGL_DEBUG, "hello world\n");
 * }
 */
#define ts_for_each_lchan(lchan, ts) ts_as_pchan_for_each_lchan(lchan, ts, (ts)->pchan_is)

/* Iterate over all possible lchans available that have an FSM allocated based
 * on PCHAN \ref ts (dynamic) configuration.
 * Iterate all lchan instances set up by this \ref ts type, including those
 * lchans currently disabled or in process of being enabled (e.g. due to dynamic
 * timeslot in switchover). Compare ts_for_each_lchan(), which iterates only the
 * enabled lchans.
 * For example, it is useful in case dynamic timeslot \ref ts is in process of
 * switching from configuration PDCH (no lchans) to TCH_F (1 lchan), where
 * pchan_is is still set to PDCH but \ref ts may contain already an \ref lchan
 * of type TCH_F which initiated the request to switch the \ts configuration.
 */
#define ts_for_each_potential_lchan(lchan, ts) ts_as_pchan_for_each_lchan(lchan, ts, (ts)->pchan_on_init)

enum lchan_activate_mode {
	FOR_NONE,
	FOR_MS_CHANNEL_REQUEST,
	FOR_ASSIGNMENT,
	FOR_HANDOVER,
	FOR_VTY,
};

extern const struct value_string lchan_activate_mode_names[];
static inline const char *lchan_activate_mode_name(enum lchan_activate_mode activ_for)
{ return get_value_string(lchan_activate_mode_names, activ_for); }

struct lchan_activate_info {
	enum lchan_activate_mode activ_for;
	struct gsm_subscriber_connection *for_conn;
	/* This always is for a specific lchan, so its lchan->type indicates full or half rate.
	 * When a dyn TS was selected, the lchan->type has been set to the desired rate. */
	enum gsm48_chan_mode chan_mode;
	struct gsm_encr encr;
	/* AMR config */
	uint16_t s15_s0;
	bool requires_voice_stream;
	bool wait_before_switching_rtp; /*< true = requires LCHAN_EV_READY_TO_SWITCH_RTP */
	uint16_t msc_assigned_cic;
	/* During intra-BSC handover, we keep the MGW endpoint intact and just re-route to the new lchan. This
	 * activate_info is for the new lchan, the re_use_mgw_endpoint_from_lchan points at the old lchan. */
	struct gsm_lchan *re_use_mgw_endpoint_from_lchan;
};

struct gsm_lchan {
	/* The TS that we're part of */
	struct gsm_bts_trx_ts *ts;
	/* The logical subslot number in the TS */
	uint8_t nr;
	char *name;

	char *last_error;

	struct osmo_fsm_inst *fi;
	struct osmo_fsm_inst *fi_rtp;
	struct osmo_mgcpc_ep_ci *mgw_endpoint_ci_bts;

	struct {
		struct lchan_activate_info info;
		bool activ_ack; /*< true as soon as RSL Chan Activ Ack is received */
		bool immediate_assignment_sent;
		/*! This flag ensures that when an lchan activation has succeeded, and we have already
		 * sent ACKs like Immediate Assignment or BSSMAP Assignment Complete, and if other errors
		 * occur later, e.g. during release, that we don't send a NACK out of context. */
		bool concluded;
		enum gsm0808_cause gsm0808_error_cause;
	} activate;

	struct {
		/* If an event to release the lchan comes in while still waiting for responses, just mark this
		 * flag, so that the lchan will gracefully release at the next sensible junction. */
		bool requested;
		bool do_rr_release;

		/* There is an RSL error cause of value 0, so we need a separate flag. */
		bool in_error;
		/* RSL error code, RSL_ERR_* */
		uint8_t rsl_error_cause;

		/* If a release event is being handled, ignore other ricocheting release events until that
		 * release handling has concluded. */
		bool in_release_handler;

		/* is this release at the end of a CSFB call? */
		bool is_csfb;
	} release;

	/* The logical channel type */
	enum gsm_chan_t type;
	/* RSL channel mode */
	enum rsl_cmod_spd rsl_cmode;
	/* If TCH, traffic channel mode */
	enum gsm48_chan_mode tch_mode;
	enum lchan_csd_mode csd_mode;
	/* Power levels for MS and BTS */
	uint8_t bs_power;
	uint8_t ms_power;
	/* Encryption information */
	struct gsm_encr encr;

	/* AMR bits */
	uint8_t mr_ms_lv[7];
	uint8_t mr_bts_lv[7];
	/* AMR bits were based on these rate bits: */
	uint16_t s15_s0;

	/* Established data link layer services */
	uint8_t sapis[8];

	struct {
		uint32_t bound_ip; /*< where the BTS receives RTP */
		uint16_t bound_port;
		uint32_t connect_ip; /*< where the BTS sends RTP to (MGW) */
		uint16_t connect_port;
		uint16_t conn_id;
		uint8_t rtp_payload;
		uint8_t rtp_payload2;
		uint8_t speech_mode;

		/* info we need to postpone the AoIP
		 * assignment completed message */
		struct {
			uint8_t rr_cause;
			bool valid;
		} ass_compl;
	} abis_ip;

	uint8_t rqd_ta;

	/* table of neighbor cell measurements */
	struct neigh_meas_proc neigh_meas[MAX_NEIGH_MEAS];

	/* cache of last measurement reports on this lchan */
	struct gsm_meas_rep meas_rep[MAX_MEAS_REP];
	int meas_rep_idx;
	int meas_rep_count;
	uint8_t meas_rep_last_seen_nr;

	/* GSM Random Access data */
	/* TODO: don't allocate this, rather keep an "is_present" flag */
	struct gsm48_req_ref *rqd_ref;

	struct gsm_subscriber_connection *conn;

	/* Depending on the preferences that where submitted together with
	 * the assignment and the current channel load, the BSC has to select
	 * one of the offered codec/rates. The final selection by the BSC is
	 * stored here and is used when sending the assignment complete or
	 * when performing a handover procedure. */
	struct channel_mode_and_rate ch_mode_rate;
};

/* One Timeslot in a TRX */
struct gsm_bts_trx_ts {
	struct gsm_bts_trx *trx;
	/* number of this timeslot at the TRX */
	uint8_t nr;

	struct osmo_fsm_inst *fi;
	char *last_errmsg;

	/* vty phys_chan_config setting, not necessarily in effect in case it was changed in the telnet
	 * vty after OML activation. Gets written on vty 'write file'. */
	enum gsm_phys_chan_config pchan_from_config;
	/* When the timeslot OML is established, pchan_from_config is copied here. This is the pchan
	 * currently in effect; for dynamic ts, this is the dyn kind (GSM_PCHAN_TCH_F_TCH_H_PDCH or
	 * GSM_PCHAN_TCH_F_PDCH) and does not show the pchan type currently active. */
	enum gsm_phys_chan_config pchan_on_init;
	/* This is the *actual* pchan type currently active. For dynamic timeslots, this reflects either
	 * GSM_PCHAN_NONE or one of the standard GSM_PCHAN_TCH_F, GSM_PCHAN_TCH_H, GSM_PCHAN_PDCH.
	 * Callers can use this transparently without being aware of dyn ts. */
	enum gsm_phys_chan_config pchan_is;

	/* After a PDCH ACT NACK, we shall not infinitely loop to try and ACT again.
	 * Also marks a timeslot where PDCH was deactivated by VTY. This is cleared whenever a timeslot
	 * enters IN_USE state, i.e. after each TCH use we try to PDCH ACT once again. */
	bool pdch_act_allowed;

	/* Whether TS_EV_OML_READY was received */
	bool is_oml_ready;
	/* Whether TS_EV_RSL_READY was received */
	bool is_rsl_ready;

	struct gsm_abis_mo mo;
	struct tlv_parsed nm_attr;
	uint8_t nm_chan_comb;
	int tsc;		/* -1 == use BTS TSC */

	struct {
		/* Parameters below are configured by VTY */
		int enabled;
		uint8_t maio;
		uint8_t hsn;
		struct bitvec arfcns;
		uint8_t arfcns_data[1024/8];
		/* This is the pre-computed MA for channel assignments */
		struct bitvec ma;
		uint8_t ma_len;	/* part of ma_data that is used */
		uint8_t ma_data[8];	/* 10.5.2.21: max 8 bytes value part */
	} hopping;

	/* To which E1 subslot are we connected */
	struct gsm_e1_subslot e1_link;

	union {
		struct {
			struct om2k_mo om2k_mo;
		} rbs2000;
	};

	struct gsm_lchan lchan[TS_MAX_LCHAN];
};

/* One TRX in a BTS */
struct gsm_bts_trx {
	/* list header in bts->trx_list */
	struct llist_head list;

	struct gsm_bts *bts;
	/* number of this TRX in the BTS */
	uint8_t nr;
	/* human readable name / description */
	char *description;
	/* how do we talk RSL with this TRX? */
	struct gsm_e1_subslot rsl_e1_link;
	uint8_t rsl_tei;
	struct e1inp_sign_link *rsl_link;

	/* Timeout for initiating the RSL connection. */
	struct osmo_timer_list rsl_connect_timeout;

	/* Some BTS (specifically Ericsson RBS) have a per-TRX OML Link */
	struct e1inp_sign_link *oml_link;

	struct gsm_abis_mo mo;
	struct tlv_parsed nm_attr;
	struct {
		struct gsm_abis_mo mo;
	} bb_transc;

	uint16_t arfcn;
	int nominal_power;		/* in dBm */
	unsigned int max_power_red;	/* in actual dB */

	union {
		struct {
			struct {
				struct gsm_abis_mo mo;
			} bbsig;
			struct {
				struct gsm_abis_mo mo;
			} pa;
		} bs11;
		struct {
			unsigned int test_state;
			uint8_t test_nr;
			struct rxlev_stats rxlev_stat;
		} ipaccess;
		struct {
			struct {
				struct om2k_mo om2k_mo;
			} trxc;
			struct {
				struct om2k_mo om2k_mo;
			} rx;
			struct {
				struct om2k_mo om2k_mo;
			} tx;
		} rbs2000;
	};
	struct gsm_bts_trx_ts ts[TRX_NR_TS];
};

#define GSM_BTS_SI2Q(bts, i)   (struct gsm48_system_information_type_2quater *)((bts)->si_buf[SYSINFO_TYPE_2quater][i])
#define GSM_BTS_HAS_SI(bts, i) ((bts)->si_valid & (1 << i))
#define GSM_BTS_SI(bts, i)     (void *)((bts)->si_buf[i][0])
#define GSM_LCHAN_SI(lchan, i) (void *)((lchan)->si.buf[i][0])

enum gsm_bts_type {
	GSM_BTS_TYPE_UNKNOWN,
	GSM_BTS_TYPE_BS11,
	GSM_BTS_TYPE_NANOBTS,
	GSM_BTS_TYPE_RBS2000,
	GSM_BTS_TYPE_NOKIA_SITE,
	GSM_BTS_TYPE_OSMOBTS,
	_NUM_GSM_BTS_TYPE
};

enum gsm_bts_type_variant {
	BTS_UNKNOWN,
	BTS_OSMO_LITECELL15,
	BTS_OSMO_OCTPHY,
	BTS_OSMO_SYSMO,
	BTS_OSMO_TRX,
	_NUM_BTS_VARIANT
};

/* Used by OML layer for BTS Attribute reporting */
enum bts_attribute {
	BTS_TYPE_VARIANT,
	BTS_SUB_MODEL,
	TRX_PHY_VERSION,
};

struct vty;

struct gsm_bts_model {
	struct llist_head list;

	enum gsm_bts_type type;
	enum gsm_bts_type_variant variant;
	const char *name;

	bool started;
	int (*start)(struct gsm_network *net);
	int (*oml_rcvmsg)(struct msgb *msg);
	char * (*oml_status)(const struct gsm_bts *bts);

	void (*e1line_bind_ops)(struct e1inp_line *line);

	void (*config_write_bts)(struct vty *vty, struct gsm_bts *bts);
	void (*config_write_trx)(struct vty *vty, struct gsm_bts_trx *trx);
	void (*config_write_ts)(struct vty *vty, struct gsm_bts_trx_ts *ts);

	/* Should SI2bis and SI2ter be disabled by default on this BTS model? */
	bool force_combined_si;

	struct tlv_definition nm_att_tlvdef;

	/* features of a given BTS model set via gsm_bts_model_register() locally */
	struct bitvec features;
	uint8_t _features_data[MAX_BTS_FEATURES/8];
};



/*
 * This keeps track of the paging status of one BTS. It
 * includes a number of pending requests, a back pointer
 * to the gsm_bts, a timer and some more state.
 */
struct gsm_bts_paging_state {
	/* pending requests */
	struct llist_head pending_requests;
	struct gsm_bts *bts;

	struct osmo_timer_list work_timer;
	struct osmo_timer_list credit_timer;

	/* free chans needed */
	int free_chans_need;

	/* load */
	uint16_t available_slots;
};

struct gsm_envabtse {
	struct gsm_abis_mo mo;
};

struct gsm_bts_gprs_nsvc {
	struct gsm_bts *bts;
	/* data read via VTY config file, to configure the BTS
	 * via OML from BSC */
	int id;
	uint16_t nsvci;
	uint16_t local_port;	/* on the BTS */
	uint16_t remote_port;	/* on the SGSN */
	uint32_t remote_ip;	/* on the SGSN */

	struct gsm_abis_mo mo;
};

enum gprs_rlc_par {
	RLC_T3142,
	RLC_T3169,
	RLC_T3191,
	RLC_T3193,
	RLC_T3195,
	RLC_N3101,
	RLC_N3103,
	RLC_N3105,
	CV_COUNTDOWN,
	T_DL_TBF_EXT,	/* ms */
	T_UL_TBF_EXT,	/* ms */
	_NUM_RLC_PAR
};

enum gprs_cs {
	GPRS_CS1,
	GPRS_CS2,
	GPRS_CS3,
	GPRS_CS4,
	GPRS_MCS1,
	GPRS_MCS2,
	GPRS_MCS3,
	GPRS_MCS4,
	GPRS_MCS5,
	GPRS_MCS6,
	GPRS_MCS7,
	GPRS_MCS8,
	GPRS_MCS9,
	_NUM_GRPS_CS
};

struct gprs_rlc_cfg {
	uint16_t parameter[_NUM_RLC_PAR];
	struct {
		uint16_t repeat_time; /* ms */
		uint8_t repeat_count;
	} paging;
	uint32_t cs_mask; /* bitmask of gprs_cs */
	uint8_t initial_cs;
	uint8_t initial_mcs;
};


enum neigh_list_manual_mode {
	NL_MODE_AUTOMATIC = 0,
	NL_MODE_MANUAL = 1,
	NL_MODE_MANUAL_SI5SEP = 2, /* SI2 and SI5 have separate neighbor lists */
};

enum bts_loc_fix {
	BTS_LOC_FIX_INVALID = 0,
	BTS_LOC_FIX_2D = 1,
	BTS_LOC_FIX_3D = 2,
};

extern const struct value_string bts_loc_fix_names[];

struct bts_location {
	struct llist_head list;
	time_t tstamp;
	enum bts_loc_fix valid;
	double lat;
	double lon;
	double height;
};

/* Channel load counter */
struct load_counter {
	unsigned int total;
	unsigned int used;
};

/* Useful to track N-N relations between BTS, for example neighbors. */
struct gsm_bts_ref {
	struct llist_head entry;
	struct gsm_bts *bts;
};

/* A single Page of a SMSCB message */
struct bts_smscb_page {
	/* SMSCB message we're part of */
	struct bts_smscb_message *msg;
	/* Page Number within message (1 to 15) */
	uint8_t nr;
	/* number of valid blocks in data (up to 4) */
	uint8_t num_blocks;
	/* up to four blocks of 22 bytes each */
	uint8_t data[88];
};

/* A SMSCB message (received from CBSP) */
struct bts_smscb_message {
	/* entry in bts_smscb_chan_state.messages */
	struct llist_head list;
	struct {
		/* input data from CBSP (CBC) side */
		uint16_t msg_id;
		uint16_t serial_nr;
		enum cbsp_category category;
		uint16_t rep_period;
		uint16_t num_bcast_req;
		uint8_t dcs;
	} input;
	/* how often have all pages of this message been broadcast? */
	uint32_t bcast_count;
	/* actual page data of this message */
	uint8_t num_pages; /* up to 15 */
	struct bts_smscb_page page[15];
};

/* per-channel (basic/extended) CBCH state for a single BTS */
struct bts_smscb_chan_state {
	/* back-pointer to BTS */
	struct gsm_bts *bts;
	/* list of bts_smscb_message */
	struct llist_head messages;
	/* scheduling array; pointer of SMSCB pages */
	struct bts_smscb_page **sched_arr;
	size_t sched_arr_size;
	/* index of the next to be transmitted page into the scheduler array */
	size_t next_idx;
	/* number of messages we have to pause due to overflow */
	uint8_t overflow;
};

struct bts_oml_fail_rep {
	struct llist_head list;
	time_t time;
	struct msgb *mb;
};

/* One BTS */
struct gsm_bts {
	/* list header in net->bts_list */
	struct llist_head list;

	/* Geographical location of the BTS */
	struct llist_head loc_list;

	/* number of this BTS in network */
	uint8_t nr;
	/* human readable name / description */
	char *description;
	/* Cell Identity */
	uint16_t cell_identity;
	/* location area code of this BTS */
	uint16_t location_area_code;
	/* Base Station Identification Code (BSIC), lower 3 bits is BCC,
	 * which is used as TSC for the CCCH */
	uint8_t bsic;
	/* type of BTS */
	enum gsm_bts_type type;
	enum gsm_bts_type_variant variant;
	struct gsm_bts_model *model;
	enum gsm_band band;
	char version[MAX_VERSION_LENGTH];
	char sub_model[MAX_VERSION_LENGTH];

	/* features of a given BTS set/reported via OML */
	struct bitvec features;
	uint8_t _features_data[MAX_BTS_FEATURES/8];

	/* Connected PCU version (if any) */
	char pcu_version[MAX_VERSION_LENGTH];

	/* maximum Tx power that the MS is permitted to use in this cell */
	int ms_max_power;

	/* how do we talk OML with this TRX? */
	struct gsm_e1_subslot oml_e1_link;
	uint8_t oml_tei;
	struct e1inp_sign_link *oml_link;
	/* Timer to use for deferred drop of OML link, see \ref ipaccess_drop_oml_deferred */
	struct osmo_timer_list oml_drop_link_timer;
	/* when OML link was established */
	time_t uptime;

	/* Abis network management O&M handle */
	struct abis_nm_h *nmh;

	struct gsm_abis_mo mo;

	/* number of this BTS on given E1 link */
	uint8_t bts_nr;

	/* DTX features of this BTS */
	enum gsm48_dtx_mode dtxu;
	bool dtxd;

	/* paging state and control */
	struct gsm_bts_paging_state paging;

	/* CCCH is on C0 */
	struct gsm_bts_trx *c0;

	struct {
		struct gsm_abis_mo mo;
	} site_mgr;

	/* bitmask of all SI that are present/valid in si_buf */
	uint32_t si_valid;
	/* 3GPP TS 44.018 Table 10.5.2.33b.1 INDEX and COUNT for SI2quater */
	uint8_t si2q_index; /* distinguish individual SI2quater messages */
	uint8_t si2q_count; /* si2q_index for the last (highest indexed) individual SI2quater message */
	/* buffers where we put the pre-computed SI */
	sysinfo_buf_t si_buf[_MAX_SYSINFO_TYPE][SI2Q_MAX_NUM];
	/* offsets used while generating SI2quater */
	size_t e_offset;
	size_t u_offset;
	/* 3GPP TS 08.58 §8.5.1 BCCH INFORMATION. Some nanoBTS fail upon
	 * receival of empty SI disabling unsupported SI. see OS#3707. */
	bool si_unused_send_empty;

	/* ip.access Unit ID's have Site/BTS/TRX layout */
	union {
		struct {
			uint16_t site_id;
			uint16_t bts_id;
			uint32_t flags;
			uint32_t rsl_ip;
		} ip_access;
		struct {
			struct {
				struct gsm_abis_mo mo;
			} cclk;
			struct {
				struct gsm_abis_mo mo;
			} rack;
			struct gsm_envabtse envabtse[4];
		} bs11;
		struct {
			struct {
				struct om2k_mo om2k_mo;
				struct gsm_abis_mo mo;
				struct llist_head conn_groups;
			} cf;
			struct {
				struct om2k_mo om2k_mo;
				struct gsm_abis_mo mo;
				struct llist_head conn_groups;
			} is;
			struct {
				struct om2k_mo om2k_mo;
				struct gsm_abis_mo mo;
				struct llist_head conn_groups;
			} con;
			struct {
				struct om2k_mo om2k_mo;
				struct gsm_abis_mo mo;
			} dp;
			struct {
				struct om2k_mo om2k_mo;
				struct gsm_abis_mo mo;
			} tf;
			struct {
				struct om2k_mo om2k_mo;
				struct gsm_abis_mo mo;
			} mctr;
			uint32_t use_superchannel:1;
			struct {
				uint16_t limit;
				uint16_t active;
			} om2k_version[16];
		} rbs2000;
		struct {
			uint8_t bts_type;
			unsigned int configured:1,	/* we sent the config data request */
				skip_reset:1,		/* skip reset at bootstrap */
				no_loc_rel_cnf:1,	/* don't wait for RSL REL CONF */
				bts_reset_timer_cnf,	/* timer for BTS RESET */
				did_reset:1,		/* we received a RESET ACK */
				wait_reset:2;		/* we are waiting for reset to complete */
			struct osmo_timer_list reset_timer;
		} nokia;
	};

	/* Not entirely sure how ip.access specific this is */
	struct {
		enum bts_gprs_mode mode;
		struct {
			struct gsm_abis_mo mo;
			uint16_t nsei;
			uint8_t timer[7];
		} nse;
		struct {
			struct gsm_abis_mo mo;
			uint16_t bvci;
			uint8_t timer[11];
			struct gprs_rlc_cfg rlc_cfg;
		} cell;
		struct gsm_bts_gprs_nsvc nsvc[2];
		uint8_t rac;
		uint8_t net_ctrl_ord;
		bool ctrl_ack_type_use_block;
		bool egprs_pkt_chan_request;
	} gprs;

	/* threshold (in percent) when BTS shall send CCCH LOAD IND */
	int ccch_load_ind_thresh;

	/* RACH NM values */
	int rach_b_thresh;
	int rach_ldavg_slots;

	/* transceivers */
	int num_trx;
	struct llist_head trx_list;

	/* SI related items */
	int force_combined_si;
	bool force_combined_si_set;
	int bcch_change_mark;

	/* Abis NM queue */
	struct llist_head abis_queue;
	int abis_nm_pend;

	struct gsm_network *network;

	/* should the channel allocator allocate channels from high TRX to TRX0,
	 * rather than starting from TRX0 and go upwards? */
	int chan_alloc_reverse;

	enum neigh_list_manual_mode neigh_list_manual_mode;
	/* parameters from which we build SYSTEM INFORMATION */
	struct {
		struct gsm48_rach_control rach_control;
		uint8_t ncc_permitted;
		struct gsm48_cell_sel_par cell_sel_par;
		struct gsm48_si_selection_params cell_ro_sel_par; /* rest octet */
		struct gsm48_cell_options cell_options;
		struct gsm48_control_channel_descr chan_desc;
		struct bitvec neigh_list;
		struct bitvec cell_alloc;
		struct bitvec si5_neigh_list;
		struct osmo_earfcn_si2q si2quater_neigh_list;
		size_t uarfcn_length; /* index for uarfcn and scramble lists */
		struct {
			/* bitmask large enough for all possible ARFCN's */
			uint8_t neigh_list[1024/8];
			uint8_t cell_alloc[1024/8];
			/* If the user wants a different neighbor list in SI5 than in SI2 */
			uint8_t si5_neigh_list[1024/8];
			uint8_t meas_bw_list[MAX_EARFCN_LIST];
			uint16_t earfcn_list[MAX_EARFCN_LIST];
			uint16_t uarfcn_list[MAX_EARFCN_LIST];
			uint16_t scramble_list[MAX_EARFCN_LIST];
		} data;
	} si_common;
	bool early_classmark_allowed;
	bool early_classmark_allowed_3g;
	/* for testing only: Have an infinitely long radio link timeout */
	bool infinite_radio_link_timeout;

	/* do we use static (user-defined) system information messages? (bitmask) */
	uint32_t si_mode_static;

	/* access control class ramping */
	struct acc_ramp acc_ramp;

	/* exclude the BTS from the global RF Lock handling */
	int excl_from_rf_lock;

	/* supported codecs beside FR */
	struct bts_codec_conf codec;

	/* BTS dependencies bit field */
	uint32_t depends_on[256/(8*4)];

	/* full and half rate multirate config */
	struct amr_multirate_conf mr_full;
	struct amr_multirate_conf mr_half;

	/* PCU socket state */
	char *pcu_sock_path;
	struct pcu_sock_state *pcu_state;

	struct rate_ctr_group *bts_ctrs;
	struct osmo_stat_item_group *bts_statg;

	struct handover_cfg *ho;

	/* A list of struct gsm_bts_ref, indicating neighbors of this BTS.
	 * When the si_common neigh_list is in automatic mode, it is populated from this list as well as
	 * gsm_network->neighbor_bss_cells. */
	struct llist_head local_neighbors;

	/* BTS-specific overrides for timer values from struct gsm_network. */
	uint8_t T3122;	/* ASSIGNMENT REJECT wait indication */
	bool T3113_dynamic; /* Calculate T3113 timeout dynamically based on BTS channel config and load */

	/* Periodic channel load measurements are used to maintain T3122. */
	struct load_counter chan_load_samples[7];
	int chan_load_samples_idx;
	uint8_t chan_load_avg; /* current channel load average in percent (0 - 100). */

	/* cell broadcast system */
	struct osmo_timer_list cbch_timer;
	struct bts_smscb_chan_state cbch_basic;
	struct bts_smscb_chan_state cbch_extended;
	struct osmo_timer_list etws_timer;	/* when to stop ETWS PN */

	struct llist_head oml_fail_rep;
};

/* One rejected BTS */
struct gsm_bts_rejected {
	/* list header in net->bts_rejected */
	struct llist_head list;

	uint16_t site_id;
	uint16_t bts_id;
	char ip[INET6_ADDRSTRLEN];
	time_t time;
};

struct gsm_network *gsm_network_init(void *ctx);

struct gsm_bts *gsm_bts_alloc(struct gsm_network *net, uint8_t bts_num);
struct gsm_bts *gsm_bts_num(const struct gsm_network *net, int num);
bool gsm_bts_matches_lai(const struct gsm_bts *bts, const struct osmo_location_area_id *lai);
bool gsm_bts_matches_cell_id(const struct gsm_bts *bts, const struct gsm0808_cell_id *cell_id);
struct gsm_bts *gsm_bts_by_cell_id(const struct gsm_network *net,
				   const struct gsm0808_cell_id *cell_id,
				   int match_idx);
int gsm_bts_local_neighbor_add(struct gsm_bts *bts, struct gsm_bts *neighbor);
int gsm_bts_local_neighbor_del(struct gsm_bts *bts, const struct gsm_bts *neighbor);

struct gsm_bts_trx *gsm_bts_trx_alloc(struct gsm_bts *bts);
struct gsm_bts_trx *gsm_bts_trx_num(const struct gsm_bts *bts, int num);

enum gsm_bts_type str2btstype(const char *arg);
const char *btstype2str(enum gsm_bts_type type);

enum bts_attribute str2btsattr(const char *s);
const char *btsatttr2str(enum bts_attribute v);

enum gsm_bts_type_variant str2btsvariant(const char *arg);
const char *btsvariant2str(enum gsm_bts_type_variant v);

extern const struct value_string gsm_chreq_descs[];
extern const struct value_string gsm_pchant_names[];
extern const struct value_string gsm_pchant_descs[];
extern const struct value_string gsm_pchan_ids[];
const char *gsm_pchan_name(enum gsm_phys_chan_config c);
static inline const char *gsm_pchan_id(enum gsm_phys_chan_config c)
{ return get_value_string(gsm_pchan_ids, c); }
enum gsm_phys_chan_config gsm_pchan_parse(const char *name);
const char *gsm_lchant_name(enum gsm_chan_t c);
const char *gsm_chreq_name(enum gsm_chreq_reason_t c);
char *gsm_bts_name(const struct gsm_bts *bts);
char *gsm_trx_name(const struct gsm_bts_trx *trx);
char *gsm_ts_name(const struct gsm_bts_trx_ts *ts);
char *gsm_ts_and_pchan_name(const struct gsm_bts_trx_ts *ts);
char *gsm_lchan_name_compute(const struct gsm_lchan *lchan);

static inline char *gsm_lchan_name(const struct gsm_lchan *lchan)
{
	return lchan->name;
}

void gsm_abis_mo_reset(struct gsm_abis_mo *mo);

struct gsm_nm_state *
gsm_objclass2nmstate(struct gsm_bts *bts, uint8_t obj_class,
		 const struct abis_om_obj_inst *obj_inst);
void *
gsm_objclass2obj(struct gsm_bts *bts, uint8_t obj_class,
	     const struct abis_om_obj_inst *obj_inst);

/* reset the state of all MO in the BTS */
void gsm_bts_mo_reset(struct gsm_bts *bts);

uint8_t gsm_pchan2chan_nr(enum gsm_phys_chan_config pchan,
			  uint8_t ts_nr, uint8_t lchan_nr);
uint8_t gsm_lchan2chan_nr(const struct gsm_lchan *lchan);
uint8_t gsm_lchan_as_pchan2chan_nr(const struct gsm_lchan *lchan,
				   enum gsm_phys_chan_config as_pchan);

void gsm48_lchan2chan_desc(struct gsm48_chan_desc *cd,
			   const struct gsm_lchan *lchan);
void gsm48_lchan2chan_desc_as_configured(struct gsm48_chan_desc *cd, const struct gsm_lchan *lchan);

/* return the gsm_lchan for the CBCH (if it exists at all) */
struct gsm_lchan *gsm_bts_get_cbch(struct gsm_bts *bts);

static inline uint8_t gsm_ts_tsc(const struct gsm_bts_trx_ts *ts)
{
	if (ts->tsc != -1)
		return ts->tsc;
	else
		return ts->trx->bts->bsic & 7;
}

struct gsm_lchan *rsl_lchan_lookup(struct gsm_bts_trx *trx, uint8_t chan_nr,
				   int *rc);

enum gsm_phys_chan_config ts_pchan(struct gsm_bts_trx_ts *ts);
uint8_t pchan_subslots(enum gsm_phys_chan_config pchan);
bool ts_is_tch(struct gsm_bts_trx_ts *ts);


static inline struct gsm_bts *conn_get_bts(struct gsm_subscriber_connection *conn) {
	OSMO_ASSERT(conn->lchan);
	return conn->lchan->ts->trx->bts;
}

void conn_update_ms_power_class(struct gsm_subscriber_connection *conn, uint8_t power_class);
void lchan_update_ms_power_ctrl_level(struct gsm_lchan *lchan, int ms_power_dbm);

enum bts_counter_id {
	BTS_CTR_CHREQ_TOTAL,
	BTS_CTR_CHREQ_NO_CHANNEL,
	BTS_CTR_CHAN_RF_FAIL,
	BTS_CTR_CHAN_RLL_ERR,
	BTS_CTR_BTS_OML_FAIL,
	BTS_CTR_BTS_RSL_FAIL,
	BTS_CTR_CODEC_AMR_F,
	BTS_CTR_CODEC_AMR_H,
	BTS_CTR_CODEC_EFR,
	BTS_CTR_CODEC_V1_FR,
	BTS_CTR_CODEC_V1_HR,
	BTS_CTR_PAGING_ATTEMPTED,
	BTS_CTR_PAGING_ALREADY,
	BTS_CTR_PAGING_RESPONDED,
	BTS_CTR_PAGING_EXPIRED,
	BTS_CTR_PAGING_NO_ACTIVE_PAGING,
	BTS_CTR_PAGING_MSC_FLUSH,
	BTS_CTR_CHAN_ACT_TOTAL,
	BTS_CTR_CHAN_ACT_NACK,
	BTS_CTR_RSL_UNKNOWN,
	BTS_CTR_RSL_IPA_NACK,
	BTS_CTR_MODE_MODIFY_NACK,
	BTS_CTR_LCHAN_BORKEN_FROM_UNUSED,
	BTS_CTR_LCHAN_BORKEN_FROM_WAIT_ACTIV_ACK,
	BTS_CTR_LCHAN_BORKEN_FROM_WAIT_RF_RELEASE_ACK,
	BTS_CTR_LCHAN_BORKEN_FROM_BORKEN,
	BTS_CTR_LCHAN_BORKEN_FROM_UNKNOWN,
	BTS_CTR_LCHAN_BORKEN_EV_CHAN_ACTIV_ACK,
	BTS_CTR_LCHAN_BORKEN_EV_CHAN_ACTIV_NACK,
	BTS_CTR_LCHAN_BORKEN_EV_RF_CHAN_REL_ACK,
	BTS_CTR_LCHAN_BORKEN_EV_VTY,
	BTS_CTR_LCHAN_BORKEN_EV_TEARDOWN,
	BTS_CTR_TS_BORKEN_FROM_NOT_INITIALIZED,
	BTS_CTR_TS_BORKEN_FROM_UNUSED,
	BTS_CTR_TS_BORKEN_FROM_WAIT_PDCH_ACT,
	BTS_CTR_TS_BORKEN_FROM_PDCH,
	BTS_CTR_TS_BORKEN_FROM_WAIT_PDCH_DEACT,
	BTS_CTR_TS_BORKEN_FROM_IN_USE,
	BTS_CTR_TS_BORKEN_FROM_BORKEN,
	BTS_CTR_TS_BORKEN_FROM_UNKNOWN,
	BTS_CTR_TS_BORKEN_EV_PDCH_ACT_ACK_NACK,
	BTS_CTR_TS_BORKEN_EV_PDCH_DEACT_ACK_NACK,
	BTS_CTR_TS_BORKEN_EV_TEARDOWN,
};

static const struct rate_ctr_desc bts_ctr_description[] = {
	[BTS_CTR_CHREQ_TOTAL] = 		{"chreq:total", "Received channel requests."},
	[BTS_CTR_CHREQ_NO_CHANNEL] = 		{"chreq:no_channel", "Sent to MS no channel available."},
	[BTS_CTR_CHAN_RF_FAIL] = 		{"chan:rf_fail", "Received a RF failure indication from BTS."},
	[BTS_CTR_CHAN_RLL_ERR] = 		{"chan:rll_err", "Received a RLL failure with T200 cause from BTS."},
	[BTS_CTR_BTS_OML_FAIL] = 		{"oml_fail", "Received a TEI down on a OML link."},
	[BTS_CTR_BTS_RSL_FAIL] = 		{"rsl_fail", "Received a TEI down on a OML link."},
	[BTS_CTR_CODEC_AMR_F] =			{"codec:amr_f", "Count the usage of AMR/F codec by channel mode requested."},
	[BTS_CTR_CODEC_AMR_H] =			{"codec:amr_h", "Count the usage of AMR/H codec by channel mode requested."},
	[BTS_CTR_CODEC_EFR] = 			{"codec:efr", "Count the usage of EFR codec by channel mode requested."},
	[BTS_CTR_CODEC_V1_FR] =			{"codec:fr", "Count the usage of FR codec by channel mode requested."},
	[BTS_CTR_CODEC_V1_HR] =			{"codec:hr", "Count the usage of HR codec by channel mode requested."},

	[BTS_CTR_PAGING_ATTEMPTED] = 		{"paging:attempted", "Paging attempts for a subscriber."},
	[BTS_CTR_PAGING_ALREADY] = 		{"paging:already", "Paging attempts ignored as subscriber was already being paged."},
	[BTS_CTR_PAGING_RESPONDED] = 		{"paging:responded", "Paging attempts with successful paging response."},
	[BTS_CTR_PAGING_EXPIRED] = 		{"paging:expired", "Paging Request expired because of timeout T3113."},
	[BTS_CTR_PAGING_NO_ACTIVE_PAGING] =	{"paging:no_active_paging", "Paging response without an active paging request (arrived after paging expiration?)."},
	[BTS_CTR_PAGING_MSC_FLUSH] =		{"paging:msc_flush", "Paging flushed due to MSC Reset BSSMAP message."},
	[BTS_CTR_CHAN_ACT_TOTAL] =		{"chan_act:total", "Total number of Channel Activations."},
	[BTS_CTR_CHAN_ACT_NACK] =		{"chan_act:nack", "Number of Channel Activations that the BTS NACKed"},
	[BTS_CTR_RSL_UNKNOWN] =			{"rsl:unknown", "Number of unknown/unsupported RSL messages received from BTS"},
	[BTS_CTR_RSL_IPA_NACK] =		{"rsl:ipa_nack", "Number of IPA (RTP/dyn-PDCH) related NACKs received from BTS"},
	[BTS_CTR_MODE_MODIFY_NACK] =		{"chan:mode_modify_nack", "Number of Channel Mode Modify NACKs received from BTS"},

	/* lchan/TS BORKEN state counters */
	[BTS_CTR_LCHAN_BORKEN_FROM_UNUSED] =              {"lchan_borken:from_state:unused", "Transitions from lchan UNUSED state to BORKEN state"},
	[BTS_CTR_LCHAN_BORKEN_FROM_WAIT_ACTIV_ACK] =      {"lchan_borken:from_state:wait_activ_ack", "Transitions from lchan WAIT_ACTIV_ACK state to BORKEN state"},
	[BTS_CTR_LCHAN_BORKEN_FROM_WAIT_RF_RELEASE_ACK] = {"lchan_borken:from_state:wait_rf_release_ack", "Transitions from lchan WAIT_RF_RELEASE_ACK state to BORKEN state"},
	[BTS_CTR_LCHAN_BORKEN_FROM_BORKEN] =              {"lchan_borken:from_state:borken", "Transitions from lchan BORKEN state to BORKEN state"},
	[BTS_CTR_LCHAN_BORKEN_FROM_UNKNOWN] =             {"lchan_borken:from_state:unknown", "Transitions from an unknown lchan state to BORKEN state"},
	[BTS_CTR_LCHAN_BORKEN_EV_CHAN_ACTIV_ACK] =        {"lchan_borken:event:chan_activ_ack", "CHAN_ACTIV_ACK received in the lchan BORKEN state"},
	[BTS_CTR_LCHAN_BORKEN_EV_CHAN_ACTIV_NACK] =       {"lchan_borken:event:chan_activ_nack", "CHAN_ACTIV_NACK received in the lchan BORKEN state"},
	[BTS_CTR_LCHAN_BORKEN_EV_RF_CHAN_REL_ACK] =       {"lchan_borken:event:rf_chan_rel_ack", "RF_CHAN_REL_ACK received in the lchan BORKEN state"},
	[BTS_CTR_LCHAN_BORKEN_EV_VTY] =                   {"lchan_borken:event:vty", "VTY commands received in the lchan BORKEN state"},
	[BTS_CTR_LCHAN_BORKEN_EV_TEARDOWN] =              {"lchan_borken:event:teardown", "lchan in a BORKEN state is shutting down (BTS disconnected?)"},
	[BTS_CTR_TS_BORKEN_FROM_NOT_INITIALIZED] =   {"ts_borken:from_state:not_initialized", "Transitions from TS NOT_INITIALIZED state to BORKEN state"},
	[BTS_CTR_TS_BORKEN_FROM_UNUSED] =            {"ts_borken:from_state:unused", "Transitions from TS UNUSED state to BORKEN state"},
	[BTS_CTR_TS_BORKEN_FROM_WAIT_PDCH_ACT] =     {"ts_borken:from_state:wait_pdch_act", "Transitions from TS WAIT_PDCH_ACT state to BORKEN state"},
	[BTS_CTR_TS_BORKEN_FROM_PDCH] =              {"ts_borken:from_state:pdch", "Transitions from TS PDCH state to BORKEN state"},
	[BTS_CTR_TS_BORKEN_FROM_WAIT_PDCH_DEACT] =   {"ts_borken:from_state:wait_pdch_deact", "Transitions from TS WAIT_PDCH_DEACT state to BORKEN state"},
	[BTS_CTR_TS_BORKEN_FROM_IN_USE] =            {"ts_borken:from_state:in_use", "Transitions from TS IN_USE state to BORKEN state"},
	[BTS_CTR_TS_BORKEN_FROM_BORKEN] =            {"ts_borken:from_state:borken", "Transitions from TS BORKEN state to BORKEN state"},
	[BTS_CTR_TS_BORKEN_FROM_UNKNOWN] =           {"ts_borken:from_state:unknown", "Transitions from an unknown TS state to BORKEN state"},
	[BTS_CTR_TS_BORKEN_EV_PDCH_ACT_ACK_NACK] =   {"ts_borken:event:pdch_act_ack_nack", "PDCH_ACT_ACK/NACK received in the TS BORKEN state"},
	[BTS_CTR_TS_BORKEN_EV_PDCH_DEACT_ACK_NACK] = {"ts_borken:event:pdch_deact_ack_nack", "PDCH_DEACT_ACK/NACK received in the TS BORKEN state"},
	[BTS_CTR_TS_BORKEN_EV_TEARDOWN] =            {"ts_borken:event:teardown", "TS in a BORKEN state is shutting down (BTS disconnected?)"},
};

static const struct rate_ctr_group_desc bts_ctrg_desc = {
	"bts",
	"base transceiver station",
	OSMO_STATS_CLASS_GLOBAL,
	ARRAY_SIZE(bts_ctr_description),
	bts_ctr_description,
};

enum {
	BTS_STAT_CHAN_LOAD_AVERAGE,
	BTS_STAT_CHAN_CCCH_SDCCH4_USED,
	BTS_STAT_CHAN_CCCH_SDCCH4_TOTAL,
	BTS_STAT_CHAN_TCH_F_USED,
	BTS_STAT_CHAN_TCH_F_TOTAL,
	BTS_STAT_CHAN_TCH_H_USED,
	BTS_STAT_CHAN_TCH_H_TOTAL,
	BTS_STAT_CHAN_SDCCH8_USED,
	BTS_STAT_CHAN_SDCCH8_TOTAL,
	BTS_STAT_CHAN_TCH_F_PDCH_USED,
	BTS_STAT_CHAN_TCH_F_PDCH_TOTAL,
	BTS_STAT_CHAN_CCCH_SDCCH4_CBCH_USED,
	BTS_STAT_CHAN_CCCH_SDCCH4_CBCH_TOTAL,
	BTS_STAT_CHAN_SDCCH8_CBCH_USED,
	BTS_STAT_CHAN_SDCCH8_CBCH_TOTAL,
	BTS_STAT_CHAN_TCH_F_TCH_H_PDCH_USED,
	BTS_STAT_CHAN_TCH_F_TCH_H_PDCH_TOTAL,
	BTS_STAT_T3122,
	BTS_STAT_RACH_BUSY,
	BTS_STAT_RACH_ACCESS,
	BTS_STAT_OML_CONNECTED,
	BTS_STAT_RSL_CONNECTED,
	BTS_STAT_LCHAN_BORKEN,
	BTS_STAT_TS_BORKEN,
};

enum {
	BSC_CTR_ASSIGNMENT_ATTEMPTED,
	BSC_CTR_ASSIGNMENT_COMPLETED,
	BSC_CTR_ASSIGNMENT_STOPPED,
	BSC_CTR_ASSIGNMENT_NO_CHANNEL,
	BSC_CTR_ASSIGNMENT_TIMEOUT,
	BSC_CTR_ASSIGNMENT_FAILED,
	BSC_CTR_ASSIGNMENT_ERROR,
	BSC_CTR_HANDOVER_ATTEMPTED,
	BSC_CTR_HANDOVER_COMPLETED,
	BSC_CTR_HANDOVER_STOPPED,
	BSC_CTR_HANDOVER_NO_CHANNEL,
	BSC_CTR_HANDOVER_TIMEOUT,
	BSC_CTR_HANDOVER_FAILED,
	BSC_CTR_HANDOVER_ERROR,
	BSC_CTR_INTER_BSC_HO_OUT_ATTEMPTED,
	BSC_CTR_INTER_BSC_HO_OUT_COMPLETED,
	BSC_CTR_INTER_BSC_HO_OUT_STOPPED,
	BSC_CTR_INTER_BSC_HO_OUT_TIMEOUT,
	BSC_CTR_INTER_BSC_HO_OUT_ERROR,
	BSC_CTR_INTER_BSC_HO_IN_ATTEMPTED,
	BSC_CTR_INTER_BSC_HO_IN_COMPLETED,
	BSC_CTR_INTER_BSC_HO_IN_STOPPED,
	BSC_CTR_INTER_BSC_HO_IN_NO_CHANNEL,
	BSC_CTR_INTER_BSC_HO_IN_FAILED,
	BSC_CTR_INTER_BSC_HO_IN_TIMEOUT,
	BSC_CTR_INTER_BSC_HO_IN_ERROR,
	BSC_CTR_PAGING_ATTEMPTED,
	BSC_CTR_PAGING_DETACHED,
	BSC_CTR_PAGING_RESPONDED,
	BSC_CTR_PAGING_NO_ACTIVE_PAGING,
	BSC_CTR_UNKNOWN_UNIT_ID,
};

static const struct rate_ctr_desc bsc_ctr_description[] = {
	[BSC_CTR_ASSIGNMENT_ATTEMPTED] =	{"assignment:attempted", "Assignment attempts."},
	[BSC_CTR_ASSIGNMENT_COMPLETED] =	{"assignment:completed", "Assignment completed."},
	[BSC_CTR_ASSIGNMENT_STOPPED] = 		{"assignment:stopped", "Connection ended during Assignment."},
	[BSC_CTR_ASSIGNMENT_NO_CHANNEL] = 	{"assignment:no_channel", "Failure to allocate lchan for Assignment."},
	[BSC_CTR_ASSIGNMENT_TIMEOUT] = 		{"assignment:timeout", "Assignment timed out."},
	[BSC_CTR_ASSIGNMENT_FAILED] = 		{"assignment:failed", "Received Assignment Failure message."},
	[BSC_CTR_ASSIGNMENT_ERROR] = 		{"assignment:error", "Assignment failed for other reason."},

	[BSC_CTR_HANDOVER_ATTEMPTED] = 		{"handover:attempted", "Intra-BSC handover attempts."},
	[BSC_CTR_HANDOVER_COMPLETED] = 		{"handover:completed", "Intra-BSC handover completed."},
	[BSC_CTR_HANDOVER_STOPPED] = 		{"handover:stopped", "Connection ended during HO."},
	[BSC_CTR_HANDOVER_NO_CHANNEL] = 	{"handover:no_channel", "Failure to allocate lchan for HO."},
	[BSC_CTR_HANDOVER_TIMEOUT] = 		{"handover:timeout", "Handover timed out."},
	[BSC_CTR_HANDOVER_FAILED] = 		{"handover:failed", "Received Handover Fail messages."},
	[BSC_CTR_HANDOVER_ERROR] = 		{"handover:error", "Re-assignment failed for other reason."},

	[BSC_CTR_INTER_BSC_HO_OUT_ATTEMPTED] =	{"interbsc_ho_out:attempted",
						 "Attempts to handover to remote BSS."},
	[BSC_CTR_INTER_BSC_HO_OUT_COMPLETED] =	{"interbsc_ho_out:completed",
						 "Handover to remote BSS completed."},
	[BSC_CTR_INTER_BSC_HO_OUT_STOPPED] =	{"interbsc_ho_out:stopped", "Connection ended during HO."},
	[BSC_CTR_INTER_BSC_HO_OUT_TIMEOUT] =	{"interbsc_ho_out:timeout", "Handover timed out."},
	[BSC_CTR_INTER_BSC_HO_OUT_ERROR] =	{"interbsc_ho_out:error",
						 "Handover to remote BSS failed for other reason."},

	[BSC_CTR_INTER_BSC_HO_IN_ATTEMPTED] =	{"interbsc_ho_in:attempted",
						 "Attempts to handover from remote BSS."},
	[BSC_CTR_INTER_BSC_HO_IN_COMPLETED] =	{"interbsc_ho_in:completed",
						 "Handover from remote BSS completed."},
	[BSC_CTR_INTER_BSC_HO_IN_STOPPED] =	{"interbsc_ho_in:stopped", "Connection ended during HO."},
	[BSC_CTR_INTER_BSC_HO_IN_NO_CHANNEL] =	{"interbsc_ho_in:no_channel",
						 "Failure to allocate lchan for HO."},
	[BSC_CTR_INTER_BSC_HO_IN_TIMEOUT] =	{"interbsc_ho_in:timeout", "Handover from remote BSS timed out."},
	[BSC_CTR_INTER_BSC_HO_IN_FAILED] =	{"interbsc_ho_in:failed", "Received Handover Fail message."},
	[BSC_CTR_INTER_BSC_HO_IN_ERROR] =	{"interbsc_ho_in:error",
						 "Handover from remote BSS failed for other reason."},

	[BSC_CTR_PAGING_ATTEMPTED] = 		{"paging:attempted", "Paging attempts for a subscriber."},
	[BSC_CTR_PAGING_DETACHED] = 		{"paging:detached", "Paging request send failures because no responsible BTS was found."},
	[BSC_CTR_PAGING_RESPONDED] = 		{"paging:responded", "Paging attempts with successful response."},
	[BSC_CTR_PAGING_NO_ACTIVE_PAGING] =	{"paging:no_active_paging", "Paging response without an active paging request (arrived after paging expiration?)."},

	[BSC_CTR_UNKNOWN_UNIT_ID] = 		{"abis:unknown_unit_id", "Connection attempts from unknown IPA CCM Unit ID."},
};



static const struct rate_ctr_group_desc bsc_ctrg_desc = {
	"bsc",
	"base station controller",
	OSMO_STATS_CLASS_GLOBAL,
	ARRAY_SIZE(bsc_ctr_description),
	bsc_ctr_description,
};

/* Constants for the BSC stats */
enum {
	BSC_STAT_NUM_BTS_TOTAL,
};

struct gsm_tz {
	int override; /* if 0, use system's time zone instead. */
	int hr; /* hour */
	int mn; /* minute */
	int dst; /* daylight savings */
};

struct gsm_network {
	/* TODO MSCSPLIT the gsm_network struct is basically a kitchen sink for
	 * global settings and variables, "madly" mixing BSC and MSC stuff. Split
	 * this in e.g. struct osmo_bsc and struct osmo_msc, with the things
	 * these have in common, like country and network code, put in yet
	 * separate structs and placed as members in osmo_bsc and osmo_msc. */

	struct osmo_plmn_id plmn;

	/* bit-mask of permitted encryption algorithms. LSB=A5/0, MSB=A5/7 */
	uint8_t a5_encryption_mask;
	int neci;

	struct handover_cfg *ho;
	struct {
		unsigned int congestion_check_interval_s;
		struct osmo_timer_list congestion_check_timer;
	} hodec2;

	/* structures for keeping rate counters and gauge stats */
	struct rate_ctr_group *bsc_ctrs;
	struct osmo_stat_item_group *bsc_statg;

	unsigned int num_bts;
	struct llist_head bts_list;
	struct llist_head bts_rejected;

	/* see gsm_network_T_defs */
	struct osmo_tdef *T_defs;

	enum gsm_chan_t ctype_by_chreq[_NUM_CHREQ_T];

	/* Use a TCH for handling requests of type paging any */
	int pag_any_tch;

	/* msc configuration */
	struct llist_head mscs;
	uint8_t mscs_round_robin_next_nr;
	/* Emergency calls potentially select a different set of MSCs, so to not mess up the normal round-robin
	 * behavior, emergency calls need a separate round-robin counter. */
	uint8_t mscs_round_robin_next_emerg_nr;

	/* rf ctl related bits */
	int mid_call_timeout;
	char *rf_ctrl_name;
	struct osmo_bsc_rf *rf_ctrl;
	int auto_off_timeout;

	struct bsc_cbc_link *cbc;

	/* control interface */
	struct ctrl_handle *ctrl;

	/* Allow or disallow TCH/F on dynamic TCH/F_TCH/H_PDCH; OS#1778 */
	bool dyn_ts_allow_tch_f;

	/* all active subscriber connections. */
	struct llist_head subscr_conns;

	/* if override is nonzero, this timezone data is used for all MM
	 * contexts. */
	/* TODO: in OsmoNITB, tz-override used to be BTS-specific. To enable
	 * BTS|RNC specific timezone overrides for multi-tz networks in
	 * OsmoMSC, this should be tied to the location area code (LAC). */
	struct gsm_tz tz;

	/* List of all struct bsc_subscr used in libbsc. This llist_head is
	 * allocated so that the llist_head pointer itself can serve as a
	 * talloc context (useful to not have to pass the entire gsm_network
	 * struct to the bsc_subscr_* API, and for bsc_susbscr unit tests to
	 * not require gsm_data.h). In an MSC-without-BSC environment, this
	 * pointer is NULL to indicate absence of a bsc_subscribers list. */
	struct llist_head *bsc_subscribers;

	/* Timer for periodic channel load measurements to maintain each BTS's T3122. */
	struct osmo_timer_list t3122_chan_load_timer;

	struct {
		struct mgcp_client_conf *conf;
		struct mgcp_client *client;
		struct osmo_tdef *tdefs;
	} mgw;

	/* Remote BSS Cell Identifier Lists */
	struct neighbor_ident_list *neighbor_bss_cells;

	/* Don't refuse to start with mutually exclusive codec settings */
	bool allow_unusable_timeslots;
};

struct gsm_audio_support {
        uint8_t hr  : 1,
                ver : 7;
};

static inline const struct osmo_location_area_id *bts_lai(struct gsm_bts *bts)
{
	static struct osmo_location_area_id lai;
	lai = (struct osmo_location_area_id){
		.plmn = bts->network->plmn,
		.lac = bts->location_area_code,
	};
	return &lai;
}

extern void talloc_ctx_init(void *ctx_root);

int gsm_set_bts_type(struct gsm_bts *bts, enum gsm_bts_type type);

enum gsm_bts_type parse_btstype(const char *arg);
const char *btstype2str(enum gsm_bts_type type);
struct gsm_bts *gsm_bts_by_lac(struct gsm_network *net, unsigned int lac,
				struct gsm_bts *start_bts);

extern void *tall_bsc_ctx;

/* this actually refers to the IPA transport, not the BTS model */
static inline int is_ipaccess_bts(const struct gsm_bts *bts)
{
	switch (bts->type) {
	case GSM_BTS_TYPE_NANOBTS:
	case GSM_BTS_TYPE_OSMOBTS:
		return 1;
	default:
		break;
	}
	return 0;
}

static inline int is_sysmobts_v2(const struct gsm_bts *bts)
{
	switch (bts->type) {
	case GSM_BTS_TYPE_OSMOBTS:
		return 1;
	default:
		break;
	}
	return 0;
}

static inline int is_siemens_bts(const struct gsm_bts *bts)
{
	switch (bts->type) {
	case GSM_BTS_TYPE_BS11:
		return 1;
	default:
		break;
	}

	return 0;
}

static inline int is_nokia_bts(const struct gsm_bts *bts)
{
	switch (bts->type) {
	case GSM_BTS_TYPE_NOKIA_SITE:
		return 1;
	default:
		break;
	}

	return 0;
}

static inline int is_ericsson_bts(const struct gsm_bts *bts)
{
	switch (bts->type) {
	case GSM_BTS_TYPE_RBS2000:
		return 1;
	default:
		break;
	}

	return 0;
}

static inline int is_e1_bts(const struct gsm_bts *bts)
{
	switch (bts->type) {
	case GSM_BTS_TYPE_BS11:
	case GSM_BTS_TYPE_RBS2000:
	case GSM_BTS_TYPE_NOKIA_SITE:
		return 1;
	default:
		break;
	}

	return 0;
}

extern struct gsm_network *bsc_gsmnet;

enum bts_gprs_mode bts_gprs_mode_parse(const char *arg, int *valid);
const char *bts_gprs_mode_name(enum bts_gprs_mode mode);
int bts_gprs_mode_is_compat(struct gsm_bts *bts, enum bts_gprs_mode mode);

void gsm48_ra_id_by_bts(struct gsm48_ra_id *buf, struct gsm_bts *bts);
void gprs_ra_id_by_bts(struct gprs_ra_id *raid, struct gsm_bts *bts);

int gsm_bts_model_register(struct gsm_bts_model *model);

struct gsm_subscriber_connection *bsc_subscr_con_allocate(struct gsm_network *network);

struct gsm_bts *gsm_bts_alloc_register(struct gsm_network *net, enum gsm_bts_type type, uint8_t bsic);
struct gsm_bts *bsc_bts_alloc_register(struct gsm_network *net, enum gsm_bts_type type, uint8_t bsic);

void set_ts_e1link(struct gsm_bts_trx_ts *ts, uint8_t e1_nr,
		   uint8_t e1_ts, uint8_t e1_ts_ss);

void gsm_trx_lock_rf(struct gsm_bts_trx *trx, bool locked, const char *reason);
struct gsm_bts_trx *gsm_bts_trx_by_nr(struct gsm_bts *bts, int nr);
int gsm_bts_trx_set_system_infos(struct gsm_bts_trx *trx);
int gsm_bts_set_system_infos(struct gsm_bts *bts);

/* generic E1 line operations for all ISDN-based BTS. */
extern struct e1inp_line_ops bts_isdn_e1inp_line_ops;

extern const struct value_string bts_type_names[_NUM_GSM_BTS_TYPE+1];
extern const struct value_string bts_type_descs[_NUM_GSM_BTS_TYPE+1];

char *get_model_oml_status(const struct gsm_bts *bts);

unsigned long long bts_uptime(const struct gsm_bts *bts);

/* control interface handling */
int bsc_base_ctrl_cmds_install(void);

/* dependency handling */
void bts_depend_mark(struct gsm_bts *bts, int dep);
void bts_depend_clear(struct gsm_bts *bts, int dep);
int bts_depend_check(struct gsm_bts *bts);
int bts_depend_is_depedency(struct gsm_bts *base, struct gsm_bts *other);

int gsm_bts_get_radio_link_timeout(const struct gsm_bts *bts);
void gsm_bts_set_radio_link_timeout(struct gsm_bts *bts, int value);

bool trx_is_usable(const struct gsm_bts_trx *trx);
bool ts_is_usable(const struct gsm_bts_trx_ts *ts);

int gsm_lchan_type_by_pchan(enum gsm_phys_chan_config pchan);
enum gsm_phys_chan_config gsm_pchan_by_lchan_type(enum gsm_chan_t type);

void gsm_bts_all_ts_dispatch(struct gsm_bts *bts, uint32_t ts_ev, void *data);
void gsm_trx_all_ts_dispatch(struct gsm_bts_trx *trx, uint32_t ts_ev, void *data);

int bts_count_free_ts(struct gsm_bts *bts, enum gsm_phys_chan_config pchan);

bool trx_has_valid_pchan_config(const struct gsm_bts_trx *trx);

#endif /* _GSM_DATA_H */
