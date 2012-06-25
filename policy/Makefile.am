polkit_policydir = $(datadir)/polkit-1/actions
dist_polkit_policy_DATA =					\
	org.mate.color.policy

# You will need a recent intltool or the patch from this bug
# http://bugzilla.gnome.org/show_bug.cgi?id=462312
@INTLTOOL_POLICY_RULE@

EXTRA_DIST = org.mate.color.policy.in
DISTCLEANFILES = org.mate.color.policy

clean-local :
	rm -f *~

