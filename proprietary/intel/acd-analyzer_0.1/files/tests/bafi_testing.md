# Prerequisites
* Robot Framework (https://robotframework.org/)
* default_device_map.json located in bafi directory

# About BAFI compliance tests
There are three tests for each processor and general tests, testing the binary
output against already existing and correct output. If the outputs are the same,
the tests will pass flawlessly. If the outputs differ you will be provided with
information about which field is different and what is the correct output. E.g.:
```
{
    {
        "Original crashdump": "PCI-to-PCI Bridge", 
        "Property": "TOR.socket0.Decoded.Port", 
        "Generated crashdump": "PCI0"
    }
}
```

# How to run BAFI compliance tests
From bafi directory execute:
### ICX
```
robot -d tests/robot_tests/ICX tests/robot_tests/ICX/icx_tests.robot
```
### CPX
```
robot -d tests/robot_tests/CPX tests/robot_tests/CPX/cpx_tests.robot
```
### SKX
```
robot -d tests/robot_tests/SKX tests/robot_tests/SKX/skx_tests.robot
```

Tests reports will be located in the directory provided after -d flag.
For example: tests/robot_tests/ICX.
### Passed tests
```
Icx Tests :: ICX crashdumps content tests                             | PASS |
 3 critical tests, 3 passed, 0 failed
 3 tests total, 3 passed, 0 failed
```
### Failed tests
```
Icx Tests :: ICX crashdumps content tests                             | FAIL |
 3 critical tests, 1 passed, 2 failed
 3 tests total, 1 passed, 2 failed
```
