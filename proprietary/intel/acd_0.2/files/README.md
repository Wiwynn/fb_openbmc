# CrashDump

The Eaglestream BMC CrashDump firmware is intended for use by internal
debuggers and external customers in order to correlate failures to known
signatures and/or as a first step in debugging new failure signatures on
Eaglestream platforms.

CrashDump is a method for the BMC to collect crash data from the CPU
immediately upon failure. This crash data can be then retrieved at a later time
by the users.

```
                                         _ _ _ _
                                        |   _ _ |_
              _ _ _ _ _ _               |_| DIMM  |
            |            |                |_ _ _ _|
            |  PCH       |                    |
            |_ _ _ _ _ _ |                    |
                                            _ _ _ _ _ _
                                          |            |
                                          |  CPU 1     |
     _ _ _ _ _ _ _ _ _                    |_ _ _ _ _ _ |
    |                 |  error signalling   +   |
    | BMC   _ _ _ _ _ |+-+-+-+-+-+-+-+-+-+-+-   |
    |     |          ||                     +   |
    |     | crashdump||_________________________|
    |     |_ _ _ _ _ ||      PECI           +   |
    |_ _ _ _ _ || _ _ |                    _+ _ |_ _ _
               ||                         |           |
               ||                         | CPU 2     |
             _ \/_ _ _                    |_ _ _ _ _ _|
            |  DRAM   |                       |
            |_ _ _ _ _|                       |
                                         _ _ _|_
                                        |   _ _ |_
                                        |_| DIMM  |
                                          |_ _ _ _|
```

## Documentation
Documentation for the project can be found in the [docs folder](docs/) of the repository.
  - [Build Instructions](docs/build_instructions.md)
  - [Build Dependencies](docs/dependencies.md) if building as stand alone
    application
  - [changelog](docs/changelog.md)
