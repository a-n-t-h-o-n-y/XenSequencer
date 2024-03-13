#include <memory>

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

class MainComponent : public juce::Component
{
  public:
    MainComponent()
    {
        this->setWantsKeyboardFocus(true);
        this->addAndMakeVisible(displayLabel);
        this->setSize(600, 400);
    }

  public:
    void paint(juce::Graphics &g) override
    {
        g.fillAll(juce::Colours::black);
    }

    void resized() override
    {
        displayLabel.setBounds(20, 20, getWidth() - 40, 40);
    }

    /**
     * Handles key events.
     *
     * @param key The key event.
     * @return True if key is handled, otherwise false.
     */
    [[nodiscard]] auto keyPressed(const juce::KeyPress &key) -> bool override
    {
        auto const description = key.getTextDescription();
        lastKeyPressedDescription = "Last key pressed: " + description;
        displayLabel.setText(lastKeyPressedDescription,
                             juce::NotificationType::dontSendNotification);
        return true;
    }

  private:
    juce::String lastKeyPressedDescription;
    juce::Label displayLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

class KeyboardInputApp : public juce::JUCEApplication
{
  public:
    const juce::String getApplicationName() override
    {
        return "KeyPress";
    }

    const juce::String getApplicationVersion() override
    {
        return "0.0.1";
    }

    void initialise(const juce::String &) override
    {
        main_window_ = std::make_unique<MainWindow>("Keyboard Input App",
                                                    new MainComponent(), *this);
    }

    void shutdown() override
    {
        main_window_.reset();
    }

    class MainWindow : public juce::DocumentWindow
    {
      public:
        MainWindow(const juce::String &name, juce::Component *c, JUCEApplication &a)
            : DocumentWindow(
                  name,
                  juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(
                      ResizableWindow::backgroundColourId),
                  DocumentWindow::allButtons),
              app(a)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(c, true);
            setResizable(true, true);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            app.systemRequestedQuit();
        }

      private:
        JUCEApplication &app;
    };

  private:
    std::unique_ptr<MainWindow> main_window_;
};

// This macro generates the main() function.
START_JUCE_APPLICATION(KeyboardInputApp)
