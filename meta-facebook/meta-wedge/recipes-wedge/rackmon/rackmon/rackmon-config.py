from rackmond import configure_rackmond

reglist = [
    {"begin": 0x0, #MFR_MODEL
     "length": 8},
    {"begin": 0x10, #MFR_DATE
     "length": 8},
    {"begin": 0x20, #FB Part #
     "length": 8},
    {"begin": 0x30, #HW Revision
     "length": 4},
    {"begin": 0x38, #FW Revision
     "length": 4},
    {"begin": 0x40, #MFR Serial #
     "length": 16},
    {"begin": 0x60, #Workorder #
     "length": 4},
    {"begin": 0x68, #PSU Status
     "length": 1,
     "keep": 10,   # 10-sample ring buffer
     "flags": 1},
    {"begin": 0x69, #Battery Status
     "length": 1,
     "keep": 10,   # 10-sample ring buffer
     "flags": 1},
    {"begin": 0x80, #Input VAC
     "length": 1,
     "keep": 10},
    {"begin": 0x82, #Input Current AC
     "length": 1,
     "keep": 10},
    {"begin": 0x84, #Battery Voltage
     "length": 1,
     "keep": 10},
    {"begin": 0x86, #Battery Current Output
     "length": 1},
    {"begin": 0x88, #Battery Current Input
     "length": 1},
    {"begin": 0x8A, #Output Voltage (main converter)
     "length": 1,
     "keep": 10},
    {"begin": 0x8C, #Output Current (main converter)
     "length": 1,
     "keep": 10},
    {"begin": 0x8E, #IT Load Voltage Output
     "length": 1},
    {"begin": 0x90, #IT Load Current Output
     "length": 1},
    {"begin": 0x92, #Bulk Cap Voltage
     "length": 1},
    {"begin": 0x94, #Input Power
     "length": 1,
     "keep": 10},
    {"begin": 0x96, #Output Power
     "length": 1,
     "keep": 10},
    {"begin": 0x98, #RPM Fan 0
     "length": 1},
    {"begin": 0x9A, #RPM Fan 1
     "length": 1},
    {"begin": 0x9E, #Temp 0
     "length": 1},
    {"begin": 0xA0, #Temp 1
     "length": 1},
]

def main():
    configure_rackmond(reglist)

if __name__ == "__main__":
    main()
