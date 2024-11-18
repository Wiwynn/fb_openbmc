#include "pldm-update.hpp"
#include "mmc-recovery.hpp"

#include <CLI/CLI.hpp>

int main(int argc, char** argv)
{
    CLI::App app{"PLDM update tool"};
    app.require_subcommand(1);

    auto file = std::string();
    auto recover_mmc_mode = false;

    auto update_ag = app.add_subcommand("ag", "Update Aegis image.");
    update_ag->add_option("<FILE>", file, "The file to update")->required();
    update_ag->add_flag("--recover-mmc", recover_mmc_mode, "Recover the MMC");
    update_ag->callback([&]() {
        return recover_mmc_mode ? recover_mmc(file) : pldm_update(file);
    });
    CLI11_PARSE(app, argc, argv);
    return 0;
}
