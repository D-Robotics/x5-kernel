#ifndef __DT_BINDINGS_DROBOT_FIREWALL_H__
#define __DT_BINDINGS_DROBOT_FIREWALL_H__

#define CPU_NOC_A55_PERI       1
#define HSIO_NOC_DAP_M         2
#define HSIO_NOC_DMA_M         3
#define HSIO_NOC_TESTPORT_M    4
#define HIFI5_NOC_DSP_DMA      5
#define HIFI5_NOC_HIFI5_P0     6
#define CPU_NOC_A55_ACE0       7
#define CPU_NOC_A55_ACE1       8
#define HSIO_NOC_ETR_M         9
#define HSIO_NOC_EMMC_M        10
#define HSIO_NOC_GMAC_M        11
#define HSIO_NOC_SD_M          12
#define HSIO_NOC_SDIO_M        13
#define HSIO_NOC_SECURITY_M    14
#define HSIO_NOC_USB2_M        15
#define HSIO_NOC_USB3_M        16
#define BPU_NOC_BPU            17
#define VIN_NOC_DW200_AXI0     18
#define VIN_NOC_DW200_AXI1     19
#define VIN_NOC_DW200_AXI2     20
#define VIN_NOC_ISP_AXI1       21
#define VIN_NOC_ISP_AXI3       22
#define VIN_NOC_ISP_AXI4       23
#define VIN_NOC_ISP_AXI5       24
#define VIN_NOC_SIF0           25
#define VIN_NOC_SIF1           26
#define VIN_NOC_SIF2           27
#define VIN_NOC_SIF3           28
#define VIN_NOC_BT1120         29
#define VIN_NOC_DC8000         30
#define VIN_NOC_SIF_DISP       31
#define CODEC_NOC_VIDEO_CODEC  32
#define CODEC_NOC_JPEG_CODEC   33
#define GPU_NOC_GPU_3D_MST0    34
#define GPU_NOC_GPU_3D_MST1    35
#define GPU_NOC_GPU_2D         36

#define A55_NSAID_1   (~((1U << CPU_NOC_A55_ACE0) | (1U << CPU_NOC_A55_ACE1) | (1U << CPU_NOC_A55_PERI)))
#define BPU_NSAID_1   (~(1U << BPU_NOC_BPU))
#define VIDEO_NSAID_2 (~((1U << (CODEC_NOC_VIDEO_CODEC - 32)) | (1U << (CODEC_NOC_JPEG_CODEC - 32))))
#define GPU_NSAID_2                                                                          \
    ~((1U << (GPU_NOC_GPU_3D_MST0 - 32)) | (1U << (GPU_NOC_GPU_3D_MST1 - 32)) |               \
     (1U << (GPU_NOC_GPU_2D - 32)))

#define CAMERA_NSAID_1                                                                       \
    ~(1U << VIN_NOC_DW200_AXI0 | 1U << VIN_NOC_DW200_AXI1 | 1U << VIN_NOC_DW200_AXI2 |        \
     1U << VIN_NOC_ISP_AXI1 | 1U << VIN_NOC_ISP_AXI3 | 1U << VIN_NOC_ISP_AXI4 |              \
     1U << VIN_NOC_ISP_AXI5 | 1U << VIN_NOC_SIF0 | 1U << VIN_NOC_SIF1 | 1U << VIN_NOC_SIF2 | \
     1U << VIN_NOC_SIF3 | 1U << VIN_NOC_BT1120 | 1U << VIN_NOC_DC8000 | 1U << VIN_NOC_SIF_DISP)
#define DSP_NSAID_1 ~(1U << HIFI5_NOC_DSP_DMA)
#define DSP_NSAID_2 ~(1U << (HIFI5_NOC_HIFI5_P0_MODIFIED - 32))

#define HORIZON_DDR_MPU_0             (0x38500000)
#define HORIZON_DDR_MPU_1             (0x38600000)
#define HORIZON_DDR_MPU_2             (0x38700000)
#define HORIZON_DDR_MPU_3             (0x38800000)
#define HORIZON_DDR_MPU_4             (0x38900000)
#endif
