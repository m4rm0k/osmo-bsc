AM_CPPFLAGS = \
	$(all_includes) \
	-I$(top_srcdir)/include \
	$(NULL)

AM_CFLAGS = \
	-Wall \
	$(LIBOSMOCORE_CFLAGS) \
	$(LIBOSMOGSM_CFLAGS) \
	$(LIBOSMOABIS_CFLAGS) \
	$(NULL)

noinst_PROGRAMS = \
	gsm0408_test \
	$(NULL)

EXTRA_DIST = \
	gsm0408_test.ok \
	$(NULL)

gsm0408_test_SOURCES = \
	gsm0408_test.c \
	$(NULL)

gsm0408_test_LDADD = \
	$(top_builddir)/src/osmo-bsc/osmo_bsc-gsm_04_08_rr.o \
	$(top_builddir)/src/osmo-bsc/osmo_bsc-arfcn_range_encode.o \
	$(top_builddir)/src/osmo-bsc/osmo_bsc-rest_octets.o \
	$(top_builddir)/src/osmo-bsc/osmo_bsc-system_information.o \
	$(top_builddir)/src/osmo-bsc/osmo_bsc-neighbor_ident.o \
	$(top_builddir)/src/osmo-bsc/libbscbts.la \
	$(LIBOSMOCORE_LIBS) \
	$(LIBOSMOGSM_LIBS) \
	$(LIBOSMOABIS_LIBS) \
	$(NULL)
