# For CPLD images containing EBR_INIT data, the update-ebr-init feature must 
# be enabled to ensure the CPLD firmware checksum matches correctly.
EXTRA_OEMESON:append = " -Dupdate-ebr-init=enabled"
