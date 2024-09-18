#include <xen/gui/center_component.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include <sequence/measure.hpp>
#include <sequence/tuning.hpp>
#include <sequence/utility.hpp>

#include <signals_light/signal.hpp>

#include <xen/double_buffer.hpp>
#include <xen/gui/accordion.hpp>
#include <xen/gui/fonts.hpp>
#include <xen/gui/library_view.hpp>
#include <xen/gui/sequence.hpp>
#include <xen/gui/sequence_bank.hpp>
#include <xen/gui/themes.hpp>
#include <xen/scale.hpp>
#include <xen/selection.hpp>
#include <xen/state.hpp>
#include <xen/string_manip.hpp>

namespace
{

[[nodiscard]] auto make_cell(
    sequence::Cell const &cell, std::optional<xen::Scale> const &scale,
    sequence::Tuning const &tuning) -> std::unique_ptr<xen::gui::Cell>
{
    auto const builder = xen::gui::BuildAndAllocateCell{
        scale,
        tuning,
    };
    return std::visit(builder, cell);
}

[[nodiscard]] auto get_all_pitches(sequence::Cell const &cell) -> std::set<int>
{
    return std::visit(
        sequence::utility::overload{
            [](sequence::Rest const &) -> std::set<int> { return {}; },
            [](sequence::Note const &n) -> std::set<int> { return {n.pitch}; },
            [&](sequence::Sequence const &s) -> std::set<int> {
                auto result = std::set<int>{};
                for (auto const &c : s.cells)
                {
                    auto temp = get_all_pitches(c);
                    result.insert(std::make_move_iterator(std::begin(temp)),
                                  std::make_move_iterator(std::end(temp)));
                }
                return result;
            },
        },
        cell);
}

} // namespace

// -------------------------------------------------------------------------------------

namespace xen::gui
{

void CenterComponentLabel::lookAndFeelChanged()
{
    this->setColour(juce::Label::ColourIds::textColourId,
                    this->findColour(ColorID::ForegroundHigh));
    this->setColour(juce::Label::ColourIds::backgroundColourId,
                    this->findColour(ColorID::Background));
}

// -------------------------------------------------------------------------------------

FieldEdit::FieldEdit(juce::String const &key, juce::String const &value)
{
    this->addAndMakeVisible(key_);
    this->addAndMakeVisible(value_);

    value_.setEditable(false, true);
    value_.onEditorShow = [this] { temp_text_ = value_.getText(); };
    value_.onTextChange = [this] {
        auto const new_text = value_.getText();
        value_.setText(temp_text_, juce::dontSendNotification);
        this->on_text_change(new_text);
    };

    this->set_key(key);
    this->set_value(value);
}

void FieldEdit::set_key(juce::String const &key)
{
    key_.setText(key + ": ", juce::dontSendNotification);
}

void FieldEdit::set_value(juce::String const &value)
{
    value_.setText(value, juce::dontSendNotification);
}

void FieldEdit::set_font(juce::Font const &font)
{
    key_.setFont(font);
    value_.setFont(font);
}

void FieldEdit::resized()
{
    auto flex_box = juce::FlexBox{};
    flex_box.flexDirection = juce::FlexBox::Direction::row;

    auto const width = ((float)key_.getFont().getStringWidth(key_.getText()) + 6.f);
    flex_box.items.add(juce::FlexItem{key_}.withWidth(width));
    flex_box.items.add(juce::FlexItem{value_}.withFlex(1));

    flex_box.performLayout(this->getLocalBounds());
}

// -------------------------------------------------------------------------------------

MeasureInfo::MeasureInfo()
{
    this->setComponentID("MeasureInfo");

    this->addAndMakeVisible(time_signature_);
    this->addAndMakeVisible(base_frequency_);
    this->addAndMakeVisible(measure_name_);
    this->addAndMakeVisible(tuning_name_);

    time_signature_.on_text_change.connect([this](juce::String const &text) {
        this->on_command("set measure timesignature " + text.toStdString());
    });

    base_frequency_.on_text_change.connect([this](juce::String const &text) {
        this->on_command("set tuning basefrequency " + text.toStdString());
    });

    measure_name_.on_text_change.connect([this](juce::String const &text) {
        this->on_command("set measure name " + double_quote(text.toStdString()));
    });

    tuning_name_.on_text_change.connect([this](juce::String const &text) {
        this->on_command("set tuning name " + double_quote(text.toStdString()));
    });

    auto const font = fonts::monospaced().regular.withHeight(16.f);
    time_signature_.set_font(font);
    time_signature_.set_key("Time Signature");

    base_frequency_.set_font(font);
    base_frequency_.set_key("Base Frequency (Hz)");

    measure_name_.set_font(font);

    tuning_name_.set_font(font);
    tuning_name_.set_key("Tuning");
}

void MeasureInfo::update_ui(SequencerState const &state, AuxState const &aux)
{
    {
        auto &measure = state.sequence_bank[aux.selected.measure];
        auto const text = juce::String{measure.time_signature.numerator} + "/" +
                          juce::String{measure.time_signature.denominator};
        time_signature_.set_value(text);
    }

    {
        auto const text = juce::String{state.base_frequency};
        base_frequency_.set_value(text);
    }

    {
        auto const index = aux.selected.measure;
        measure_name_.set_key(juce::String{index});
        measure_name_.set_value(juce::String{state.measure_names[index]});
    }

    {
        tuning_name_.set_value(juce::String{state.tuning_name});
    }
}

void MeasureInfo::resized()
{
    auto flex_box = juce::FlexBox{};
    flex_box.flexDirection = juce::FlexBox::Direction::row;

    flex_box.items.add(juce::FlexItem{time_signature_}.withFlex(1));
    flex_box.items.add(juce::FlexItem{base_frequency_}.withFlex(1));
    flex_box.items.add(juce::FlexItem{measure_name_}.withFlex(1));
    flex_box.items.add(juce::FlexItem{tuning_name_}.withFlex(1));

    flex_box.performLayout(this->getLocalBounds());
}

void MeasureInfo::paintOverChildren(juce::Graphics &g)
{
    g.setColour(this->findColour(ColorID::ForegroundLow));
    g.drawRect(this->getLocalBounds(), 1);
}

// -------------------------------------------------------------------------------------

PitchColumn::PitchColumn(std::size_t size, float vertical_offset)
    : size_{size}, vertical_offset_{vertical_offset}
{
}

void PitchColumn::update(std::size_t new_size)
{
    size_ = new_size;
}

void PitchColumn::paint(juce::Graphics &g)
{
    g.fillAll(this->findColour(ColorID::BackgroundMedium));

    auto const bounds = this->getLocalBounds().toFloat().reduced(0.f, vertical_offset_);
    auto const item_height = bounds.getHeight() / static_cast<float>(size_);

    g.setColour(this->findColour(ColorID::ForegroundLow));
    g.setFont(fonts::monospaced().regular.withHeight(16.f));
    for (std::size_t i = 0; i < size_; ++i)
    {
        float y = bounds.getBottom() - ((float)i + 1.f) * item_height;
        auto const text = juce::String(i).paddedLeft('0', 2);

        g.drawText(text, bounds.withY(y).withHeight(item_height),
                   juce::Justification::centred, true);
    }
}

// -------------------------------------------------------------------------------------

MeasureView::MeasureView(DoubleBuffer<AudioThreadStateForGUI> const &audio_thread_state)
    : cell_ptr_{make_cell(sequence::Rest{}, std::nullopt,
                          {.intervals = {0}, .octave = 1})},
      audio_thread_state_{audio_thread_state}
{
    this->startTimer(34);
}

MeasureView::~MeasureView()
{
    this->stopTimer();
}

auto MeasureView::get_cell() -> Cell &
{
    return *cell_ptr_;
}

auto MeasureView::get_cell() const -> Cell const &
{
    return *cell_ptr_;
}

void MeasureView::update_ui(SequencerState const &state, AuxState const &aux)
{
    selected_measure_ = aux.selected.measure;
    measure_ = state.sequence_bank[selected_measure_];

    // TODO can you do a simple equality check to conditionally rebuild the cell?
    cell_ptr_.reset();
    cell_ptr_ = make_cell(measure_.cell, state.scale, state.tuning);
    this->addAndMakeVisible(*cell_ptr_);
    this->resized();
}

void MeasureView::set_playhead(std::optional<float> percent)
{
    if (playhead_ != percent)
    {
        playhead_ = percent;
        this->repaint();
    }
}

void MeasureView::resized()
{
    cell_ptr_->setBounds(this->getLocalBounds());
}

void MeasureView::paint(juce::Graphics &g)
{
    g.setColour(this->findColour(ColorID::BackgroundHigh));
    g.fillAll();
}

void MeasureView::paintOverChildren(juce::Graphics &g)
{
    if (playhead_.has_value())
    {
        g.setColour(this->findColour(ColorID::ForegroundMedium));

        auto const x_pos = playhead_.value() * static_cast<float>(this->getWidth());
        g.drawLine(x_pos, 4.f, x_pos, static_cast<float>(this->getHeight() - 4));
    }
}

void MeasureView::timerCallback()
{
    auto const audio_thread_state = audio_thread_state_.read();
    if (audio_thread_state.note_start_times[selected_measure_] != (std::uint64_t)-1)
    {
        auto const samples_in_measure = sequence::samples_count(
            measure_, audio_thread_state.daw.sample_rate, audio_thread_state.daw.bpm);
        if (samples_in_measure == 0)
        {
            this->set_playhead(std::nullopt);
            return;
        }
        auto const current_sample = audio_thread_state.accumulated_sample_count;
        auto const start_sample =
            audio_thread_state.note_start_times[selected_measure_];
        auto const percent =
            (float)((current_sample - start_sample) % samples_in_measure) /
            (float)samples_in_measure;
        this->set_playhead(percent);
    }
    else
    {
        this->set_playhead(std::nullopt);
    }
}

// -------------------------------------------------------------------------------------

SequenceView::SequenceView(
    DoubleBuffer<AudioThreadStateForGUI> const &audio_thread_state)
    : pitch_column{12, 4.f}, measure_view{audio_thread_state}
{
    this->setComponentID("SequenceView");
    this->setWantsKeyboardFocus(true);

    this->addAndMakeVisible(measure_info);
    this->addAndMakeVisible(sequence_bank_accordion);
    this->addAndMakeVisible(pitch_column);
    this->addAndMakeVisible(measure_view);

    measure_info.on_command.connect(
        [this](std::string const &command) { this->on_command(command); });
}

void SequenceView::update_ui(SequencerState const &state, AuxState const &aux)
{
    measure_info.update_ui(state, aux);

    measure_view.update_ui(state, aux);

    pitch_column.update(state.tuning.intervals.size());

    if (state.tuning.octave == 1'200.f)
    {
        auto const selected_pitches = get_all_pitches(
            xen::get_selected_cell_const(state.sequence_bank, aux.selected));

        tuning_reference_ptr = std::make_unique<TuningReference>(
            state.tuning, state.scale, selected_pitches);

        this->addAndMakeVisible(*tuning_reference_ptr);
    }
    else
    {
        tuning_reference_ptr = nullptr;
    }

    sequence_bank.update_ui(aux.selected.measure);

    this->resized();
}

void SequenceView::select(std::vector<std::size_t> const &indices)
{
    measure_view.get_cell().select_child(indices);
}

void SequenceView::resized()
{
    sequence_bank_accordion.set_flexitem(
        juce::FlexItem{}.withWidth((float)this->getHeight()));

    auto flex_box = juce::FlexBox{};
    flex_box.flexDirection = juce::FlexBox::Direction::column;

    auto horizontal_flex = juce::FlexBox{};
    horizontal_flex.flexDirection = juce::FlexBox::Direction::row;
    horizontal_flex.items.add(juce::FlexItem{pitch_column}.withWidth(23.f));
    horizontal_flex.items.add(juce::FlexItem{measure_view}.withFlex(1));
    if (tuning_reference_ptr != nullptr)
    {
        horizontal_flex.items.add(
            juce::FlexItem{*tuning_reference_ptr}.withWidth(23.f));
    }
    // TODO figure out how to make square
    horizontal_flex.items.add(sequence_bank_accordion.get_flexitem());

    flex_box.items.add(juce::FlexItem{measure_info}.withHeight(23.f));
    flex_box.items.add(juce::FlexItem{horizontal_flex}.withFlex(1));

    flex_box.performLayout(this->getLocalBounds());
}

// -------------------------------------------------------------------------------------

CenterComponent::CenterComponent(
    juce::File const &sequence_library_dir, juce::File const &tuning_library_dir,
    DoubleBuffer<AudioThreadStateForGUI> const &audio_thread_state)
    : sequence_view{audio_thread_state},
      library_view{sequence_library_dir, tuning_library_dir}
{
    this->addAndMakeVisible(sequence_view);
    this->addChildComponent(library_view);
}

void CenterComponent::show_sequence_view()
{
    sequence_view.setVisible(true);
    library_view.setVisible(false);
    this->resized();
}

void CenterComponent::show_library_view()
{
    sequence_view.setVisible(false);
    library_view.setVisible(true);
    this->resized();
}

void CenterComponent::update_ui(SequencerState const &state, AuxState const &aux)
{
    state_ = state;
    sequence_view.update_ui(state_, aux);

    // TODO update library view? Is there anything? Are current directories updated
    // via a separate mechanism?
}

void CenterComponent::resized()
{
    this->current_component().setBounds(this->getLocalBounds());
}

auto CenterComponent::current_component() -> juce::Component &
{
    if (sequence_view.isVisible())
    {
        return sequence_view;
    }
    else
    {
        assert(library_view.isVisible());
        return library_view;
    }
}

} // namespace xen::gui