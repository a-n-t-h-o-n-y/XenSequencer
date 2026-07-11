#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <xen/command_catalog.hpp>
#include <xen/constants.hpp>

[[nodiscard]] auto make_command_reference_table(
    std::vector<xen::Documentation> const &docs) -> std::string
{
    auto result = std::string{"name | signature | description\n"
                              "---- | --------- | -----------\n"};

    for (auto const &doc : docs)
    {
        auto sig = std::string{"`"};
        if (doc.signature.pattern_arg)
        {
            sig += "[pattern] ";
        }
        sig += doc.signature.id;
        for (auto const &arg : doc.signature.arguments)
        {
            sig += (" " + arg);
        }
        sig += '`';
        auto description = doc.description;
        description = std::regex_replace(description, std::regex{"\n"}, "<br>");
        result.append(doc.signature.id + " | " + sig + " | " + description + "\n");
    }

    return result;
}

/**
 * Generate a markdown document containing a reference for all commands
 */
int main(int argc, char const *argv[])
{
    try
    {
        auto const check_only = argc == 3 && std::string_view{argv[1]} == "--check";
        if ((!check_only && argc != 2) || (check_only && argc != 3))
        {
            throw std::runtime_error{"Usage: cmd_reference [--check] <output_dir>"};
        }

        constexpr auto filename = "command_reference.md";
        auto const output_dir = std::filesystem::path{argv[check_only ? 2 : 1]};
        auto const output_path = std::filesystem::path{output_dir} / filename;
        auto const docs = xen::catalog_docs();
        auto const doc_str = "# Command Reference (v" + std::string{xen::VERSION} +
                             ")\n\n" + make_command_reference_table(docs);

        if (check_only)
        {
            auto input = std::ifstream{output_path, std::ios::binary};
            auto existing = std::ostringstream{};
            existing << input.rdbuf();
            if (!input || existing.str() != doc_str)
            {
                throw std::runtime_error{
                    "Command reference is out of date. Regenerate it with: "
                    "cmake --build build --target update_command_reference"};
            }
            return EXIT_SUCCESS;
        }

        std::filesystem::create_directories(output_dir);
        auto output = std::ofstream{output_path, std::ios::binary | std::ios::trunc};
        output << doc_str;
        if (!output)
        {
            throw std::runtime_error{"Unable to write command reference: " +
                                     output_path.string()};
        }

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
