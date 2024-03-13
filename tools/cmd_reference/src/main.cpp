#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

#include <xen/xen_command_tree.hpp>

#include "make_command_reference.hpp"

/**
 * Generate a markdown document containing a reference for all commands
 */
int main(int argc, char const *argv[])
{
    try
    {
        auto const output_dir =
            argc > 1 ? argv[1]
                     : throw std::runtime_error{
                           "Error: output directory not specified in command line.\n"
                           "Usage: cmd_reference <output_dir>"};

        constexpr auto filename = "command_reference.md";
        auto const output_path = std::filesystem::path{output_dir} / filename;
        if (std::filesystem::exists(output_path))
        {
            throw std::runtime_error{"Error: file already exists: " +
                                     output_path.string()};
        }

        auto const doc_str =
            "# Command Reference\n\n" + make_command_reference(xen::command_tree, 1);

        auto output_stream = std::ofstream{output_path};
        output_stream << doc_str;

        return EXIT_SUCCESS;
    }
    catch (std::exception const &e)
    {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::cerr << "Unknown exception\n";
        return EXIT_FAILURE;
    }
}