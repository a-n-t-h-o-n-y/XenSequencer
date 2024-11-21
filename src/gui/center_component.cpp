#include <xen/gui/center_component.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
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

[[nodiscard]] auto make_cell(sequence::Cell const &cell,
                             std::optional<xen::Scale> const &scale,
                             sequence::Tuning const &tuning,
                             xen::TranslateDirection scale_translate_direction)
    -> std::unique_ptr<xen::gui::Cell>
{
    auto const builder = xen::gui::BuildAndAllocateCell{
        scale,
        tuning,
        scale_translate_direction,
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

FieldEdit::FieldEdit(juce::String const &key, juce::String const &value,
                     bool actually_editable)
{
    this->addAndMakeVisible(key_);
    this->addAndMakeVisible(value_);

    value_.setEditable(false, actually_editable);
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

    auto const width = [&]() {
        auto glyph_arrangement = juce::GlyphArrangement{};
        glyph_arrangement.addLineOfText(key_.getFont(), key_.getText(), 0.f, 0.f);
        return glyph_arrangement.getBoundingBox(0, -1, true).getWidth() + 6.f;
    }();

    flex_box.items.add(juce::FlexItem{key_}.withWidth(width));
    flex_box.items.add(juce::FlexItem{value_}.withFlex(1));

    flex_box.performLayout(this->getLocalBounds());
}

// -------------------------------------------------------------------------------------

MeasureInfo::MeasureInfo()
    : time_signature_{"Time Signature"}, key_{"Key"},
      base_frequency_{"Zero Freq. (Hz)"}, scale_{"Scale"}, scale_mode_{"Mode"},
      tuning_name_{"Tuning", "", false}, measure_name_{""}

{
    this->setComponentID("MeasureInfo");

    this->addAndMakeVisible(time_signature_);
    this->addAndMakeVisible(key_);
    this->addAndMakeVisible(base_frequency_);
    this->addAndMakeVisible(scale_);
    this->addAndMakeVisible(scale_mode_);
    this->addAndMakeVisible(tuning_name_);
    this->addAndMakeVisible(measure_name_);

    {
        auto const font = fonts::monospaced().regular.withHeight(17.f);

        time_signature_.set_font(font);
        key_.set_font(font);
        base_frequency_.set_font(font);
        scale_.set_font(font);
        scale_mode_.set_font(font);
        tuning_name_.set_font(font);
        measure_name_.set_font(font);
    }

    time_signature_.on_text_change.connect([this](juce::String const &text) {
        this->on_command("set measure timesignature " + text.toStdString());
    });

    key_.on_text_change.connect([this](juce::String const &text) {
        this->on_command("set key " + text.toStdString());
    });

    base_frequency_.on_text_change.connect([this](juce::String const &text) {
        this->on_command("set basefrequency " + text.toStdString());
    });

    scale_.on_text_change.connect([this](juce::String const &text) {
        this->on_command("set scale " + double_quote(strip(text.toStdString())));
    });

    scale_mode_.on_text_change.connect([this](juce::String const &text) {
        this->on_command("set mode " + text.toStdString());
    });

    measure_name_.on_text_change.connect([this](juce::String const &text) {
        this->on_command("set measure name " + double_quote(strip(text.toStdString())));
    });
}

void MeasureInfo::update(SequencerState const &state, AuxState const &aux)
{
    {
        auto const &measure = state.sequence_bank[aux.selected.measure];
        auto const text = juce::String{measure.time_signature.numerator} + "/" +
                          juce::String{measure.time_signature.denominator};
        time_signature_.set_value(text);
    }

    {
        key_.set_value(std::to_string(state.key));
    }

    {
        auto const text = juce::String{state.base_frequency};
        base_frequency_.set_value(text);
    }

    if (state.scale.has_value())
    {
        scale_.set_value(state.scale->name);
        scale_mode_.setVisible(true);
        scale_mode_.set_value(juce::String(state.scale->mode));
    }
    else
    {
        scale_mode_.setVisible(false);
        scale_.set_value("Chromatic");
    }

    {
        tuning_name_.set_value(state.tuning_name);
    }

    {
        auto const index = aux.selected.measure;
        measure_name_.set_key(juce::String{index});
        measure_name_.set_value(state.sequence_names[index]);
    }

    this->resized();
}

void MeasureInfo::resized()
{
    auto flex_box = juce::FlexBox{};
    flex_box.flexDirection = juce::FlexBox::Direction::row;

    flex_box.items.add(juce::FlexItem{time_signature_}.withFlex(0.8f));
    flex_box.items.add(juce::FlexItem{key_}.withFlex(0.333f));
    flex_box.items.add(juce::FlexItem{base_frequency_}.withFlex(0.8f));
    flex_box.items.add(juce::FlexItem{scale_}.withFlex(1.f));
    if (scale_mode_.isVisible())
    {
        flex_box.items.add(juce::FlexItem{scale_mode_}.withFlex(0.4f));
    }
    flex_box.items.add(juce::FlexItem{tuning_name_}.withFlex(1.f));
    flex_box.items.add(juce::FlexItem{measure_name_}.withFlex(1.f));

    flex_box.performLayout(this->getLocalBounds());
}

void MeasureInfo::paintOverChildren(juce::Graphics &g)
{
    g.setColour(this->findColour(ColorID::ForegroundLow));
    g.drawRect(this->getLocalBounds(), 1);
}

// -------------------------------------------------------------------------------------

PitchColumn::PitchColumn(std::size_t size) : size_{size}
{
}

void PitchColumn::update(std::size_t new_size)
{
    size_ = new_size;
}

void PitchColumn::paint(juce::Graphics &g)
{
    g.fillAll(this->findColour(ColorID::BackgroundHigh));

    auto const bounds = this->getLocalBounds().toFloat().reduced(0.f, 4.f);
    auto const item_height = bounds.getHeight() / (float)size_;

    g.setColour(this->findColour(ColorID::ForegroundLow));
    g.setFont(fonts::monospaced().regular.withHeight(17.f));
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
                          {.intervals = {0}, .octave = 1, .description = ""},
                          TranslateDirection::Up)},
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

void MeasureView::update(SequencerState const &state, AuxState const &aux)
{
    if (selected_state_ != aux.selected || sequencer_state_ != state)
    {
        selected_state_ = aux.selected;
        sequencer_state_ = state;

        cell_ptr_.reset();
        auto &measure = state.sequence_bank[selected_state_.measure];
        cell_ptr_ = make_cell(measure.cell, state.scale, state.tuning,
                              state.scale_translate_direction);
        this->addAndMakeVisible(*cell_ptr_);

        if (auto const child_ptr = this->get_selected_child(); child_ptr != nullptr)
        {
            child_ptr->make_selected();
        }
        this->resized();
    }
}

void MeasureView::set_playhead(std::optional<float> percent)
{
    if (playhead_ != percent)
    {
        playhead_ = percent;
        this->repaint();
    }
}

auto MeasureView::get_selected_child() -> Cell *
{
    return this->get_cell().find_child(selected_state_.cell);
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
        auto const bounds = this->getLocalBounds().reduced(2, 4).toFloat();
        auto const x = bounds.getX() + playhead_.value() * bounds.getWidth();

        g.setColour(this->findColour(ColorID::ForegroundMedium));
        auto const thickness = 1.f;
        g.fillRect(x - thickness / 2.f, bounds.getY(), thickness, bounds.getHeight());
    }
}

void MeasureView::timerCallback()
{
    auto const audio_thread_state = audio_thread_state_.read();
    if (audio_thread_state.note_start_times[selected_state_.measure] != (SampleIndex)-1)
    {
        auto &measure = sequencer_state_.sequence_bank[selected_state_.measure];
        auto const samples_in_measure = sequence::samples_count(
            measure, audio_thread_state.daw.sample_rate, audio_thread_state.daw.bpm);
        if (samples_in_measure == 0)
        {
            this->set_playhead(std::nullopt);
            return;
        }
        auto const current_sample = audio_thread_state.accumulated_sample_count;
        auto const start_sample =
            audio_thread_state.note_start_times[selected_state_.measure];
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
    : pitch_column{12}, measure_view{audio_thread_state}
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

void SequenceView::update(SequencerState const &state, AuxState const &aux)
{
    measure_info.update(state, aux);

    measure_view.update(state, aux);

    pitch_column.update(state.tuning.intervals.size());
    // std::ranges::equal_to to avoid float comparison warning
    if (std::ranges::equal_to{}(state.tuning.octave, 1'200.f))
    {
        auto const selected_pitches = get_all_pitches(
            xen::get_selected_cell_const(state.sequence_bank, aux.selected));

        tuning_reference_ptr = std::make_unique<TuningReference>(
            state.tuning, state.scale, selected_pitches,
            state.scale_translate_direction);

        this->addAndMakeVisible(*tuning_reference_ptr);
    }
    else
    {
        tuning_reference_ptr = nullptr;
    }

    sequence_bank.update(aux.selected.measure);

    this->resized();
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
    this->addChildComponent(message_log);
}

void CenterComponent::show_sequence_view()
{
    message_log.setVisible(false);
    library_view.setVisible(false);
    sequence_view.setVisible(true);
    this->resized();
}

void CenterComponent::show_library_view()
{
    sequence_view.setVisible(false);
    message_log.setVisible(false);
    library_view.setVisible(true);
    this->resized();
}

void CenterComponent::show_message_log()
{
    sequence_view.setVisible(false);
    library_view.setVisible(false);
    message_log.setVisible(true);
    this->resized();
}

void CenterComponent::update(SequencerState const &state, AuxState const &aux,
                             std::vector<Scale> const &scales)
{
    state_ = state;
    sequence_view.update(state_, aux);
    library_view.scales_list.update(scales);
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
    else if (library_view.isVisible())
    {
        return library_view;
    }
    else
    {
        assert(message_log.isVisible());
        return message_log;
    }
}

} // namespace xen::gui