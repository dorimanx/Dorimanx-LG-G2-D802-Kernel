ccsecurity-objs := permission.o gc.o memory.o policy_io.o realpath.o

ifeq ($(VERSION)$(PATCHLEVEL),24)

ifdef CONFIG_CCSECURITY
O_TARGET := ccsecurity.o
ifdef CONFIG_CCSECURITY_LKM
all_targets: load_policy.o
obj-m := ccsecurity.o
obj-y := $(ccsecurity-objs)
else
all_targets: ccsecurity.o
obj-y := load_policy.o $(ccsecurity-objs)
endif
export-objs := load_policy.o
endif
include $(TOPDIR)/Rules.make

policy/profile.conf:
	@mkdir -p policy/
	@echo Creating an empty policy/profile.conf
	@touch $@

policy/exception_policy.conf:
	@mkdir -p policy/
	@echo Creating a default policy/exception_policy.conf
	@touch $@

policy/domain_policy.conf:
	@mkdir -p policy/
	@echo Creating an empty policy/domain_policy.conf
	@touch $@

policy/manager.conf:
	@mkdir -p policy/
	@echo Creating an empty policy/manager.conf
	@touch $@

policy/stat.conf:
	@mkdir -p policy/
	@echo Creating an empty policy/stat.conf
	@touch $@

builtin-policy.h: policy/profile.conf policy/exception_policy.conf policy/domain_policy.conf policy/manager.conf policy/stat.conf
	@echo Generating built-in policy for TOMOYO 1.8.x.
	@echo "static char ccs_builtin_profile[] __initdata =" > $@.tmp
	@sed -e 's/\\/\\\\/g' -e 's/\"/\\"/g' -e 's/\(.*\)/"\1\\n"/' < policy/profile.conf >> $@.tmp
	@echo "\"\";" >> $@.tmp
	@echo "static char ccs_builtin_exception_policy[] __initdata =" >> $@.tmp
	@sed -e 's/\\/\\\\/g' -e 's/\"/\\"/g' -e 's/\(.*\)/"\1\\n"/' < policy/exception_policy.conf >> $@.tmp
	@echo "\"\";" >> $@.tmp
	@echo "static char ccs_builtin_domain_policy[] __initdata =" >> $@.tmp
	@sed -e 's/\\/\\\\/g' -e 's/\"/\\"/g' -e 's/\(.*\)/"\1\\n"/' < policy/domain_policy.conf >> $@.tmp
	@echo "\"\";" >> $@.tmp
	@echo "static char ccs_builtin_manager[] __initdata =" >> $@.tmp
	@sed -e 's/\\/\\\\/g' -e 's/\"/\\"/g' -e 's/\(.*\)/"\1\\n"/' < policy/manager.conf >> $@.tmp
	@echo "\"\";" >> $@.tmp
	@echo "static char ccs_builtin_stat[] __initdata =" >> $@.tmp
	@sed -e 's/\\/\\\\/g' -e 's/\"/\\"/g' -e 's/\(.*\)/"\1\\n"/' < policy/stat.conf >> $@.tmp
	@echo "\"\";" >> $@.tmp
	@mv $@.tmp $@

policy_io.o: builtin-policy.h

else

obj-y += load_policy.o
ifdef CONFIG_CCSECURITY_LKM
obj-m += ccsecurity.o
else
obj-y += ccsecurity.o
endif

$(obj)/policy/profile.conf:
	@mkdir -p $(obj)/policy/
	@echo Creating an empty policy/profile.conf
	@touch $@

$(obj)/policy/exception_policy.conf:
	@mkdir -p $(obj)/policy/
	@echo Creating a default policy/exception_policy.conf
	@touch $@

$(obj)/policy/domain_policy.conf:
	@mkdir -p $(obj)/policy/
	@echo Creating an empty policy/domain_policy.conf
	@touch $@

$(obj)/policy/manager.conf:
	@mkdir -p $(obj)/policy/
	@echo Creating an empty policy/manager.conf
	@touch $@

$(obj)/policy/stat.conf:
	@mkdir -p $(obj)/policy/
	@echo Creating an empty policy/stat.conf
	@touch $@

$(obj)/builtin-policy.h: $(obj)/policy/profile.conf $(obj)/policy/exception_policy.conf $(obj)/policy/domain_policy.conf $(obj)/policy/manager.conf $(obj)/policy/stat.conf
	@echo Generating built-in policy for TOMOYO 1.8.x.
	@echo "static char ccs_builtin_profile[] __initdata =" > $@.tmp
	@sed -e 's/\\/\\\\/g' -e 's/\"/\\"/g' -e 's/\(.*\)/"\1\\n"/' < $(obj)/policy/profile.conf >> $@.tmp
	@echo "\"\";" >> $@.tmp
	@echo "static char ccs_builtin_exception_policy[] __initdata =" >> $@.tmp
	@sed -e 's/\\/\\\\/g' -e 's/\"/\\"/g' -e 's/\(.*\)/"\1\\n"/' < $(obj)/policy/exception_policy.conf >> $@.tmp
	@echo "\"\";" >> $@.tmp
	@echo "static char ccs_builtin_domain_policy[] __initdata =" >> $@.tmp
	@sed -e 's/\\/\\\\/g' -e 's/\"/\\"/g' -e 's/\(.*\)/"\1\\n"/' < $(obj)/policy/domain_policy.conf >> $@.tmp
	@echo "\"\";" >> $@.tmp
	@echo "static char ccs_builtin_manager[] __initdata =" >> $@.tmp
	@sed -e 's/\\/\\\\/g' -e 's/\"/\\"/g' -e 's/\(.*\)/"\1\\n"/' < $(obj)/policy/manager.conf >> $@.tmp
	@echo "\"\";" >> $@.tmp
	@echo "static char ccs_builtin_stat[] __initdata =" >> $@.tmp
	@sed -e 's/\\/\\\\/g' -e 's/\"/\\"/g' -e 's/\(.*\)/"\1\\n"/' < $(obj)/policy/stat.conf >> $@.tmp
	@echo "\"\";" >> $@.tmp
	@mv $@.tmp $@

$(obj)/policy_io.o: $(obj)/builtin-policy.h

endif
