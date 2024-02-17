
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>
#include <vector>
#include <CLI/CLI.hpp>

#include "libobmcjtag.hpp"
#include "libobmccpld.hpp"


int main(int argc, char** argv)
{
    uint8_t cpldType;

    CLI::App app{"Jtag Test Tool"};

    auto getIdCode = app.add_subcommand("id_code", "Get CPLD ID Code");
    getIdCode
        ->add_option("-c, --cpld", cpldType, "LATTICE=0")
        ->required();
	
    auto getUserCode = app.add_subcommand("user_code", "Get CPLD User Code");
    getUserCode
        ->add_option("-c, --cpld", cpldType, "LATTICE=0")
        ->required();

    CLI11_PARSE(app, argc, argv);

    if (cpldType != LATTICE) {
        std::cerr << "Wrong option: only support LATTICE cpld\n";
        return 0;
    }

	if (getIdCode->parsed())
    {
		getCpldIdCode(cpldType);
    }
    else if (getUserCode->parsed())
    {
        getCpldUserCode(cpldType);
    }

    return 0;
}