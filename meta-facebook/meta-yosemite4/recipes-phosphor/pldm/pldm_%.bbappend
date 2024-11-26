FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

EXTRA_OEMESON:append = " \
  -Dtransport-implementation=af-mctp \
  -Dmaximum-transfer-size=150 \
  -Dsensor-polling-time=2000 \
  -Ddbus-timeout-value=10 \
  -Dinstance-id-expiration-interval=6 \
"
