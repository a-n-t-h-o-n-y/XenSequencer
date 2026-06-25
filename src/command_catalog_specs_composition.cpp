#include "command_catalog_specs_internal.hpp"

#include <string>
#include <utility>

#include <xen/command_dsl.hpp>
#include <xen/composition.hpp>
#include <xen/message_level.hpp>

namespace xen::catalog_detail
{
namespace
{

constexpr auto composition_edit_policy =
    CommandPolicy{ProjectOperation::Edit,  LibraryAccess::None,
                  WorkspaceAccess::None,   FileAccess::None,
                  TargetRequirement::None, RepeatPolicy::OnSuccessfulProjectChange,
                  HistoryPolicy::Commit};

} // namespace

void append_composition_specs(std::vector<CommandSpec> &specs)
{
    specs.push_back(
        command({"composition", "loop", "start"}, false,
                "Set the composition loop start column.", composition_edit_policy,
                std::make_tuple(required_arg<std::size_t>("column_index")),
                [](CommandHandlerContext &context, CommandInvocation const &,
                   std::size_t column_index) {
                    auto state = context.project();
                    set_loop_start(state.composition, column_index);
                    context.edit_project() = std::move(state);
                    return make_result(minfo("Composition Loop Start Set: " +
                                             std::to_string(column_index)));
                }));

    specs.push_back(command({"composition", "loop", "end"}, false,
                            "Set the composition loop end column.",
                            composition_edit_policy,
                            std::make_tuple(required_arg<std::size_t>("column_index")),
                            [](CommandHandlerContext &context,
                               CommandInvocation const &, std::size_t column_index) {
                                auto state = context.project();
                                set_loop_end(state.composition, column_index);
                                context.edit_project() = std::move(state);
                                return make_result(minfo("Composition Loop End Set: " +
                                                         std::to_string(column_index)));
                            }));
}

} // namespace xen::catalog_detail
