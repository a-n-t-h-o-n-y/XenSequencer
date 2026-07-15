#include <catch2/catch_test_macros.hpp>

#include <sequence/sequence.hpp>

#include <xen/selection.hpp>
#include <xen/sequencer_session.hpp>

using namespace xen;

namespace
{

auto execute(SequencerSession &session, std::string const &command,
             std::optional<SelectionPath> selection = std::nullopt)
    -> CommandApplicationResult
{
    return session.execute_command_string(
        command,
        {.selection = std::move(selection),
         .expected_project_revision = session.project_snapshot().project_revision});
}

} // namespace

TEST_CASE("Targeted edits require a supplied selection", "[core][state-actions]")
{
    auto session = SequencerSession{};

    auto const result = session.execute_command_string(
        "delete",
        {.expected_project_revision = session.project_snapshot().project_revision});

    CHECK(result.status.first == MessageLevel::Error);
    CHECK(result.status.second == "selection is required");
}

TEST_CASE("Delete suggests nearest surviving sibling", "[core][state-actions]")
{
    auto session = SequencerSession{};
    REQUIRE(execute(session, "note 1", SelectionPath{}).status.first ==
            MessageLevel::Info);
    REQUIRE(execute(session, "note 2", SelectionPath{}).status.first ==
            MessageLevel::Info);

    auto const selection = select_element_in_cell({}, 0);
    auto const result = execute(session, "delete", selection);

    CHECK(result.status.first == MessageLevel::Info);
    REQUIRE(result.suggested_selection.has_value());
    CHECK(*result.suggested_selection == select_element_in_cell({}, 0));
}

TEST_CASE("Paste reports an empty session copy buffer", "[core][copy-paste]")
{
    auto session = SequencerSession{};
    auto const before = session.project_snapshot();

    auto const result = execute(session, "paste", SelectionPath{});

    CHECK(result.status.first == MessageLevel::Error);
    CHECK(result.status.second == "Copy Buffer Is Empty");
    CHECK(session.project_snapshot().project == before.project);
    CHECK(session.project_snapshot().project_revision == before.project_revision);
}

TEST_CASE("Command chains read their staged copy buffer", "[core][copy-paste]")
{
    auto session = SequencerSession{};
    REQUIRE(execute(session, "note 7", SelectionPath{}).status.first ==
            MessageLevel::Info);
    auto const copied = selected_sequence(session.project_snapshot().project, {});

    auto const result =
        execute(session, "copy; sequence clear; paste", SelectionPath{});

    REQUIRE(result.status.first == MessageLevel::Info);
    CHECK(selected_sequence(session.project_snapshot().project, {}) == copied);
}

TEST_CASE("Element copy, cut, and paste retain selection semantics",
          "[core][copy-paste]")
{
    auto session = SequencerSession{};
    REQUIRE(execute(session, "note 1", SelectionPath{}).status.first ==
            MessageLevel::Info);
    REQUIRE(execute(session, "note 2", SelectionPath{}).status.first ==
            MessageLevel::Info);

    auto const first = select_element_in_cell({}, 0);
    auto const second = select_element_in_cell({}, 1);
    REQUIRE(execute(session, "copy", first).status.first == MessageLevel::Info);
    auto const pasted = execute(session, "paste", second);
    REQUIRE(pasted.status.first == MessageLevel::Info);
    REQUIRE(pasted.suggested_selection.has_value());
    CHECK(*pasted.suggested_selection == second);

    auto const after_paste = selected_sequence(session.project_snapshot().project, {});
    REQUIRE(after_paste.elements.size() == 3);
    CHECK(std::get<sequence::Note>(after_paste.elements.at(0)).pitch == 1);
    CHECK(std::get<sequence::Note>(after_paste.elements.at(1)).pitch == 2);
    CHECK(std::get<sequence::Note>(after_paste.elements.at(2)).pitch == 1);

    auto const cut = execute(session, "cut", select_element_in_cell({}, 2));
    REQUIRE(cut.status.first == MessageLevel::Info);
    REQUIRE(cut.suggested_selection.has_value());
    CHECK(*cut.suggested_selection == select_element_in_cell({}, 1));
    REQUIRE(execute(session, "paste", SelectionPath{}).status.first ==
            MessageLevel::Info);
    CHECK(selected_sequence(session.project_snapshot().project, {}).elements.size() ==
          3);
}

TEST_CASE("Independent sessions have isolated copy buffers", "[core][copy-paste]")
{
    auto source = SequencerSession{};
    auto destination = SequencerSession{};
    REQUIRE(execute(source, "note 4", SelectionPath{}).status.first ==
            MessageLevel::Info);
    REQUIRE(execute(source, "copy", SelectionPath{}).status.first ==
            MessageLevel::Info);

    auto const result = execute(destination, "paste", SelectionPath{});

    CHECK(result.status.first == MessageLevel::Error);
    CHECK(result.status.second == "Copy Buffer Is Empty");
}

TEST_CASE("MIDI CC commands materialize, edit, remove, and undo note automation",
          "[core][state-actions][midi-cc]")
{
    auto session = SequencerSession{};
    REQUIRE(execute(session, "note 4", SelectionPath{}).status.first ==
            MessageLevel::Info);
    auto const note_selection = select_element_in_cell({}, 0);

    REQUIRE(execute(session, "shift midiCC 74 -0.1", note_selection).status.first ==
            MessageLevel::Info);
    auto note = std::get<sequence::Note>(
        selected_sequence(session.project_snapshot().project, {}).elements.front());
    REQUIRE(note.midi_cc.contains(74));
    CHECK(note.midi_cc.at(74) == 0.4f);

    REQUIRE(execute(session, "undo").status.first == MessageLevel::Info);
    note = std::get<sequence::Note>(
        selected_sequence(session.project_snapshot().project, {}).elements.front());
    CHECK_FALSE(note.midi_cc.contains(74));

    REQUIRE(execute(session, "set midiCC 1 0", note_selection).status.first ==
            MessageLevel::Info);
    note = std::get<sequence::Note>(
        selected_sequence(session.project_snapshot().project, {}).elements.front());
    REQUIRE(note.midi_cc.contains(1));
    CHECK(note.midi_cc.at(1) == 0.f);

    REQUIRE(execute(session, "remove midiCC 1", note_selection).status.first ==
            MessageLevel::Info);
    note = std::get<sequence::Note>(
        selected_sequence(session.project_snapshot().project, {}).elements.front());
    CHECK_FALSE(note.midi_cc.contains(1));

    auto const before_noop = session.project_snapshot().project_revision;
    REQUIRE(execute(session, "remove midiCC 1", note_selection).status.first ==
            MessageLevel::Info);
    CHECK(session.project_snapshot().project_revision == before_noop);
}

TEST_CASE("MIDI CC label commands enforce case-insensitive uniqueness",
          "[core][state-actions][midi-cc]")
{
    auto session = SequencerSession{};
    REQUIRE(execute(session, "set midiCCLabel 74 \"Filter Cutoff\"").status.first ==
            MessageLevel::Info);
    CHECK(session.project_snapshot().project.midi_cc_labels.at(74) == "Filter Cutoff");

    auto const duplicate = execute(session, "set midiCCLabel 71 \"filter cutoff\"");
    CHECK(duplicate.status.first == MessageLevel::Error);
    CHECK_FALSE(session.project_snapshot().project.midi_cc_labels.contains(71));

    REQUIRE(execute(session, "remove midiCCLabel 74").status.first ==
            MessageLevel::Info);
    CHECK(session.project_snapshot().project.midi_cc_labels.empty());

    REQUIRE(execute(session, "undo").status.first == MessageLevel::Info);
    CHECK(session.project_snapshot().project.midi_cc_labels.at(74) == "Filter Cutoff");
    REQUIRE(execute(session, "redo").status.first == MessageLevel::Info);
    CHECK(session.project_snapshot().project.midi_cc_labels.empty());
}
