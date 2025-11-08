#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_data_structures/juce_data_structures.h>

/**
 * Resizable Window Manager
 */
class ResizableUIManager
{
public:
    ResizableUIManager(juce::Component& editor, 
                      int minW = 800, int minH = 600,
                      int maxW = 2400, int maxH = 1800,
                      int defaultW = 1200, int defaultH = 900)
        : editorComponent(editor),
          minWidth(minW), minHeight(minH),
          maxWidth(maxW), maxHeight(maxH),
          defaultWidth(defaultW), defaultHeight(defaultH)
    {
        loadSavedSize();
        
        constrainer.setMinimumSize(minWidth, minHeight);
        constrainer.setMaximumSize(maxWidth, maxHeight);
        constrainer.setFixedAspectRatio(static_cast<double>(defaultWidth) / defaultHeight);
    }
    
    void attachToEditor()
    {
        if (auto* peer = editorComponent.getPeer())
        {
            peer->setConstrainer(&constrainer);
        }
        
        editorComponent.setSize(currentWidth, currentHeight);
    }
    
    void setScale(float newScale)
    {
        scale = juce::jlimit(0.5f, 2.0f, newScale);
        
        int newWidth = static_cast<int>(defaultWidth * scale);
        int newHeight = static_cast<int>(defaultHeight * scale);
        
        editorComponent.setSize(newWidth, newHeight);
        
        currentWidth = newWidth;
        currentHeight = newHeight;
        
        saveSize();
    }
    
    float getScale() const { return scale; }
    
    void saveSize()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "WiiPluck";
        options.filenameSuffix = ".settings";
        options.folderName = "WiiPluck";
        options.osxLibrarySubFolder = "Application Support";
        
        juce::PropertiesFile properties(options);
        properties.setValue("windowWidth", currentWidth);
        properties.setValue("windowHeight", currentHeight);
        properties.setValue("windowScale", scale);
        properties.save();
    }
    
    void loadSavedSize()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "WiiPluck";
        options.filenameSuffix = ".settings";
        options.folderName = "WiiPluck";
        options.osxLibrarySubFolder = "Application Support";
        
        juce::PropertiesFile properties(options);
        
        currentWidth = properties.getIntValue("windowWidth", defaultWidth);
        currentHeight = properties.getIntValue("windowHeight", defaultHeight);
        scale = static_cast<float>(properties.getDoubleValue("windowScale", 1.0));
        
        currentWidth = juce::jlimit(minWidth, maxWidth, currentWidth);
        currentHeight = juce::jlimit(minHeight, maxHeight, currentHeight);
        scale = juce::jlimit(0.5f, 2.0f, scale);
    }
    
private:
    juce::Component& editorComponent;
    juce::ComponentBoundsConstrainer constrainer;
    
    int minWidth, minHeight;
    int maxWidth, maxHeight;
    int defaultWidth, defaultHeight;
    int currentWidth, currentHeight;
    float scale = 1.0f;
};

/**
 * Scale Selector Component
 */
class ScaleSelector : public juce::Component
{
public:
    ScaleSelector(ResizableUIManager& manager) : uiManager(manager)
    {
        scaleSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        scaleSlider.setRange(0.5, 2.0, 0.1);
        scaleSlider.setValue(1.0);
        scaleSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
        scaleSlider.setTextValueSuffix("%");
        scaleSlider.onValueChange = [this] {
            uiManager.setScale(scaleSlider.getValue());
        };
        addAndMakeVisible(scaleSlider);
        
        label.setText("UI Scale:", juce::dontSendNotification);
        addAndMakeVisible(label);
        
        // Preset buttons
        scale50Button.setButtonText("50%");
        scale50Button.onClick = [this] { setScale(0.5f); };
        addAndMakeVisible(scale50Button);
        
        scale100Button.setButtonText("100%");
        scale100Button.onClick = [this] { setScale(1.0f); };
        addAndMakeVisible(scale100Button);
        
        scale150Button.setButtonText("150%");
        scale150Button.onClick = [this] { setScale(1.5f); };
        addAndMakeVisible(scale150Button);
        
        scale200Button.setButtonText("200%");
        scale200Button.onClick = [this] { setScale(2.0f); };
        addAndMakeVisible(scale200Button);
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff1a1a2e));
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(5);
        
        label.setBounds(bounds.removeFromLeft(80));
        
        int buttonWidth = 60;
        scale50Button.setBounds(bounds.removeFromLeft(buttonWidth).reduced(2));
        scale100Button.setBounds(bounds.removeFromLeft(buttonWidth).reduced(2));
        scale150Button.setBounds(bounds.removeFromLeft(buttonWidth).reduced(2));
        scale200Button.setBounds(bounds.removeFromLeft(buttonWidth).reduced(2));
        
        bounds.removeFromLeft(10);
        scaleSlider.setBounds(bounds);
    }
    
private:
    void setScale(float scale)
    {
        scaleSlider.setValue(scale, juce::sendNotificationAsync);
    }
    
    ResizableUIManager& uiManager;
    juce::Slider scaleSlider;
    juce::Label label;
    juce::TextButton scale50Button, scale100Button, scale150Button, scale200Button;
};
