#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PresetManager.h"

class AddNewPresetItem : public juce::Component
{
public:
    AddNewPresetItem()
    {
        setSize(400, 40);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Ghost/dashed border
        g.setColour(juce::Colour(0xffd8b5ff).withAlpha(0.6f));

        // Draw dashed border
        juce::Path dashedPath;
        dashedPath.addRoundedRectangle(bounds.reduced(2), 5.0f);
        g.strokePath(dashedPath, juce::PathStrokeType(2.0f));

        // Plus icon and text
        g.setColour(juce::Colour(0xff6b4f9e));
        g.setFont(juce::Font(16.0f, juce::Font::bold));

        // Draw plus symbol
        auto iconArea = bounds.removeFromLeft(40);
        g.drawText("+", iconArea, juce::Justification::centred);

        // Draw text
        g.setFont(juce::Font(14.0f));
        g.drawText("Add New Preset", bounds.reduced(10, 5), juce::Justification::centredLeft);
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        if (onAddPreset)
            onAddPreset();
    }

    void mouseEnter(const juce::MouseEvent&) override
    {
        setAlpha(0.8f);
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        setAlpha(1.0f);
    }

    std::function<void()> onAddPreset;
};

class PresetListItem : public juce::Component
{
public:
    PresetListItem(const Preset& preset, int index)
        : presetData(preset), presetIndex(index)
    {
        setSize(400, 40);

        // Add delete button (only for user presets)
        if (!presetData.isFactory)
        {
            deleteButton.setButtonText("X");
            deleteButton.onClick = [this] {
                if (onDelete)
                    onDelete(presetIndex);
            };
            deleteButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffff6b6b));
            deleteButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            addAndMakeVisible(deleteButton);
        }
        else
        {
            // Factory preset - show lock icon instead
            deleteButton.setButtonText("🔒");
            deleteButton.setEnabled(false);
            deleteButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffd8b5ff));
            deleteButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff6b4f9e));
            addAndMakeVisible(deleteButton);
        }
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Purple background when selected
        if (isSelected)
            g.fillAll(juce::Colour(0xffd8b5ff)); // Pastel purple for selected
        else if (isMouseOver())
            g.fillAll(juce::Colour(0xfffff0ff)); // Lighter pastel purple on hover

        g.drawRect(bounds, 1.0f);

        auto textArea = bounds.reduced(10, 5);

        // Leave space for delete button on the right
        textArea.removeFromRight(35);

        // Favorite star
        if (presetData.isFavorite)
        {
            g.drawText("★", textArea.removeFromLeft(20), juce::Justification::centred);
        }

        // Name
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText(presetData.name, textArea.removeFromLeft(180), juce::Justification::centredLeft);

        // Category
        g.setFont(juce::Font(11.0f));
        g.drawText(presetData.category, textArea.removeFromLeft(70), juce::Justification::centredLeft);

        // Rating
        juce::String stars;
        for (int i = 0; i < presetData.rating; ++i)
            stars += "★";
        g.drawText(stars, textArea, juce::Justification::centredRight);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        deleteButton.setBounds(bounds.removeFromRight(30).reduced(5));
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        // Don't trigger onClick if clicking on delete button
        if (!deleteButton.getBounds().contains(e.getPosition()))
        {
            if (onClick)
                onClick(presetIndex);
        }
    }

    void setSelected(bool shouldBeSelected)
    {
        isSelected = shouldBeSelected;
        repaint();
    }

    juce::String getPresetName() const
    {
        return presetData.name;
    }

    std::function<void(int)> onClick;
    std::function<void(int)> onDelete;

private:
    Preset presetData;  // Store a copy, not a reference (to avoid dangling reference)
    int presetIndex;
    bool isSelected = false;
    juce::TextButton deleteButton;
};

class PresetBrowser : public juce::Component
{
public:
    PresetBrowser(PresetManager& manager) : presetManager(manager)
    {
        // Search box - pastel colors, NO black/blue
        searchBox.setTextToShowWhenEmpty("Search presets...", juce::Colour(0xffc8a5ff));
        searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xfffff0ff));
        searchBox.setColour(juce::TextEditor::textColourId, juce::Colour(0xff6b4f9e));
        searchBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xffd8b5ff));
        searchBox.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xffc8a5ff));
        searchBox.onTextChange = [this] { updatePresetList(); };
        addAndMakeVisible(searchBox);
        
        // Category filter - pastel colors
        categorySelector.addItem("All", 1);
        for (int i = 0; i < presetManager.getCategories().size(); ++i)
        {
            categorySelector.addItem(presetManager.getCategories()[i], i + 2);
        }
        categorySelector.setSelectedId(1);
        categorySelector.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xfffff0ff));
        categorySelector.setColour(juce::ComboBox::textColourId, juce::Colour(0xff6b4f9e));
        categorySelector.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xffd8b5ff));
        categorySelector.setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff6b4f9e));
        categorySelector.onChange = [this] { updatePresetList(); };
        addAndMakeVisible(categorySelector);

        // Factory/User/All toggle - pastel colors
        factoryUserToggle.setButtonText("All");
        factoryUserToggle.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffd8b5ff));
        factoryUserToggle.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff6b4f9e));
        factoryUserToggle.onClick = [this] {
            // Cycle through: All -> Factory -> User -> All
            if (filterMode == 0)  // All -> Factory
                filterMode = 1;
            else if (filterMode == 1)  // Factory -> User
                filterMode = 2;
            else  // User -> All
                filterMode = 0;

            updateFilterButtonText();
            updatePresetList();
        };
        addAndMakeVisible(factoryUserToggle);

        // Favorites toggle - pastel colors
        favoritesButton.setButtonText("★ Favorites");
        favoritesButton.setToggleable(true);
        favoritesButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffd8b5ff));
        favoritesButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffc8a5ff));
        favoritesButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff6b4f9e));
        favoritesButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff6b4f9e));
        favoritesButton.onClick = [this] { updatePresetList(); };
        addAndMakeVisible(favoritesButton);

        // Save button - pastel colors
        saveButton.setButtonText("Save");
        saveButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffd8b5ff));
        saveButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff6b4f9e));
        saveButton.onClick = [this] { showSaveDialog(); };
        addAndMakeVisible(saveButton);
        
        // Preset list
        viewport.setViewedComponent(&presetContainer, false);
        addAndMakeVisible(viewport);
        
        updatePresetList();
    }
    
    void paint(juce::Graphics& g) override
    {
        // Pastel purple background
        g.fillAll(juce::Colour(0xfff5f0ff));

        auto titleArea = getLocalBounds().removeFromTop(50).toFloat();
        // Pastel pink/purple gradient for title
        g.setGradientFill(juce::ColourGradient(
            juce::Colour(0xffe8dcff), 0, 0,
            juce::Colour(0xffd8b5ff), 0, titleArea.getBottom(), false));
        g.fillRect(titleArea);

        g.setFont(juce::Font(20.0f, juce::Font::bold));
        g.setColour(juce::Colour(0xff6b4f9e)); // Darker purple for text
        g.drawText("PRESETS", titleArea.reduced(15), juce::Justification::centredLeft);

        // Pink border
        g.setColour(juce::Colour(0xffd8b5ff));
        g.drawRect(getLocalBounds(), 2);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        bounds.removeFromTop(40);
        
        // Controls row
        auto controlsArea = bounds.removeFromTop(40);
        factoryUserToggle.setBounds(controlsArea.removeFromLeft(90).reduced(2));
        controlsArea.removeFromLeft(10);
        categorySelector.setBounds(controlsArea.removeFromLeft(110).reduced(2));
        controlsArea.removeFromLeft(10);
        searchBox.setBounds(controlsArea.removeFromLeft(180).reduced(2));
        controlsArea.removeFromLeft(10);
        favoritesButton.setBounds(controlsArea.removeFromLeft(100).reduced(2));
        saveButton.setBounds(controlsArea.removeFromRight(80).reduced(2));
        
        bounds.removeFromTop(10);
        
        // Preset list
        viewport.setBounds(bounds);

        int ghostHeight = ghostItem ? 42 : 0;
        int totalHeight = presetItems.size() * 42 + ghostHeight;
        presetContainer.setSize(bounds.getWidth() - 20, totalHeight);

        auto itemBounds = presetContainer.getLocalBounds();

        // Position ghost item first if it exists
        if (ghostItem)
        {
            ghostItem->setBounds(itemBounds.removeFromTop(40));
            itemBounds.removeFromTop(2);
        }

        for (auto* item : presetItems)
        {
            item->setBounds(itemBounds.removeFromTop(40));
            itemBounds.removeFromTop(2);
        }
    }

    // Public method to refresh the preset list
    void refresh()
    {
        updatePresetList();
    }

    void setSelectedPresetName(const juce::String& name)
    {
        selectedPresetName = name;
        // Update visual state of all items
        for (auto* item : presetItems)
        {
            item->setSelected(item->getPresetName() == name);
        }
    }

    std::function<void(int)> onPresetSelected;

private:
    juce::String selectedPresetName;
    void updateFilterButtonText()
    {
        if (filterMode == 0)
            factoryUserToggle.setButtonText("All");
        else if (filterMode == 1)
            factoryUserToggle.setButtonText("Factory");
        else
            factoryUserToggle.setButtonText("User");
    }

    void updatePresetList()
    {
        presetItems.clear();
        presetContainer.removeAllChildren();

        juce::String searchText = searchBox.getText();
        juce::String category = categorySelector.getSelectedId() == 1 ?
            "All" : categorySelector.getText();
        bool favoritesOnly = favoritesButton.getToggleState();
        bool factoryOnly = (filterMode == 1);   // Show only factory presets
        bool userOnly = (filterMode == 2);      // Show only user presets

        auto results = presetManager.searchPresets(searchText, category, {}, favoritesOnly, factoryOnly, userOnly);

        // Add "Add New Preset" ghost box when showing user presets
        if (userOnly && searchText.isEmpty() && !favoritesOnly)
        {
            ghostItem = std::make_unique<AddNewPresetItem>();
            ghostItem->onAddPreset = [this] {
                showSaveDialog();
            };
            presetContainer.addAndMakeVisible(ghostItem.get());
        }
        else
        {
            ghostItem.reset();
        }

        for (int i = 0; i < results.size(); ++i)
        {
            auto* item = new PresetListItem(results[i], i);

            // Capture the preset name to find the actual index
            juce::String presetName = results[i].name;

            item->onClick = [this, presetName](int) {
                // Find the actual preset index by name
                auto allPresets = presetManager.getPresetNames();
                for (int j = 0; j < allPresets.size(); ++j)
                {
                    if (allPresets[j] == presetName)
                    {
                        // Load preset silently (no audio triggering)
                        presetManager.loadPresetSilently(j);
                        selectedPresetName = presetName;
                        // Update all items' selection state
                        for (auto* otherItem : presetItems)
                        {
                            if (auto* presetItem = dynamic_cast<PresetListItem*>(otherItem))
                                presetItem->setSelected(presetItem->getPresetName() == presetName);
                        }
                        if (onPresetSelected)
                            onPresetSelected(j);
                        break;
                    }
                }
            };

            // Set selection state based on current selected preset
            item->setSelected(presetName == selectedPresetName);

            item->onDelete = [this, presetName](int) {
                // Find the actual preset index by name for deletion
                auto allPresets = presetManager.getPresetNames();
                for (int j = 0; j < allPresets.size(); ++j)
                {
                    if (allPresets[j] == presetName)
                    {
                        presetManager.deletePreset(j);
                        updatePresetList(); // Refresh the list
                        break;
                    }
                }
            };

            presetItems.add(item);
            presetContainer.addAndMakeVisible(item);
        }
        
        resized();
    }
    
    void showSaveDialog()
    {
        // Simple save dialog
        juce::AlertWindow dialog("Save Preset", 
                                "Enter preset name and category",
                                juce::AlertWindow::NoIcon);
        
        dialog.addTextEditor("name", "New Preset", "Name:");
        dialog.addComboBox("category", presetManager.getCategories(), "Category:");
        dialog.addButton("Save", 1);
        dialog.addButton("Cancel", 0);
        
        if (dialog.runModalLoop() == 1)
        {
            juce::String name = dialog.getTextEditorContents("name");
            juce::String category = dialog.getComboBoxComponent("category")->getText();
            presetManager.savePreset(name, category);
            updatePresetList();
        }
    }
    
    PresetManager& presetManager;

    juce::TextButton factoryUserToggle;  // Factory/User/All toggle
    juce::TextEditor searchBox;
    juce::ComboBox categorySelector;
    juce::TextButton favoritesButton;
    juce::TextButton saveButton;

    juce::Viewport viewport;
    juce::Component presetContainer;
    juce::OwnedArray<PresetListItem> presetItems;
    std::unique_ptr<AddNewPresetItem> ghostItem;

    int filterMode = 0;  // 0 = All, 1 = Factory, 2 = User
};
