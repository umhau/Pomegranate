##
# Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
#                           <macan@ncic.ac.cn>
#
# Time-stamp: <2012-08-10 13:57:59 macan>
#
# This is the makefile for HVFS project.
#
# Armed by EMACS.

HOME_PATH = $(shell pwd)

include Makefile.inc

RING_SOURCES = $(LIB_PATH)/ring.c $(LIB_PATH)/lib.c $(LIB_PATH)/hash.c \
				$(LIB_PATH)/xlock.c

all : unit_test lib triggers

$(HVFS_LIB) : $(lib_depend_files)
	@$(ECHO) -e " " CD"\t" $(LIB_PATH)
	@$(ECHO) -e " " MK"\t" $@
	@$(MAKE) --no-print-directory -C $(LIB_PATH) -e "HOME_PATH=$(HOME_PATH)"

$(MDS_LIB) : $(mds_depend_files)
	@$(ECHO) -e " " CD"\t" $(MDS)
	@$(ECHO) -e " " MK"\t" $@
	@$(MAKE) --no-print-directory -C $(MDS) -e "HOME_PATH=$(HOME_PATH)"

$(MDSL_LIB) : $(mdsl_depend_files)
	@$(ECHO) -e " " CD"\t" $(MDSL)
	@$(ECHO) -e " " MK"\t" $@
	@$(MAKE) --no-print-directory -C $(MDSL) -e "HOME_PATH=$(HOME_PATH)"

$(OSD_LIB) : $(osd_depend_files)
	@$(ECHO) -e " " CD"\t" $(OSD)
	@$(ECHO) -e " " MK"\t" $@
	@$(MAKE) --no-print-directory -C $(OSD) -e "HOME_PATH=$(HOME_PATH)"

$(R2_LIB) : $(r2_depend_files)
	@$(ECHO) -e " " CD"\t" $(R2)
	@$(ECHO) -e " " MK"\t" $@
	@$(MAKE) --no-print-directory -C $(R2) -e "HOME_PATH=$(HOME_PATH)"

$(XNET_LIB) : $(xnet_depend_files)
	@$(ECHO) -e " " CD"\t" $(XNET)
	@$(ECHO) -e " " MK"\t" $@
	@$(MAKE) --no-print-directory -C $(XNET) -e "HOME_PATH=$(HOME_PATH)"

$(API_LIB) : $(api_depend_files)
	@$(ECHO) -e " " CD"\t" $(API)
	@$(ECHO) -e " " MK"\t" $@
	@$(MAKE) --no-print-directory -C $(API) -e "HOME_PATH=$(HOME_PATH)"

$(BRANCH_LIB) : $(branch_depend_files)
	@$(ECHO) -e " " CD"\t" $(BRANCH)
	@$(ECHO) -e " " MK"\t" $@
	@$(MAKE) --no-print-directory -C $(BRANCH) -e "HOME_PATH=$(HOME_PATH)"

ifdef USE_FUSE
$(FUSE_LIB) : $(fuse_depend_files)
	@$(ECHO) -e " " CD"\t" $(FUSE)
	@$(ECHO) -e " " MK"\t" $@
	@$(MAKE) --no-print-directory -C $(FUSE) -e "HOME_PATH=$(HOME_PATH)"
else
$(FUSE_LIB) : $(fuse_depend_files)
	@$(ECHO) -e " " MK"\t" $@ " (Ignored! Use 'USE_FUSE=1' to enable fuse support.)"
endif

triggers : $(triggers_depend_files) build_triggers
	@$(ECHO) "Triggers' dynamic library are ready."

build_triggers : 
	@$(ECHO) -e " " CD"\t" $(TRIGGERS)
	@$(ECHO) -e " " MK"\t" $@
	@$(MAKE) --no-print-directory -C $(TRIGGERS) -e "HOME_PATH=$(HOME_PATH)"

clean :
	@$(MAKE) --no-print-directory -C $(LIB_PATH) -e "HOME_PATH=$(HOME_PATH)" clean
	@$(MAKE) --no-print-directory -C $(MDS) -e "HOME_PATH=$(HOME_PATH)" clean
	@$(MAKE) --no-print-directory -C $(MDSL) -e "HOME_PATH=$(HOME_PATH)" clean
	@$(MAKE) --no-print-directory -C $(OSD) -e "HOME_PATH=$(HOME_PATH)" clean
	@$(MAKE) --no-print-directory -C $(R2) -e "HOME_PATH=$(HOME_PATH)" clean
	@$(MAKE) --no-print-directory -C $(API) -e "HOME_PATH=$(HOME_PATH)" clean
	@$(MAKE) --no-print-directory -C $(BRANCH) -e "HOME_PATH=$(HOME_PATH)" clean
	@$(MAKE) --no-print-directory -C $(XNET) -e "HOME_PATH=$(HOME_PATH)" clean
	@$(MAKE) --no-print-directory -C $(TEST)/mds -e "HOME_PATH=$(HOME_PATH)" clean
	@$(MAKE) --no-print-directory -C $(TEST)/mdsl -e "HOME_PATH=$(HOME_PATH)" clean
	@$(MAKE) --no-print-directory -C $(TEST)/xnet -e "HOME_PATH=$(HOME_PATH)" clean
	@$(MAKE) --no-print-directory -C $(TEST)/fuse -e "HOME_PATH=$(HOME_PATH)" clean
	@$(MAKE) --no-print-directory -C $(TRIGGERS) -e "HOME_PATH=$(HOME_PATH)" clean
	@$(MAKE) --no-print-directory -C $(FUSE) -e "HOME_PATH=$(HOME_PATH)" clean
	-@rm -rf $(LIB_PATH)/ring $(LIB_PATH)/a.out

depclean:
	@$(MAKE) --no-print-directory -C $(TEST)/result -e "HOME_PATH=$(HOME_PATH)" clean

help :
	@$(ECHO) "Environment Variables:"
	@$(ECHO) ""
	@$(ECHO) "1. USE_BDB           if defined, compile w/ BerkeleyDB support;"
	@$(ECHO) "                     otherwise, use plain file."
	@$(ECHO) ""
	@$(ECHO) "2. DISABLE_PYTHON    if defined, do not compile w/ Python C API."
	@$(ECHO) "                     otherwise, compile and link with libpython."
	@$(ECHO) ""
	@$(ECHO) "3. JEMALLOC          Must defined w/ jemalloc install path prefix;"
	@$(ECHO) "                     otherwise, we can find the jemalloc lib path."
	@$(ECHO) ""
	@$(ECHO) "4. USE_FUSE          if defined, link with libfuse;"
	@$(ECHO) "                     otherwise, ignore fuse client."
	@$(ECHO) ""
	@$(ECHO) "5. PYTHON_INC        python include path"
	@$(ECHO) ""
	@$(ECHO) "6. BDB_HOME          BerkeleyDB install path prefix."

# Note: the following region is only for UNIT TESTing
# region for unit test
$(LIB_PATH)/ring : $(RING_SOURCES)
	@$(ECHO) -e " " CC"\t" $@
	@$(CC) $(CFLAGS) $^ -o $@ -DUNIT_TEST

lib : $(HVFS_LIB) $(MDS_LIB) $(XNET_LIB) $(MDSL_LIB) $(R2_LIB) $(API_LIB) $(BRANCH_LIB) $(FUSE_LIB) $(OSD_LIB)
	@$(ECHO) -e " " Lib is ready.

unit_test : $(ut_depend_files) $(HVFS_LIB) $(MDS_LIB) $(XNET_LIB) \
			$(MDSL_LIB) $(R2_LIB) $(API_LIB) $(BRANCH_LIB) $(FUSE_LIB) $(OSD_LIB)
	@$(ECHO) -e " " CD"\t" $(TEST)/mds
	@$(MAKE) --no-print-directory -C $(TEST)/mds -e "HOME_PATH=$(HOME_PATH)"
	@$(ECHO) -e " " CD"\t" $(TEST)/xnet
	@$(MAKE) --no-print-directory -C $(TEST)/xnet -e "HOME_PATH=$(HOME_PATH)"
	@$(ECHO) -e " " CD"\t" $(TEST)/mdsl
	@$(MAKE) --no-print-directory -C $(TEST)/mdsl -e "HOME_PATH=$(HOME_PATH)"
	@$(ECHO) -e " " CD"\t" $(TEST)/fuse
	@$(MAKE) --no-print-directory -C $(TEST)/fuse -e "HOME_PATH=$(HOME_PATH)"
	@$(ECHO) "Targets for unit test are ready."

install: unit_test triggers
	@rsync -r $(TEST)/*.sh root@glnode09:~/hvfs/test/
	@rsync -r $(CONF) root@glnode09:~/hvfs/
	@rsync -r $(BIN) root@glnode09:~/hvfs/
	@rsync -r $(TRIGGERS) root@glnode09:~/hvfs/
	@rsync -r $(LIB_PATH)/*.so.1.0 root@glnode09:~/hvfs/lib/
	@rsync -r $(TEST)/mds/*.ut root@glnode09:~/hvfs/test/mds/
	@rsync -r $(TEST)/xnet/*.ut root@glnode09:~/hvfs/test/xnet/
	@rsync -r $(TEST)/mdsl/*.ut root@glnode09:~/hvfs/test/mdsl/
	@rsync -r $(TEST)/fuse/*.ut root@glnode09:~/hvfs/test/fuse/
	@rsync -r $(TEST)/bdb/* root@glnode09:~/hvfs/test/bdb/
	@rsync -r $(TEST)/python/*.py root@glnode09:~/hvfs/test/python/
	@$(ECHO) "Install done."

xinstall: unit_test
	@rsync -r $(TEST)/*.sh root@10.10.104.1:/home/macan/test/
	@rsync -r $(CONF) root@10.10.104.1:/home/macan/
	@rsync -r $(BIN) root@10.10.104.1:/home/macan/
	@rsync -r $(TEST)/mds/*.ut root@10.10.104.1:/home/macan/test/mds/
	@rsync -r $(TEST)/xnet/*.ut root@10.10.104.1:/home/macan/test/xnet/
	@rsync -r $(TEST)/mdsl/*.ut root@10.10.104.1:/home/macan/test/mdsl/
	@$(ECHO) "Install done."

plot: 
	@$(ECHO) -e "Ploting ..."
	@$(MAKE) --no-print-directory -C $(TEST)/result -e "HOME_PATH=$(HOME_PATH)" plot
	@$(ECHO) -e "Done.\n"

rut:
	@lagent -d glnode09 -u root -sc "time ~/cbht $(CBHT_ARGS)"
	@lagent -d glnode09 -u root -sc "gprof ~/cbht"
