#ifndef __BMC_PLAT_DEF_H__
#define __BMC_PLAT_DEF_H__

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include "infrastructure/debug.h"

extern std::string hdtSelName;
extern std::string hdtDbgReqName;
extern std::string PwrOkName;
extern std::string powerButtonName;
extern std::string resetButtonName;
extern std::string warmResetButtonName;
extern std::string fpgaReservedButtonName;
extern std::string jtagTRSTName;
extern uint8_t hostFruId;

class GpioHelper;
typedef std::shared_ptr<GpioHelper> GpioHelperPtr;
typedef std::vector<GpioHelperPtr> GpioHelperVec;


class GpioHelper
{
public:
    GpioHelper(const std::string& name): m_name(name) {}
    virtual ~GpioHelper() {}

    const std::string& name() {return m_name;}
    virtual int getValue() = 0;
    virtual bool setValue(int value) = 0;

    static GpioHelperVec helperVec;

protected:
    const std::string m_name;
};

class DummyGpio : public GpioHelper
{
public:
    DummyGpio(const std::string& name, uint8_t init_val):
        GpioHelper(name), m_val(init_val) {}

    virtual int getValue()
    {
        return m_val;
    }

    virtual bool setValue(int value)
    {
        m_val = value;
        return true;
    }
private:
    uint8_t m_val;
};

void plat_init(void);

#endif

