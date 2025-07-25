include $(top_srcdir)/buildutils/config.mk

noinst_HEADERS = catch_child.h cray_energy.h mom_job_func.h mom_req_quejob.h requests.h tmsock_recov.h \
                 checkpoint.h mom_comm.h mom_main.h mom_server_lib.h rm_dep.h \
                 cray_cpa.h mom_inter.h mom_process_request.h pbs_demux.h start_exec.h


SUBDIRS = @PBS_MACH@

DIST_SUBDIRS = cygwin darwin linux solaris7

CLEANFILES = *.gcda *.gcno *.gcov

AM_CFLAGS = -I$(top_srcdir)/src/resmom/@PBS_MACH@ -DPBS_MOM \
	       -DDEMUX=\"$(program_prefix)$(DEMUX_PATH)$(program_suffix)\" \
	       -DRCP_PATH=\"$(program_prefix)$(RCP_PATH)$(program_suffix)\" \
	       -DRCP_ARGS=\"$(RCP_ARGS)\" \
	       -DPBS_SERVER_HOME=\"$(PBS_SERVER_HOME)\" -DPBS_ENVIRON=\"$(PBS_ENVIRON)\" \
	       `xml2-config --cflags`
AM_LIBS   = `xml2-config --libs`

AM_CPPFLAGS += -I$(top_srcdir)/src/resmom/@PBS_MACH@ -DPBS_MOM \
	       -DDEMUX=\"$(program_prefix)$(DEMUX_PATH)$(program_suffix)\" \
	       -DRCP_PATH=\"$(program_prefix)$(RCP_PATH)$(program_suffix)\" \
	       -DRCP_ARGS=\"$(RCP_ARGS)\" \
	       -DPBS_SERVER_HOME=\"$(PBS_SERVER_HOME)\" -DPBS_ENVIRON=\"$(PBS_ENVIRON)\" \
	       `xml2-config --cflags`

PBS_LIBS = ../lib/Libattr/libattr.a \
	   ../lib/Libsite/libsite.a \
		 ../lib/Libutils/libutils.a \
	   ../lib/Libpbs/libtorque.la

EXTRA_DIST = rm_dep.h
sbin_PROGRAMS = pbs_mom pbs_demux

LDADD = @PBS_MACH@/libmommach.a $(MOMLIBS) $(PBS_LIBS) $(ALPS_LIBS) $(HWLOC_LIBS)

pbs_mom_SOURCES = catch_child.c mom_comm.c mom_inter.c mom_main.c	\
		   mom_server.c prolog.c requests.c start_exec.c	\
		   start_exec.h checkpoint.c tmsock_recov.c		\
		   mom_req_quejob.c mom_job_func.c trq_cgroups.c			\
		   mom_process_request.c alps_reservations.c		\
		   release_reservation.c generate_alps_status.c	\
		   parse_config.c node_frequency.cpp cray_energy.c \
			 accelerators_numa.cpp pmix_interface.c pmix_tracker.cpp \
			 pmix_operation.cpp cray_taskstats.cpp \
		   ../server/attr_recov.c ../server/dis_read.c		\
		   ../server/job_attr_def.c ../server/job_recov.c	\
		   ../server/reply_send.c ../server/resc_def_all.c	\
		   ../server/job_qs_upgrade.c
if BUILDCPA
pbs_mom_SOURCES += cray_cpa.c
endif

if MIC
pbs_mom_SOURCES += mic.c
endif

if NVIDIA
pbs_mom_SOURCES += nvidia.c
endif

pbs_demux_SOURCES = pbs_demux.c
# use LDADD here so that we don't have to link MOM with libcr
pbs_demux_LDADD = $(BLCR_LDFLAGS) @PBS_MACH@/libmommach.a $(PBS_LIBS)
pbs_demux_CPPFLAGS = $(BLCR_CPPFLAGS) $(AM_CPPFLAGS)

install-exec-hook:
	$(PBS_MKDIRS) aux || :
	$(PBS_MKDIRS) mom
	rm -f $(DESTDIR)$(sbindir)/$(program_prefix)qnoded$(program_suffix)$(EXEEXT)
	ln -s $(program_prefix)pbs_mom$(program_suffix)$(EXEEXT) \
	  $(DESTDIR)$(sbindir)/$(program_prefix)qnoded$(program_suffix)$(EXEEXT)
	if [ $(FROM_TORQUE_BUILD)0 -eq 0 ]; then \
	  if systemctl 2>/dev/null | grep -- -\.mount >/dev/null 2>&1; then \
	    if [ -f $(top_srcdir)/contrib/systemd/pbs_mom.service ]; then \
	      echo "$(install_sh) -m 644 $(top_srcdir)/contrib/systemd/pbs_mom.service $(DESTDIR)/usr/lib/systemd/system/pbs_mom.service"; \
	      $(install_sh) -m 644 $(top_srcdir)/contrib/systemd/pbs_mom.service $(DESTDIR)/usr/lib/systemd/system/pbs_mom.service; \
	    fi \
	  elif [ -f $(top_srcdir)/contrib/init.d/pbs_mom ]; then \
	    prefix=''; \
            if [ "`$(top_srcdir)/buildutils/determine.distro`" = sles ]; then \
	      prefix=suse. ; \
	    fi; \
	    echo "$(install_sh) -m 755 $(top_srcdir)/contrib/init.d/$${prefix}pbs_mom $(DESTDIR)/etc/init.d/pbs_mom"; \
	    $(install_sh) -m 755 $(top_srcdir)/contrib/init.d/$${prefix}pbs_mom $(DESTDIR)/etc/init.d/pbs_mom; \
	  fi; \
	  $(install_sh) -d -m 755 $(DESTDIR)/etc/ld.so.conf.d || :; \
	  if [ ! -f $(DESTDIR)/etc/ld.so.conf.d/torque.conf ]; then \
	    echo $(libdir) > $(DESTDIR)/etc/ld.so.conf.d/torque.conf; \
	    chmod 644 $(DESTDIR)/etc/ld.so.conf.d/torque.conf; \
	    if test $(FROM_MAKE_PACKAGES)0 -eq 0 && test `id -u` -eq 0; then \
	      /sbin/ldconfig; \
	    fi; \
	  fi; \
	fi

uninstall-hook:
	rm -f $(DESTDIR)$(PBS_ENVIRON)
	rm -f $(DESTDIR)$(sbindir)/$(program_prefix)qnoded$(program_suffix)$(EXEEXT)
	if [ $(FROM_TORQUE_BUILD)0 -eq 0 ]; then \
	  rm -f $(DESTDIR)/usr/lib/systemd/system/pbs_mom.service || :; \
	  rm -f $(DESTDIR)/etc/init.d/pbs_mom || :; \
	  rm -f $(DESTDIR)/etc/ld.so.conf.d/torque.conf || :; \
	  if test $(FROM_MAKE_PACKAGES)0 -eq 0 && test `id -u` -eq 0; then \
	    /sbin/ldconfig; \
	  fi; \
	fi

