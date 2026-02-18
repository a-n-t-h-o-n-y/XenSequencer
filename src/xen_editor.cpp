#include <xen/xen_editor.hpp>

#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/xen_processor.hpp>

namespace
{

class NoTabFocusTraverser : public juce::KeyboardFocusTraverser
{
  public:
    [[nodiscard]] auto getNextComponent(juce::Component *) -> juce::Component * override
    {
        return nullptr;
    }

    [[nodiscard]] auto getPreviousComponent(juce::Component *)
        -> juce::Component * override
    {
        return nullptr;
    }
};

} // namespace

namespace xen::gui
{

XenEditor::XenEditor(XenProcessor &p, int width, int height)
    : AudioProcessorEditor{p}, processor_{p}
{
    this->setFocusContainerType(juce::Component::FocusContainerType::focusContainer);
    this->setResizable(true, true);
    this->setSize(width, height);
    this->setResizeLimits(400, 300, 0x3fffffff, 0x3fffffff);

    webview_host_ = std::make_unique<WebviewHost>(processor_);
    this->addAndMakeVisible(webview_host_.get());
    webview_host_->setBounds(this->getLocalBounds());
}

auto XenEditor::createKeyboardFocusTraverser()
    -> std::unique_ptr<juce::ComponentTraverser>
{
    return std::make_unique<NoTabFocusTraverser>();
}

void XenEditor::resized()
{
    if (webview_host_ != nullptr)
    {
        webview_host_->setBounds(this->getLocalBounds());
    }
    processor_.editor_width = this->getWidth();
    processor_.editor_height = this->getHeight();
}

} // namespace xen::gui
