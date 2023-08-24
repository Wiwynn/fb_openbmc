bic_jtag_src = []
bic_jtag_dep = []
plat_src = []
plat_dep = []

subdir('plat')

if (get_option('bic-jtag').enabled())
	bic_jtag_src += 'hw/bic_jtag_handler.cpp'
	bic_jtag_dep += dependency('libbic')
endif

executable(
    'yaapd',
    'main.cpp',
	'hw/csjtag_hw.cpp',
	'hw/gpuDebug_hw.cpp',
	'hw/hdt_hw.cpp',
	'hw/header_hw.cpp',
	'hw/i2c_hw.cpp',
	'hw/jtag_hw.cpp',
	'hw/lpcPostCode_hw.cpp',
	'hw/lpcRomEmu_hw.cpp',
	'hw/mux_hw.cpp',
	'hw/psp_sfmon.cpp',
	'hw/relay_hw.cpp',
	'hw/system_hw.cpp',
	'hw/triggers_hw.cpp',
	'plat/plat_def.cpp',
	bic_jtag_src,
	plat_src,

    dependencies: [
        gpiodcxx_dep,
        yaap_shared_dep,
		bic_jtag_dep,
		plat_dep,
    ],
    install: true,
)
