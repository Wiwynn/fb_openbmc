#include "plat_def.h"

std::string hdtSelName = "HDT_SEL";
// std::string jtagMuxOEName = "JTAG_MUX_OE";
// std::string jtagMuxSName = "JTAG_MUX_S";
std::string hdtDbgReqName = "HDT_DBREQ";
std::string PwrOkName = "MON_PWROK";
// std::string boardID0Name = "BRD_ID_0";
// std::string boardID1Name = "BRD_ID_1";
// std::string boardID2Name = "BRD_ID_2";
// std::string boardID3Name = "BRD_ID_3";
std::string powerButtonName = "ASSERT_PWR_BTN";
std::string resetButtonName = "ASSERT_RST_BTN";
std::string warmResetButtonName = "ASERT_WARM_RST_BTN";
std::string fpgaReservedButtonName = "FPGA_RSVD";
std::string jtagTRSTName = "JTAG_TRST_N";
uint8_t hostFruId = 1;

// int plat_board_id = 0;

GpioHelperVec GpioHelper::helperVec;

void __attribute__((weak))
plat_init(void)
{
    // overwrite this function to modify gpio shadows name and board id in
    // platform layer
}