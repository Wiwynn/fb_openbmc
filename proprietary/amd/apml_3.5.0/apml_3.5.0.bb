SUMMARY = "AMD EPYC System Management Interface Library"
DESCRIPTION = "AMD EPYC System Management Interface Library for user space APML implementation"

LICENSE = "NCSA"
LIC_FILES_CHKSUM = "file://License.txt;md5=a53f186511a093774907861d15f7014c"

SRC_URI =" file://apml  \
                     "

S = "${WORKDIR}/apml"

inherit cmake pkgconfig