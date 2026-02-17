#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
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

        auto const docs = xen::catalog_docs();
        auto const doc_str = "# Command Reference (v" + std::string{xen::VERSION} +
                             ")\n\n" + make_command_reference_table(docs);

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
