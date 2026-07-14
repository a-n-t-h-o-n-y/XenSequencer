#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <xen/state.hpp>

namespace xen
{

enum class DocumentErrorCode
{
    StaleProject,
    UnsavedChanges,
    InvalidPath,
    NotFound,
    FileExists,
    FileConflict,
    ProjectPathRequired,
    FileTooLarge,
    InvalidDocument,
    PreviewActive,
    RecoveryConflict,
    Io,
};

class DocumentError : public std::runtime_error
{
  public:
    DocumentError(DocumentErrorCode code_in, std::string message,
                  std::optional<std::string> current_file_revision_in = std::nullopt)
        : std::runtime_error{std::move(message)}, code{code_in},
          current_file_revision{std::move(current_file_revision_in)}
    {
    }

    DocumentErrorCode code;
    std::optional<std::string> current_file_revision;
};

struct ContentFileInfo
{
    std::string name{};
    std::string relative_path{};
    std::string stem{};
    std::string file_revision{};

    auto operator==(ContentFileInfo const &) const -> bool = default;
};

struct DocumentOperationResult
{
    ProjectSnapshot snapshot{};
    std::optional<ContentFileInfo> file{};
    std::optional<SelectionPath> suggested_selection{};
};

} // namespace xen
