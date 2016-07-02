#!/usr/bin/make -f
# Waf Makefile wrapper
WAF_HOME=/Users/mkasa/rep/klabinternal

all:
#@/Users/mkasa/rep/klabinternal/waf build

all-debug:
	@/Users/mkasa/rep/klabinternal/waf -v build

all-progress:
	@/Users/mkasa/rep/klabinternal/waf -p build

install:
	/Users/mkasa/rep/klabinternal/waf install --yes;

uninstall:
	/Users/mkasa/rep/klabinternal/waf uninstall

clean:
	@/Users/mkasa/rep/klabinternal/waf clean

distclean:
	@/Users/mkasa/rep/klabinternal/waf distclean
	@-rm -rf build
	@-rm -f Makefile

check:
	@/Users/mkasa/rep/klabinternal/waf check

dist:
	@/Users/mkasa/rep/klabinternal/waf dist

.PHONY: clean dist distclean check uninstall install all

