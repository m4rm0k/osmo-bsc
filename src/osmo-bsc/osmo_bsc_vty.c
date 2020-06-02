/* Osmo BSC VTY Configuration */
/* (C) 2009-2015 by Holger Hans Peter Freyther
 * (C) 2009-2014 by On-Waves
 * (C) 2018 by Harald Welte <laforge@gnumonks.org>
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <osmocom/bsc/gsm_data.h>
#include <osmocom/bsc/osmo_bsc.h>
#include <osmocom/bsc/bsc_msc_data.h>
#include <osmocom/bsc/vty.h>
#include <osmocom/bsc/bsc_subscriber.h>
#include <osmocom/bsc/debug.h>
#include <osmocom/bsc/osmux.h>

#include <osmocom/core/talloc.h>
#include <osmocom/gsm/gsm48.h>
#include <osmocom/gsm/gsm23236.h>
#include <osmocom/vty/logging.h>
#include <osmocom/mgcp_client/mgcp_client.h>


#include <time.h>

static struct bsc_msc_data *bsc_msc_data(struct vty *vty)
{
	return vty->index;
}

static struct cmd_node bsc_node = {
	BSC_NODE,
	"%s(config-bsc)# ",
	1,
};

static struct cmd_node msc_node = {
	MSC_NODE,
	"%s(config-msc)# ",
	1,
};

#define MSC_NR_RANGE "<0-1000>"

DEFUN(cfg_net_msc, cfg_net_msc_cmd,
      "msc [" MSC_NR_RANGE "]", "Configure MSC details\n" "MSC connection to configure\n")
{
	int index = argc == 1 ? atoi(argv[0]) : 0;
	struct bsc_msc_data *msc;

	msc = osmo_msc_data_alloc(bsc_gsmnet, index);
	if (!msc) {
		vty_out(vty, "%%Failed to allocate MSC data.%s", VTY_NEWLINE);
		return CMD_WARNING;
	}

	vty->index = msc;
	vty->node = MSC_NODE;
	return CMD_SUCCESS;
}

DEFUN(cfg_net_bsc, cfg_net_bsc_cmd,
      "bsc", "Configure BSC\n")
{
	vty->node = BSC_NODE;
	return CMD_SUCCESS;
}

static void write_msc_amr_options(struct vty *vty, struct bsc_msc_data *msc)
{
#define WRITE_AMR(vty, msc, name, var) \
	vty_out(vty, " amr-config %s %s%s", \
		name, msc->amr_conf.var ? "allowed" : "forbidden", \
		VTY_NEWLINE);

	WRITE_AMR(vty, msc, "12_2k", m12_2);
	WRITE_AMR(vty, msc, "10_2k", m10_2);
	WRITE_AMR(vty, msc, "7_95k", m7_95);
	WRITE_AMR(vty, msc, "7_40k", m7_40);
	WRITE_AMR(vty, msc, "6_70k", m6_70);
	WRITE_AMR(vty, msc, "5_90k", m5_90);
	WRITE_AMR(vty, msc, "5_15k", m5_15);
	WRITE_AMR(vty, msc, "4_75k", m4_75);
#undef WRITE_AMR

	if (msc->amr_octet_aligned)
		vty_out(vty, " amr-payload octet-aligned%s", VTY_NEWLINE);
	else
		vty_out(vty, " amr-payload bandwith-efficient%s", VTY_NEWLINE);
}

static void msc_write_nri(struct vty *vty, struct bsc_msc_data *msc, bool verbose);

static void write_msc(struct vty *vty, struct bsc_msc_data *msc)
{
	vty_out(vty, "msc %d%s", msc->nr, VTY_NEWLINE);
	if (msc->core_plmn.mnc != GSM_MCC_MNC_INVALID)
		vty_out(vty, " core-mobile-network-code %s%s",
			osmo_mnc_name(msc->core_plmn.mnc, msc->core_plmn.mnc_3_digits), VTY_NEWLINE);
	if (msc->core_plmn.mcc != GSM_MCC_MNC_INVALID)
		vty_out(vty, " core-mobile-country-code %s%s",
			osmo_mcc_name(msc->core_plmn.mcc), VTY_NEWLINE);
	if (msc->core_lac != -1)
		vty_out(vty, " core-location-area-code %d%s",
			msc->core_lac, VTY_NEWLINE);
	if (msc->core_ci != -1)
		vty_out(vty, " core-cell-identity %d%s",
			msc->core_ci, VTY_NEWLINE);

	if (msc->audio_length != 0) {
		int i;

		vty_out(vty, " codec-list ");
		for (i = 0; i < msc->audio_length; ++i) {
			if (i != 0)
				vty_out(vty, " ");

			if (msc->audio_support[i]->hr)
				vty_out(vty, "hr%.1u", msc->audio_support[i]->ver);
			else
				vty_out(vty, "fr%.1u", msc->audio_support[i]->ver);
		}
		vty_out(vty, "%s", VTY_NEWLINE);

	}

	vty_out(vty, " allow-emergency %s%s", msc->allow_emerg ?
					"allow" : "deny", VTY_NEWLINE);

	/* write amr options */
	write_msc_amr_options(vty, msc);

	/* write sccp connection configuration */
	if (msc->a.bsc_addr_name) {
		vty_out(vty, " bsc-addr %s%s",
			msc->a.bsc_addr_name, VTY_NEWLINE);
	}
	if (msc->a.msc_addr_name) {
		vty_out(vty, " msc-addr %s%s",
			msc->a.msc_addr_name, VTY_NEWLINE);
	}
	vty_out(vty, " asp-protocol %s%s", osmo_ss7_asp_protocol_name(msc->a.asp_proto), VTY_NEWLINE);
	vty_out(vty, " lcls-mode %s%s", get_value_string(bsc_lcls_mode_names, msc->lcls_mode),
		VTY_NEWLINE);

	if (msc->lcls_codec_mismatch_allow)
		vty_out(vty, " lcls-codec-mismatch allowed%s", VTY_NEWLINE);
	else
		vty_out(vty, " lcls-codec-mismatch forbidden%s", VTY_NEWLINE);

	/* write MGW configuration */
	mgcp_client_config_write(vty, " ");

	if (msc->x_osmo_ign_configured) {
		if (!msc->x_osmo_ign)
			vty_out(vty, " no mgw x-osmo-ign%s", VTY_NEWLINE);
		else
			vty_out(vty, " mgw x-osmo-ign call-id%s", VTY_NEWLINE);
	}

	if (msc->use_osmux != OSMUX_USAGE_OFF) {
		vty_out(vty, " osmux %s%s", msc->use_osmux == OSMUX_USAGE_ON ? "on" : "only",
			VTY_NEWLINE);
	}

	msc_write_nri(vty, msc, false);
}

static int config_write_msc(struct vty *vty)
{
	struct bsc_msc_data *msc;

	llist_for_each_entry(msc, &bsc_gsmnet->mscs, entry)
		write_msc(vty, msc);

	return CMD_SUCCESS;
}

static int config_write_bsc(struct vty *vty)
{
	vty_out(vty, "bsc%s", VTY_NEWLINE);
	vty_out(vty, " mid-call-timeout %d%s", bsc_gsmnet->mid_call_timeout, VTY_NEWLINE);
	if (bsc_gsmnet->rf_ctrl_name)
		vty_out(vty, " bsc-rf-socket %s%s",
			bsc_gsmnet->rf_ctrl_name, VTY_NEWLINE);

	if (bsc_gsmnet->auto_off_timeout != -1)
		vty_out(vty, " bsc-auto-rf-off %d%s",
			bsc_gsmnet->auto_off_timeout, VTY_NEWLINE);

	return CMD_SUCCESS;
}

DEFUN(cfg_net_bsc_ncc,
      cfg_net_bsc_ncc_cmd,
      "core-mobile-network-code <1-999>",
      "Use this network code for the core network\n" "MNC value\n")
{
	struct bsc_msc_data *data = bsc_msc_data(vty);
	uint16_t mnc;
	bool mnc_3_digits;

	if (osmo_mnc_from_str(argv[0], &mnc, &mnc_3_digits)) {
		vty_out(vty, "%% Error decoding MNC: %s%s", argv[0], VTY_NEWLINE);
		return CMD_WARNING;
	}
	data->core_plmn.mnc = mnc;
	data->core_plmn.mnc_3_digits = mnc_3_digits;
	return CMD_SUCCESS;
}

DEFUN(cfg_net_bsc_mcc,
      cfg_net_bsc_mcc_cmd,
      "core-mobile-country-code <1-999>",
      "Use this country code for the core network\n" "MCC value\n")
{
	uint16_t mcc;
	struct bsc_msc_data *data = bsc_msc_data(vty);
	if (osmo_mcc_from_str(argv[0], &mcc)) {
		vty_out(vty, "%% Error decoding MCC: %s%s", argv[0], VTY_NEWLINE);
		return CMD_WARNING;
	}
	data->core_plmn.mcc = mcc;
	return CMD_SUCCESS;
}

DEFUN(cfg_net_bsc_lac,
      cfg_net_bsc_lac_cmd,
      "core-location-area-code <0-65535>",
      "Use this location area code for the core network\n" "LAC value\n")
{
	struct bsc_msc_data *data = bsc_msc_data(vty);
	data->core_lac = atoi(argv[0]);
	return CMD_SUCCESS;
}

DEFUN(cfg_net_bsc_ci,
      cfg_net_bsc_ci_cmd,
      "core-cell-identity <0-65535>",
      "Use this cell identity for the core network\n" "CI value\n")
{
	struct bsc_msc_data *data = bsc_msc_data(vty);
	data->core_ci = atoi(argv[0]);
	return CMD_SUCCESS;
}

DEFUN_DEPRECATED(cfg_net_bsc_rtp_base,
      cfg_net_bsc_rtp_base_cmd,
      "ip.access rtp-base <1-65000>",
      "deprecated\n" "deprecated, RTP is handled by the MGW\n" "deprecated\n")
{
	vty_out(vty, "%% deprecated: 'ip.access rtp-base' has no effect, RTP is handled by the MGW%s", VTY_NEWLINE);
	return CMD_SUCCESS;
}

DEFUN(cfg_net_bsc_codec_list,
      cfg_net_bsc_codec_list_cmd,
      "codec-list .LIST",
      "Set the allowed audio codecs\n"
      "List of audio codecs, e.g. fr3 fr1 hr3\n")
{
	struct bsc_msc_data *data = bsc_msc_data(vty);
	int i;

	/* free the old list... if it exists */
	if (data->audio_support) {
		talloc_free(data->audio_support);
		data->audio_support = NULL;
		data->audio_length = 0;
	}

	/* create a new array */
	data->audio_support =
		talloc_zero_array(bsc_gsmnet, struct gsm_audio_support *, argc);
	data->audio_length = argc;

	for (i = 0; i < argc; ++i) {
		/* check for hrX or frX */
		if (strlen(argv[i]) != 3
				|| argv[i][1] != 'r'
				|| (argv[i][0] != 'h' && argv[i][0] != 'f')
				|| argv[i][2] < 0x30
				|| argv[i][2] > 0x39)
			goto error;

		data->audio_support[i] = talloc_zero(data->audio_support,
				struct gsm_audio_support);
		data->audio_support[i]->ver = atoi(argv[i] + 2);

		if (strncmp("hr", argv[i], 2) == 0)
			data->audio_support[i]->hr = 1;
		else if (strncmp("fr", argv[i], 2) == 0)
			data->audio_support[i]->hr = 0;
	}

	return CMD_SUCCESS;

error:
	vty_out(vty, "Codec name must be hrX or frX. Was '%s'%s",
			argv[i], VTY_NEWLINE);
	return CMD_ERR_INCOMPLETE;
}

#define LEGACY_STR "This command has no effect, it is kept to support legacy config files\n"

DEFUN_DEPRECATED(deprecated_ussd_text,
      cfg_net_msc_welcome_ussd_cmd,
      "bsc-welcome-text .TEXT", LEGACY_STR LEGACY_STR)
{
	vty_out(vty, "%% osmo-bsc no longer supports USSD notification. These commands have no effect:%s"
		"%%   bsc-welcome-text, bsc-msc-lost-text, mid-call-text, bsc-grace-text, missing-msc-text%s",
		VTY_NEWLINE, VTY_NEWLINE);
	return CMD_WARNING;
}

DEFUN_DEPRECATED(deprecated_no_ussd_text,
      cfg_net_msc_no_welcome_ussd_cmd,
      "no bsc-welcome-text",
      NO_STR LEGACY_STR)
{
	return CMD_SUCCESS;
}

ALIAS_DEPRECATED(deprecated_ussd_text,
      cfg_net_msc_lost_ussd_cmd,
      "bsc-msc-lost-text .TEXT", LEGACY_STR LEGACY_STR);

ALIAS_DEPRECATED(deprecated_no_ussd_text,
      cfg_net_msc_no_lost_ussd_cmd,
      "no bsc-msc-lost-text", NO_STR LEGACY_STR);

ALIAS_DEPRECATED(deprecated_ussd_text,
      cfg_net_msc_grace_ussd_cmd,
      "bsc-grace-text .TEXT", LEGACY_STR LEGACY_STR);

ALIAS_DEPRECATED(deprecated_no_ussd_text,
      cfg_net_msc_no_grace_ussd_cmd,
      "no bsc-grace-text", NO_STR LEGACY_STR);

ALIAS_DEPRECATED(deprecated_ussd_text,
      cfg_net_bsc_missing_msc_ussd_cmd,
      "missing-msc-text .TEXT", LEGACY_STR LEGACY_STR);

ALIAS_DEPRECATED(deprecated_no_ussd_text,
      cfg_net_bsc_no_missing_msc_text_cmd,
      "no missing-msc-text", NO_STR LEGACY_STR);

DEFUN_DEPRECATED(cfg_net_msc_type,
      cfg_net_msc_type_cmd,
      "type (normal|local)",
      LEGACY_STR LEGACY_STR)
{
	vty_out(vty, "%% 'msc' / 'type' config is deprecated and no longer has any effect%s",
		VTY_NEWLINE);
	return CMD_SUCCESS;
}

DEFUN(cfg_net_msc_emerg,
      cfg_net_msc_emerg_cmd,
      "allow-emergency (allow|deny)",
      "Allow CM ServiceRequests with type emergency\n"
      "Allow\n" "Deny\n")
{
	struct bsc_msc_data *data = bsc_msc_data(vty);
	data->allow_emerg = strcmp("allow", argv[0]) == 0;
	return CMD_SUCCESS;
}

#define AMR_CONF_STR "AMR Multirate Configuration\n"
#define AMR_COMMAND(name) \
	DEFUN(cfg_net_msc_amr_##name,					\
	  cfg_net_msc_amr_##name##_cmd,					\
	  "amr-config " #name "k (allowed|forbidden)",			\
	  AMR_CONF_STR "Bitrate\n" "Allowed\n" "Forbidden\n")		\
{									\
	struct bsc_msc_data *msc = bsc_msc_data(vty);			\
									\
	msc->amr_conf.m##name = strcmp(argv[0], "allowed") == 0; 	\
	return CMD_SUCCESS;						\
}

AMR_COMMAND(12_2)
AMR_COMMAND(10_2)
AMR_COMMAND(7_95)
AMR_COMMAND(7_40)
AMR_COMMAND(6_70)
AMR_COMMAND(5_90)
AMR_COMMAND(5_15)
AMR_COMMAND(4_75)

/* Make sure only standard SSN numbers are used. If no ssn number is
 * configured, silently apply the default SSN */
static void enforce_standard_ssn(struct vty *vty, struct osmo_sccp_addr *addr)
{
	if (addr->presence & OSMO_SCCP_ADDR_T_SSN) {
		if (addr->ssn != OSMO_SCCP_SSN_BSSAP)
			vty_out(vty,
				"setting an SSN (%u) different from the standard (%u) is not allowed, will use standard SSN for address: %s%s",
				addr->ssn, OSMO_SCCP_SSN_BSSAP, osmo_sccp_addr_dump(addr), VTY_NEWLINE);
	}

	addr->presence |= OSMO_SCCP_ADDR_T_SSN;
	addr->ssn = OSMO_SCCP_SSN_BSSAP;
}

DEFUN(cfg_msc_cs7_bsc_addr,
      cfg_msc_cs7_bsc_addr_cmd,
      "bsc-addr NAME",
      "Calling Address (local address of this BSC)\n" "SCCP address name\n")
{
	struct bsc_msc_data *msc = bsc_msc_data(vty);
	const char *bsc_addr_name = argv[0];
	struct osmo_ss7_instance *ss7;

	ss7 = osmo_sccp_addr_by_name(&msc->a.bsc_addr, bsc_addr_name);
	if (!ss7) {
		vty_out(vty, "Error: No such SCCP addressbook entry: '%s'%s", bsc_addr_name, VTY_NEWLINE);
		return CMD_ERR_INCOMPLETE;
	}

	/* Prevent mixing addresses from different CS7/SS7 instances */
	if (msc->a.cs7_instance_valid) {
		if (msc->a.cs7_instance != ss7->cfg.id) {
			vty_out(vty,
				"Error: SCCP addressbook entry from mismatching CS7 instance: '%s'%s",
				bsc_addr_name, VTY_NEWLINE);
			return CMD_ERR_INCOMPLETE;
		}
	}

	msc->a.cs7_instance = ss7->cfg.id;
	msc->a.cs7_instance_valid = true;
	enforce_standard_ssn(vty, &msc->a.bsc_addr);
	msc->a.bsc_addr_name = talloc_strdup(msc, bsc_addr_name);
	return CMD_SUCCESS;
}

DEFUN(cfg_msc_cs7_msc_addr,
      cfg_msc_cs7_msc_addr_cmd,
      "msc-addr NAME",
      "Called Address (remote address of the MSC)\n" "SCCP address name\n")
{
	struct bsc_msc_data *msc = bsc_msc_data(vty);
	const char *msc_addr_name = argv[0];
	struct osmo_ss7_instance *ss7;

	ss7 = osmo_sccp_addr_by_name(&msc->a.msc_addr, msc_addr_name);
	if (!ss7) {
		vty_out(vty, "Error: No such SCCP addressbook entry: '%s'%s", msc_addr_name, VTY_NEWLINE);
		return CMD_ERR_INCOMPLETE;
	}

	/* Prevent mixing addresses from different CS7/SS7 instances */
	if (msc->a.cs7_instance_valid) {
		if (msc->a.cs7_instance != ss7->cfg.id) {
			vty_out(vty,
				"Error: SCCP addressbook entry from mismatching CS7 instance: '%s'%s",
				msc_addr_name, VTY_NEWLINE);
			return CMD_ERR_INCOMPLETE;
		}
	}

	msc->a.cs7_instance = ss7->cfg.id;
	msc->a.cs7_instance_valid = true;
	enforce_standard_ssn(vty, &msc->a.msc_addr);
	msc->a.msc_addr_name = talloc_strdup(msc, msc_addr_name);
	return CMD_SUCCESS;
}

DEFUN(cfg_msc_cs7_asp_proto,
      cfg_msc_cs7_asp_proto_cmd,
      "asp-protocol (m3ua|sua|ipa)",
      "A interface protocol to use for this MSC)\n"
      "MTP3 User Adaptation\n"
      "SCCP User Adaptation\n"
      "IPA Multiplex (SCCP Lite)\n")
{
	struct bsc_msc_data *msc = bsc_msc_data(vty);

	msc->a.asp_proto = get_string_value(osmo_ss7_asp_protocol_vals, argv[0]);
	return CMD_SUCCESS;
}

DEFUN(cfg_net_msc_lcls_mode,
      cfg_net_msc_lcls_mode_cmd,
      "lcls-mode (disabled|mgw-loop|bts-loop)",
      "Configure 3GPP LCLS (Local Call, Local Switch)\n"
      "Disable LCLS for all calls of this MSC\n"
      "Enable LCLS with looping traffic in MGW\n"
      "Enable LCLS with looping traffic between BTS\n")
{
	struct bsc_msc_data *data = bsc_msc_data(vty);
	data->lcls_mode = get_string_value(bsc_lcls_mode_names, argv[0]);
	return CMD_SUCCESS;
}

DEFUN(cfg_net_msc_lcls_mismtch,
      cfg_net_msc_lcls_mismtch_cmd,
      "lcls-codec-mismatch (allowed|forbidden)",
      "Allow 3GPP LCLS (Local Call, Local Switch) when call legs use different codec/rate\n"
      "Allow LCLS only only for calls that use the same codec/rate on both legs\n"
      "Do not Allow LCLS for calls that use a different codec/rate on both legs\n")
{
	struct bsc_msc_data *data = bsc_msc_data(vty);

	if (strcmp(argv[0], "allowed") == 0)
		data->lcls_codec_mismatch_allow = true;
	else
		data->lcls_codec_mismatch_allow = false;

	return CMD_SUCCESS;
}

DEFUN(cfg_msc_mgw_x_osmo_ign,
      cfg_msc_mgw_x_osmo_ign_cmd,
      "mgw x-osmo-ign call-id",
      MGCP_CLIENT_MGW_STR
      "Set a (non-standard) X-Osmo-IGN header in all CRCX messages for RTP streams"
      " associated with this MSC, useful for A/SCCPlite MSCs, since osmo-bsc cannot know"
      " the MSC's chosen CallID. This is enabled by default for A/SCCPlite connections,"
      " disabled by default for all others.\n"
      "Send 'X-Osmo-IGN: C' to ignore CallID mismatches. See OsmoMGW.\n")
{
	struct bsc_msc_data *msc = bsc_msc_data(vty);
	msc->x_osmo_ign |= MGCP_X_OSMO_IGN_CALLID;
	msc->x_osmo_ign_configured = true;
	return CMD_SUCCESS;
}

DEFUN(cfg_msc_no_mgw_x_osmo_ign,
      cfg_msc_no_mgw_x_osmo_ign_cmd,
      "no mgw x-osmo-ign",
      NO_STR
      MGCP_CLIENT_MGW_STR
      "Do not send X-Osmo-IGN MGCP header to this MSC\n")
{
	struct bsc_msc_data *msc = bsc_msc_data(vty);
	msc->x_osmo_ign = 0;
	msc->x_osmo_ign_configured = true;
	return CMD_SUCCESS;
}

#define OSMUX_STR "RTP multiplexing\n"
DEFUN(cfg_msc_osmux,
      cfg_msc_osmux_cmd,
      "osmux (on|off|only)",
       OSMUX_STR "Enable OSMUX\n" "Disable OSMUX\n" "Only use OSMUX\n")
{
	struct bsc_msc_data *msc = bsc_msc_data(vty);
	if (strcmp(argv[0], "off") == 0)
		msc->use_osmux = OSMUX_USAGE_OFF;
	else if (strcmp(argv[0], "on") == 0)
		msc->use_osmux = OSMUX_USAGE_ON;
	else if (strcmp(argv[0], "only") == 0)
		msc->use_osmux = OSMUX_USAGE_ONLY;

	return CMD_SUCCESS;
}

ALIAS_DEPRECATED(deprecated_ussd_text,
      cfg_net_bsc_mid_call_text_cmd,
      "mid-call-text .TEXT",
      LEGACY_STR LEGACY_STR);

DEFUN(cfg_net_bsc_mid_call_timeout,
      cfg_net_bsc_mid_call_timeout_cmd,
      "mid-call-timeout NR",
      "Switch from Grace to Off in NR seconds.\n" "Timeout in seconds\n")
{
	bsc_gsmnet->mid_call_timeout = atoi(argv[0]);
	return CMD_SUCCESS;
}

DEFUN(cfg_net_rf_socket,
      cfg_net_rf_socket_cmd,
      "bsc-rf-socket PATH",
      "Set the filename for the RF control interface.\n" "RF Control path\n")
{
	osmo_talloc_replace_string(bsc_gsmnet, &bsc_gsmnet->rf_ctrl_name, argv[0]);
	return CMD_SUCCESS;
}

DEFUN(cfg_net_rf_off_time,
      cfg_net_rf_off_time_cmd,
      "bsc-auto-rf-off <1-65000>",
      "Disable RF on MSC Connection\n" "Timeout\n")
{
	bsc_gsmnet->auto_off_timeout = atoi(argv[0]);
	return CMD_SUCCESS;
}

DEFUN(cfg_net_no_rf_off_time,
      cfg_net_no_rf_off_time_cmd,
      "no bsc-auto-rf-off",
      NO_STR "Disable RF on MSC Connection\n")
{
	bsc_gsmnet->auto_off_timeout = -1;
	return CMD_SUCCESS;
}

DEFUN(show_statistics,
      show_statistics_cmd,
      "show statistics",
      SHOW_STR "Statistics about the BSC\n")
{
	openbsc_vty_print_statistics(vty, bsc_gsmnet);
	return CMD_SUCCESS;
}

DEFUN(show_mscs,
      show_mscs_cmd,
      "show mscs",
      SHOW_STR "MSC Connections and State\n")
{
	struct bsc_msc_data *msc;
	llist_for_each_entry(msc, &bsc_gsmnet->mscs, entry) {
		vty_out(vty, "%d %s %s ",
			msc->a.cs7_instance,
			osmo_ss7_asp_protocol_name(msc->a.asp_proto),
			osmo_sccp_inst_addr_name(msc->a.sccp, &msc->a.bsc_addr));
		vty_out(vty, "%s%s",
			osmo_sccp_inst_addr_name(msc->a.sccp, &msc->a.msc_addr),
			VTY_NEWLINE);
	}

	return CMD_SUCCESS;
}

DEFUN(show_pos,
      show_pos_cmd,
      "show position",
      SHOW_STR "Position information of the BTS\n")
{
	struct gsm_bts *bts;
	struct bts_location *curloc;
	struct tm time;
	char timestr[50];

	llist_for_each_entry(bts, &bsc_gsmnet->bts_list, list) {
		if (llist_empty(&bts->loc_list)) {
			vty_out(vty, "BTS Nr: %d position invalid%s", bts->nr,
				VTY_NEWLINE);
			continue;
		}
		curloc = llist_entry(bts->loc_list.next, struct bts_location, list);
		if (gmtime_r(&curloc->tstamp, &time) == NULL) {
			vty_out(vty, "Time conversion failed for BTS %d%s", bts->nr,
				VTY_NEWLINE);
			continue;
		}
		if (asctime_r(&time, timestr) == NULL) {
			vty_out(vty, "Time conversion failed for BTS %d%s", bts->nr,
				VTY_NEWLINE);
			continue;
		}
		/* Last character in asctime is \n */
		timestr[strlen(timestr)-1] = 0;

		vty_out(vty, "BTS Nr: %d position: %s time: %s%s", bts->nr,
			get_value_string(bts_loc_fix_names, curloc->valid), timestr,
			VTY_NEWLINE);
		vty_out(vty, " lat: %f lon: %f height: %f%s", curloc->lat, curloc->lon,
			curloc->height, VTY_NEWLINE);
	}
	return CMD_SUCCESS;
}

DEFUN(gen_position_trap,
      gen_position_trap_cmd,
      "generate-location-state-trap <0-255>",
      "Generate location state report\n"
      "BTS to report\n")
{
	int bts_nr;
	struct gsm_bts *bts;
	struct gsm_network *net = bsc_gsmnet;

	bts_nr = atoi(argv[0]);
	if (bts_nr >= net->num_bts) {
		vty_out(vty, "%% can't find BTS '%s'%s", argv[0],
			VTY_NEWLINE);
		return CMD_WARNING;
	}

	bts = gsm_bts_num(net, bts_nr);
	bsc_gen_location_state_trap(bts);
	return CMD_SUCCESS;
}

DEFUN(logging_fltr_imsi,
      logging_fltr_imsi_cmd,
      "logging filter imsi IMSI",
	LOGGING_STR FILTER_STR
      "Filter log messages by IMSI\n" "IMSI to be used as filter\n")
{
	struct bsc_subscr *bsc_subscr;
	struct log_target *tgt = osmo_log_vty2tgt(vty);
	const char *imsi = argv[0];

	if (!tgt)
		return CMD_WARNING;

	bsc_subscr = bsc_subscr_find_or_create_by_imsi(bsc_gsmnet->bsc_subscribers, imsi);

	if (!bsc_subscr) {
		vty_out(vty, "%%failed to enable logging for subscriber with IMSI(%s)%s",
			imsi, VTY_NEWLINE);
		return CMD_WARNING;
	}

	log_set_filter_bsc_subscr(tgt, bsc_subscr);
	/* log_set_filter has grabbed its own reference  */
	bsc_subscr_put(bsc_subscr);

	return CMD_SUCCESS;
}

static void dump_one_sub(struct vty *vty, struct bsc_subscr *bsub)
{
	vty_out(vty, " %15s  %08x  %5u  %d%s", bsub->imsi, bsub->tmsi, bsub->lac, bsub->use_count,
		VTY_NEWLINE);
}

DEFUN(show_subscr_all,
	show_subscr_all_cmd,
	"show subscriber all",
	SHOW_STR "Display information about subscribers\n" "All Subscribers\n")
{
	struct bsc_subscr *bsc_subscr;

	vty_out(vty, " IMSI             TMSI      LAC    Use%s", VTY_NEWLINE);
	/*           " 001010123456789  ffffffff  65534  1" */

	llist_for_each_entry(bsc_subscr, bsc_gsmnet->bsc_subscribers, entry)
		dump_one_sub(vty, bsc_subscr);

	return CMD_SUCCESS;
}

DEFUN_DEPRECATED(cfg_net_msc_ping_time, cfg_net_msc_ping_time_cmd,
      "timeout-ping ARG", LEGACY_STR "-\n")
{
	vty_out(vty, "%% timeout-ping / timeout-pong config is deprecated and has no effect%s",
		VTY_NEWLINE);
	return CMD_WARNING;
}

ALIAS_DEPRECATED(cfg_net_msc_ping_time, cfg_net_msc_no_ping_time_cmd,
      "no timeout-ping [ARG]", NO_STR LEGACY_STR "-\n");

ALIAS_DEPRECATED(cfg_net_msc_ping_time, cfg_net_msc_pong_time_cmd,
      "timeout-pong ARG", LEGACY_STR "-\n");

DEFUN_DEPRECATED(cfg_net_msc_dest, cfg_net_msc_dest_cmd,
      "dest A.B.C.D <1-65000> <0-255>", LEGACY_STR "-\n" "-\n" "-\n")
{
	vty_out(vty, "%% dest config is deprecated and has no effect%s", VTY_NEWLINE);
	return CMD_WARNING;
}

ALIAS_DEPRECATED(cfg_net_msc_dest, cfg_net_msc_no_dest_cmd,
      "no dest A.B.C.D <1-65000> <0-255>", NO_STR LEGACY_STR "-\n" "-\n" "-\n");

DEFUN(cfg_net_msc_amr_octet_align,
      cfg_net_msc_amr_octet_align_cmd,
      "amr-payload (octet-aligned|bandwith-efficient",
      "Set AMR payload framing mode\n"
      "payload fields aligned on octet boundaries\n"
      "payload fields packed (AoIP)\n")
{
	struct bsc_msc_data *data = bsc_msc_data(vty);

	if (strcmp(argv[0], "octet-aligned") == 0)
		data->amr_octet_aligned = true;
	else if (strcmp(argv[0], "bandwith-efficient") == 0)
		data->amr_octet_aligned = false;

	return CMD_SUCCESS;
}


#define NRI_STR "Mapping of Network Resource Indicators to this MSC, for MSC pooling\n"
#define NRI_FIRST_LAST_STR "First value of the NRI value range: either decimal ('23') or hexadecimal ('0x17')\n" \
	"Last value of the NRI value range: either decimal ('23') or hexadecimal ('0x17');" \
	" if omitted, apply only the first value.\n"

#define NRI_WARN(MSC, FORMAT, args...) do { \
		vty_out(vty, "%% Warning: msc %d: " FORMAT "%s", MSC->nr, ##args, VTY_NEWLINE); \
		LOGP(DMSC, LOGL_ERROR, "Warning: msc %d: " FORMAT "\n", MSC->nr, ##args); \
	} while (0)

#define NRI_ARGS_TO_STR_FMT "%s%s%s"
#define NRI_ARGS_TO_STR_ARGS(ARGC, ARGV) ARGV[0], (ARGC>1)? ".." : "", (ARGC>1)? ARGV[1] : ""

DEFUN(cfg_msc_nri_add, cfg_msc_nri_add_cmd,
      "nri add FIRST [LAST]",
      NRI_STR "Add NRI value or range to the NRI mapping for this MSC\n"
      NRI_FIRST_LAST_STR)
{
	struct bsc_msc_data *msc = bsc_msc_data(vty);
	struct bsc_msc_data *other_msc;
	bool before;
	int rc;
	const char *message;
	struct osmo_nri_range add_range;

	rc = osmo_nri_vty_add(&message, &add_range, msc, &msc->nri_ranges, argc, argv, bsc_gsmnet->nri_bitlen);
	if (message) {
		NRI_WARN(msc, "%s: " NRI_ARGS_TO_STR_FMT, message, NRI_ARGS_TO_STR_ARGS(argc, argv));
	}
	if (rc < 0)
		return CMD_WARNING;

	/* Issue a warning about NRI range overlaps (but still allow them).
	 * Overlapping ranges will map to whichever MSC comes fist in the bsc_gsmnet->mscs llist,
	 * which is not necessarily in the order of increasing msc->nr. */
	before = true;
	llist_for_each_entry(other_msc, &bsc_gsmnet->mscs, entry) {
		if (other_msc == msc) {
			before = false;
			continue;
		}
		if (osmo_nri_range_overlaps_list(&other_msc->nri_ranges, &add_range)) {
			NRI_WARN(msc, "NRI range [%d..%d] overlaps between msc %d and msc %d."
				 " For overlaps, msc %d has higher priority than msc %d",
				 add_range.first, add_range.last, msc->nr, other_msc->nr,
				 before ? other_msc->nr : msc->nr, before ? msc->nr : other_msc->nr);
		}
	}
	return CMD_SUCCESS;
}

DEFUN(cfg_msc_nri_del, cfg_msc_nri_del_cmd,
      "nri del FIRST [LAST]",
      NRI_STR "Remove NRI value or range from the NRI mapping for this MSC\n"
      NRI_FIRST_LAST_STR)
{
	struct bsc_msc_data *msc = bsc_msc_data(vty);
	int rc;
	const char *message;

	rc = osmo_nri_vty_del(&message, NULL, msc, &msc->nri_ranges, argc, argv);
	if (message) {
		NRI_WARN(msc, "%s: " NRI_ARGS_TO_STR_FMT, message, NRI_ARGS_TO_STR_ARGS(argc, argv));
	}
	if (rc < 0)
		return CMD_WARNING;
	return CMD_SUCCESS;
}

static void msc_write_nri(struct vty *vty, struct bsc_msc_data *msc, bool verbose)
{
	struct osmo_nri_range *r;

	if (verbose) {
		vty_out(vty, "msc %d%s", msc->nr, VTY_NEWLINE);
		if (llist_empty(&msc->nri_ranges)) {
			vty_out(vty, " %% no NRI mappings%s", VTY_NEWLINE);
			return;
		}
	}

	llist_for_each_entry(r, &msc->nri_ranges, entry) {
		if (osmo_nri_range_validate(r, 255))
			vty_out(vty, " %% INVALID RANGE:");
		vty_out(vty, " nri add %d", r->first);
		if (r->first != r->last)
			vty_out(vty, " %d", r->last);
		vty_out(vty, "%s", VTY_NEWLINE);
	}
}

DEFUN(cfg_msc_show_nri, cfg_msc_show_nri_cmd,
      "show nri",
      SHOW_STR NRI_STR)
{
	struct bsc_msc_data *msc = bsc_msc_data(vty);
	msc_write_nri(vty, msc, true);
	return CMD_SUCCESS;
}

DEFUN(show_nri, show_nri_cmd,
      "show nri [" MSC_NR_RANGE "]",
      SHOW_STR NRI_STR "Optional MSC number to limit to\n")
{
	struct bsc_msc_data *msc;
	if (argc > 0) {
		int msc_nr = atoi(argv[0]);
		msc = osmo_msc_data_find(bsc_gsmnet, msc_nr);
		if (!msc) {
			vty_out(vty, "%% No such MSC%s", VTY_NEWLINE);
			return CMD_SUCCESS;
		}
		msc_write_nri(vty, msc, true);
		return CMD_SUCCESS;
	}

	llist_for_each_entry(msc, &bsc_gsmnet->mscs, entry) {
		msc_write_nri(vty, msc, true);
	}
	return CMD_SUCCESS;
}

int bsc_vty_init_extra(void)
{
	struct gsm_network *net = bsc_gsmnet;

	install_element(CONFIG_NODE, &cfg_net_msc_cmd);
	install_element(CONFIG_NODE, &cfg_net_bsc_cmd);

	install_node(&bsc_node, config_write_bsc);
	install_element(BSC_NODE, &cfg_net_bsc_mid_call_text_cmd);
	install_element(BSC_NODE, &cfg_net_bsc_mid_call_timeout_cmd);
	install_element(BSC_NODE, &cfg_net_rf_socket_cmd);
	install_element(BSC_NODE, &cfg_net_rf_off_time_cmd);
	install_element(BSC_NODE, &cfg_net_no_rf_off_time_cmd);
	install_element(BSC_NODE, &cfg_net_bsc_missing_msc_ussd_cmd);
	install_element(BSC_NODE, &cfg_net_bsc_no_missing_msc_text_cmd);

	install_node(&msc_node, config_write_msc);
	install_element(MSC_NODE, &cfg_net_bsc_ncc_cmd);
	install_element(MSC_NODE, &cfg_net_bsc_mcc_cmd);
	install_element(MSC_NODE, &cfg_net_bsc_lac_cmd);
	install_element(MSC_NODE, &cfg_net_bsc_ci_cmd);
	install_element(MSC_NODE, &cfg_net_bsc_rtp_base_cmd);
	install_element(MSC_NODE, &cfg_net_bsc_codec_list_cmd);
	install_element(MSC_NODE, &cfg_net_msc_dest_cmd);
	install_element(MSC_NODE, &cfg_net_msc_no_dest_cmd);
	install_element(MSC_NODE, &cfg_net_msc_welcome_ussd_cmd);
	install_element(MSC_NODE, &cfg_net_msc_no_welcome_ussd_cmd);
	install_element(MSC_NODE, &cfg_net_msc_lost_ussd_cmd);
	install_element(MSC_NODE, &cfg_net_msc_no_lost_ussd_cmd);
	install_element(MSC_NODE, &cfg_net_msc_grace_ussd_cmd);
	install_element(MSC_NODE, &cfg_net_msc_no_grace_ussd_cmd);
	install_element(MSC_NODE, &cfg_net_msc_type_cmd);
	install_element(MSC_NODE, &cfg_net_msc_emerg_cmd);
	install_element(MSC_NODE, &cfg_net_msc_amr_12_2_cmd);
	install_element(MSC_NODE, &cfg_net_msc_amr_10_2_cmd);
	install_element(MSC_NODE, &cfg_net_msc_amr_7_95_cmd);
	install_element(MSC_NODE, &cfg_net_msc_amr_7_40_cmd);
	install_element(MSC_NODE, &cfg_net_msc_amr_6_70_cmd);
	install_element(MSC_NODE, &cfg_net_msc_amr_5_90_cmd);
	install_element(MSC_NODE, &cfg_net_msc_amr_5_15_cmd);
	install_element(MSC_NODE, &cfg_net_msc_amr_4_75_cmd);
	install_element(MSC_NODE, &cfg_net_msc_amr_octet_align_cmd);
	install_element(MSC_NODE, &cfg_net_msc_lcls_mode_cmd);
	install_element(MSC_NODE, &cfg_net_msc_lcls_mismtch_cmd);
	install_element(MSC_NODE, &cfg_msc_cs7_bsc_addr_cmd);
	install_element(MSC_NODE, &cfg_msc_cs7_msc_addr_cmd);
	install_element(MSC_NODE, &cfg_msc_cs7_asp_proto_cmd);
	install_element(MSC_NODE, &cfg_msc_nri_add_cmd);
	install_element(MSC_NODE, &cfg_msc_nri_del_cmd);
	install_element(MSC_NODE, &cfg_msc_show_nri_cmd);

	/* Deprecated: ping time config, kept to support legacy config files. */
	install_element(MSC_NODE, &cfg_net_msc_no_ping_time_cmd);
	install_element(MSC_NODE, &cfg_net_msc_ping_time_cmd);
	install_element(MSC_NODE, &cfg_net_msc_pong_time_cmd);

	install_element_ve(&show_statistics_cmd);
	install_element_ve(&show_mscs_cmd);
	install_element_ve(&show_pos_cmd);
	install_element_ve(&logging_fltr_imsi_cmd);
	install_element_ve(&show_subscr_all_cmd);
	install_element_ve(&show_nri_cmd);

	install_element(ENABLE_NODE, &gen_position_trap_cmd);

	install_element(CFG_LOG_NODE, &logging_fltr_imsi_cmd);

	mgcp_client_vty_init(net, MSC_NODE, net->mgw.conf);
	install_element(MSC_NODE, &cfg_msc_mgw_x_osmo_ign_cmd);
	install_element(MSC_NODE, &cfg_msc_no_mgw_x_osmo_ign_cmd);
	install_element(MSC_NODE, &cfg_msc_osmux_cmd);

	return 0;
}
