################################################################################
# TE driver default configurations
################################################################################

# Build system top directory
TOP_DIR             ?= $(shell pwd -P | sed "s@/shared/.*@@g")

# TE code name
TE_NAME             ?= merak

# TE unit test flag: y|n
TE_UNIT_TEST        ?= n

# TE HWA debug flag: y|n
TE_HWA_DEBUG        ?= n

# TE OTP PUF flag: y|n. PUF=1 for y. Or n otherwise.
TE_OTP_PUF          ?= $(if $(filter x1,x$(PUF)),y,n)

# TE OTP PUF standby enable: y|n. PUF=1 for y. Or n otherwise.
TE_OTP_PUF_STB_EN   ?= $(if $(filter x1,x$(PUF)),y,n)

# printf equivalent function
TE_PRINT            ?= osal_log_printf

# TE worker pool size
TE_NUM_WORKERS      ?= 8

# TE dynamic clock control flag: y|n. RPM=1 for y. Or n otherwise.
TE_DYNCLK_CTL       ?= $(if $(filter x1,x$(RPM)),y,n)

# TE ACA Blinding flag: y|n
ACA_BLINDING_EN     ?= n

# TE KASUMI flag: y|n
TE_KASUMI     ?= n

# TE SNOW3G flag: y|n
TE_SNOW3G     ?= n

# TE ZUC flag: y|n
TE_ZUC     ?= n
