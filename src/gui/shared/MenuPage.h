#pragma once

#include <JuceHeader.h>

// Rotary slider that exposes its double-click to MenuPage so a theme knob can
// be repurposed for "assign this knob to the current menu item". Overriding
// mouseDoubleClick without calling the base also blocks the attachment's
// reset-to-default behaviour.
class AssignableSlider : public juce::Slider
{
public:
    using juce::Slider::Slider;

    std::function<void()> onDoubleClick;

private:
    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        if (onDoubleClick)
            onDoubleClick();
    }
};

// Generic LCD-style menu page component, shared across GUI themes.
// Pure view/controller: reads and writes parameters through the value tree state.
// Item types:
//   select      - prev/next changes the selection (triggers the param), inc/dec does nothing
//   value       - prev/next moves the cursor between items, inc/dec edits the current item's param
//   placeholder - inert row ("Soon to be implemented")
class MenuPage : public juce::Component,
                 public juce::AudioProcessorValueTreeState::Listener
{
public:
    enum class Type { value, select, placeholder };

    struct Item
    {
        juce::String id;            // APVTS parameter id (empty for placeholder)
        juce::String label;
        Type type = Type::placeholder;
        juce::StringArray options;  // select type only
        float step = 0.0f;          // value type only; 0 = auto (0.01 for 0..1 floats, 1 for ints/bools)
        juce::StringArray valueNames;  // value type only: index = integer value -> display name (empty = show number)
    };

    struct Page
    {
        juce::String title;
        juce::Array<Item> items;
    };

    MenuPage(juce::AudioProcessorValueTreeState& vts, const juce::Array<Page>& pages)
        : valueTreeState(vts), pageList(pages)
    {
        customFont = juce::Font(juce::Typeface::createSystemTypefaceFor(BinaryData::PressStart2PRegular_ttf, BinaryData::PressStart2PRegular_ttfSize));
        customFont.setHeight(12.0f);

        addAndMakeVisible(titleLabel);
        addAndMakeVisible(itemLabel);
        addAndMakeVisible(valueLabel);

        titleLabel.setFont(customFont);
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9bea81));
        titleLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        titleLabel.setInterceptsMouseClicks(false, false);

        itemLabel.setFont(customFont);
        itemLabel.setJustificationType(juce::Justification::topLeft);
        itemLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9bea81));
        itemLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        itemLabel.setInterceptsMouseClicks(false, false);

        valueLabel.setFont(customFont);
        valueLabel.setJustificationType(juce::Justification::centredRight);
        valueLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9bea81));
        valueLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        valueLabel.setInterceptsMouseClicks(false, false);

        for (auto& pg : pageList)
            for (auto& it : pg.items)
            {
                juce::String id = it.id.trim();
                if (id.isEmpty() || paramIDs.contains(id))
                    continue;
                paramIDs.add(id);
                valueTreeState.addParameterListener(id, this);
            }

        for (int i = 0; i < pageList.size(); ++i)
            if (pageList.getReference(i).title == "Modifications")
            {
                modPageIndex = i;
                break;
            }

        popupLookAndFeel.setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff333f26));

        selectPage(0);
    }

    ~MenuPage() override
    {
        for (auto& id : paramIDs)
            valueTreeState.removeParameterListener(id, this);
    }

    // Called by the theme's PST/OVD/MOD/SEQ buttons.
    void selectPage(int idx)
    {
        if (idx < 0 || idx >= pageList.size())
            return;
        currentPage = idx;
        cursor = 0;
        updateDisplay();
        notifyCurrentItemChanged();
    }

    // Called by the theme's prev/next buttons.
    void indexStep(int delta)
    {
        auto& item = currentItem();
        if (item.type == Type::placeholder)
            return;

        if (item.type == Type::select)
        {
            changeSelection(delta);
            return;
        }

        cursor = juce::jlimit(0, pageList.getReference(currentPage).items.size() - 1, cursor + delta);
        updateDisplay();
        notifyCurrentItemChanged();
    }

    // Called by the theme's dec/inc buttons. No-op on select/placeholder pages.
    void valueStep(float direction)
    {
        auto& item = currentItem();
        if (item.type != Type::value)
            return;
        changeValue(direction);
    }

    // Called by the theme's value knob. Same 0..100 scale as the display;
    // /100 is the only conversion, needed because the APVTS API is 0..1 normalized.
    void setValue(float percentage)
    {
        auto& item = currentItem();
        if (item.type != Type::value)
            return;
        auto* param = valueTreeState.getParameter(item.id);
        if (param == nullptr)
            return;
        param->beginChangeGesture();
        param->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, percentage / 100.0f));
        param->endChangeGesture();
        updateDisplay();
    }

    // Fired when the cursor/page changes. Carries the current item's value in the
    // knob's 0..100 scale, or -1 when the knob has nothing to edit (select/placeholder).
    std::function<void(float)> onCurrentItemChanged;

    // ---- assignable macro knobs ----
    // A slot binds a theme-owned rotary slider to a value item (by param id), so
    // themes can expose quick-access knobs for MOD parameters. Double-clicking the
    // slider assigns the currently selected menu item to it. Both directions of the
    // value stay in sync through a per-slot SliderAttachment.
    static constexpr int numAssignableSlots = 2;

    void bindAssignableSlider(int slot, AssignableSlider* slider, juce::Label* label)
    {
        if (slot < 0 || slot >= numAssignableSlots || slider == nullptr)
            return;
        assignableSlots[slot].slider = slider;
        assignableSlots[slot].label = label;
        slider->onDoubleClick = [this, slot] { assignCurrentItemToSlot(slot); };
        rebindAssignable(slot);
    }

    void setAssignableParam(int slot, const juce::String& paramID)
    {
        if (slot < 0 || slot >= numAssignableSlots)
            return;
        if (assignableSlots[slot].id == paramID)
            return;
        assignableSlots[slot].id = paramID;
        rebindAssignable(slot);
    }

    void assignCurrentItemToSlot(int slot)
    {
        if (slot < 0 || slot >= numAssignableSlots)
            return;
        if (currentPage != modPageIndex || currentItem().type != Type::value)
            return;
        for (int i = 0; i < numAssignableSlots; ++i)
            if (i != slot && assignableSlots[i].id == currentItem().id)
                return;   // other knob already has this control -> no-op
        setAssignableParam(slot, currentItem().id);
    }

    juce::String assignableParam(int slot) const
    {
        if (slot < 0 || slot >= numAssignableSlots)
            return {};
        return assignableSlots[slot].id;
    }

    float assignableValue(int slot) const
    {
        if (slot < 0 || slot >= numAssignableSlots)
            return -1.0f;
        auto* param = valueTreeState.getParameter(assignableSlots[slot].id);
        return param == nullptr ? -1.0f : param->getValue() * 100.0f;
    }

    juce::String assignableLabel(int slot) const
    {
        if (slot < 0 || slot >= numAssignableSlots)
            return {};
        const juce::String id = assignableSlots[slot].id;
        if (id.isEmpty())
            return {};
        for (auto& pg : pageList)
            for (auto& it : pg.items)
                if (it.id == id)
                    return it.label;
        return {};
    }

    // Fired when a slot is (re)bound so a theme can refresh anything extra.
    std::function<void(int)> onAssignableChanged;

    void parameterChanged(const juce::String& parameterID, float newValue) override
    {
        ignoreUnused(parameterID, newValue);
        updateDisplay();
        notifyCurrentItemChanged();
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (itemLabel.getBounds().contains(e.getPosition()))
            showItemMenu();
    }

private:
    Item& currentItem()
    {
        static Item placeholderItem;
        auto& page = pageList.getReference(currentPage);
        if (page.items.isEmpty())
            return placeholderItem;
        return page.items.getReference(juce::jlimit(0, page.items.size() - 1, cursor));
    }

    void notifyCurrentItemChanged()
    {
        if (!onCurrentItemChanged)
            return;
        float v = -1.0f;
        if (currentItem().type == Type::value)
            if (auto* param = valueTreeState.getParameter(currentItem().id))
                v = param->getValue() * 100.0f;
        onCurrentItemChanged(v);
    }

    struct AssignableSlot
    {
        juce::String id;
        juce::Slider* slider = nullptr;
        juce::Label* label = nullptr;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void rebindAssignable(int slot)
    {
        auto& s = assignableSlots[slot];
        s.attachment.reset();

        const bool bound = !s.id.isEmpty()
                           && s.slider != nullptr
                           && valueTreeState.getParameter(s.id) != nullptr;
        if (bound)
            s.attachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(valueTreeState, s.id, *s.slider));

        if (s.slider != nullptr)
            s.slider->setEnabled(bound);
        if (s.label != nullptr)
            s.label->setText(bound ? assignableLabel(slot) : juce::String(), juce::dontSendNotification);
        if (onAssignableChanged)
            onAssignableChanged(slot);
    }

    juce::AudioParameterInt* intParam(const juce::String& id)
    {
        return dynamic_cast<juce::AudioParameterInt*>(valueTreeState.getParameter(id));
    }

    void changeSelection(int delta)
    {
        auto& item = currentItem();
        auto* p = intParam(item.id);
        if (p == nullptr)
            return;
        p->beginChangeGesture();
        *p = juce::jlimit(p->getRange().getStart(), p->getRange().getEnd(), p->get() + delta);
        p->endChangeGesture();
        updateDisplay();
    }

    void changeValue(float direction)
    {
        auto& item = currentItem();
        auto* param = valueTreeState.getParameter(item.id);
        if (param == nullptr)
            return;

        const auto range = param->getNormalisableRange();
        float step = item.step > 0.0f
                         ? item.step
                         : (dynamic_cast<juce::AudioParameterInt*>(param) != nullptr
                            || dynamic_cast<juce::AudioParameterBool*>(param) != nullptr)
                               ? 1.0f
                               : (range.end - range.start) / 100.0f;

        float newValue = juce::jlimit(0.0f, 1.0f, param->getValue() + direction * step / (range.end - range.start));
        param->beginChangeGesture();
        param->setValueNotifyingHost(newValue);
        param->endChangeGesture();
        updateDisplay();
        notifyCurrentItemChanged();
    }

    static juce::String valueString(juce::RangedAudioParameter* param)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterInt*>(param))
            return juce::String(p->get());
        if (auto* p = dynamic_cast<juce::AudioParameterBool*>(param))
            return p->get() ? juce::String("1") : juce::String("0");
        return juce::String((int) juce::roundToInt(param->getValue() * 100.0f));
    }

    juce::String displayValue(const Item& item)
    {
        if (! item.valueNames.isEmpty())
            if (auto* p = intParam(item.id))
                return item.valueNames[juce::jlimit(0, item.valueNames.size() - 1, p->get())];
        return valueString(valueTreeState.getParameter(item.id));
    }

    juce::String itemText(const Item& item)
    {
        if (item.type == Type::placeholder)
            return "Soon to be implemented";

        if (item.type == Type::select)
        {
            auto* p = intParam(item.id);
            if (p == nullptr || item.options.isEmpty())
                return item.label;
            return item.options[juce::jlimit(0, item.options.size() - 1, p->get())];
        }

        return item.label;
    }

    void updateDisplay()
    {
        auto& page = pageList.getReference(currentPage);
        titleLabel.setText(page.title, juce::dontSendNotification);

        auto& item = currentItem();
        if (item.type == Type::value)
        {
            itemLabel.setText(item.label, juce::dontSendNotification);
            valueLabel.setText(displayValue(item), juce::dontSendNotification);
        }
        else
        {
            itemLabel.setText(itemText(item), juce::dontSendNotification);
            valueLabel.setText({}, juce::dontSendNotification);
        }

        resized();
        itemLabel.setFont(fontToFit(itemLabel.getText(), itemLabel.getWidth()));
        valueLabel.setFont(fontToFit(valueLabel.getText(), valueLabel.getWidth()));
        repaint();
    }

// Picks the largest item font so the text never wraps beyond one line.
    juce::Font fontToFit(const juce::String& text, int width)
    {
        juce::Font f = customFont;
        if (width > 0)
        {
            float size = customFont.getHeight();
            while (size > 9.0f && wrappedLineCount(f, text, width) > 1) // ponytail: 9pt floor, ellipsize at 9pt if still wrapping
            {
                size *= 0.85f;
                f.setHeight(size);
            }
        }
        return f;
    }

    static int wrappedLineCount(const juce::Font& font, const juce::String& text, int width)
    {
        juce::AttributedString as;
        as.setWordWrap(juce::AttributedString::byWord);
        as.append(text, font);
        juce::TextLayout layout;
        layout.createLayoutWithBalancedLineLengths(as, (float) width);
        return layout.getNumLines();
    }

    void showItemMenu()
    {
        auto& item = currentItem();
        if (item.type == Type::placeholder)
            return;

        juce::Component::SafePointer<MenuPage> safeThis(this);
        auto onChoice = [safeThis](int result)
        {
            if (safeThis != nullptr && result > 0)
                safeThis->handlePopupChoice(result - 1);
        };

        if (item.type == Type::select)
        {
            auto* p = intParam(item.id);
            if (p == nullptr)
                return;

            juce::PopupMenu menu;
            for (int i = 0; i < item.options.size(); ++i)
                menu.addItem(1 + i, item.options[i], true, i == p->get());
            menu.setLookAndFeel(&popupLookAndFeel);
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                               juce::ModalCallbackFunction::create(onChoice));
            return;
        }

        juce::PopupMenu menu;
        auto& page = pageList.getReference(currentPage);
        for (int i = 0; i < page.items.size(); ++i)
        {
            auto& it = page.items.getReference(i);
            juce::String text = it.type == Type::placeholder ? "Soon to be implemented" : it.label;
            menu.addItem(1 + i, text, true, i == cursor);
        }
        menu.setLookAndFeel(&popupLookAndFeel);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                           juce::ModalCallbackFunction::create(onChoice));
    }

    void handlePopupChoice(int pick)
    {
        auto& item = currentItem();
        if (item.type == Type::select)
        {
            auto* p = intParam(item.id);
            if (p != nullptr && pick < item.options.size())
            {
                p->beginChangeGesture();
                *p = pick;
                p->endChangeGesture();
                updateDisplay();
            }
            return;
        }

        cursor = juce::jlimit(0, pageList.getReference(currentPage).items.size() - 1, pick);
        updateDisplay();
        notifyCurrentItemChanged();
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(4);
        const int lineHeight = juce::roundToInt(customFont.getHeight() * 1.4f);
        const int rowGap = 10;

        auto titleArea = b.removeFromTop(lineHeight);
        b.removeFromTop(rowGap);

        titleLabel.setBounds(titleArea);
        auto itemRow = b.removeFromTop(lineHeight);

        const int textWidth = juce::roundToInt(customFont.getStringWidth(valueLabel.getText()));
        const auto border = valueLabel.getBorderSize();
        const int valueWidth = juce::jmin(textWidth + border.getLeftAndRight() + 2, itemRow.getWidth());
        const int rowGap2 = 6;
        itemLabel.setBounds(itemRow.withTrimmedRight(juce::jmin(valueWidth + rowGap2, itemRow.getWidth())));
        valueLabel.setBounds(itemRow.withTrimmedLeft(itemRow.getWidth() - valueWidth));
    }

    // ---- pages for this plugin (the per-page code lives here, isolated from the engine) ----
public:
    static juce::Array<Page> buildPages(const juce::StringArray& overdriveModelNames)
    {
        juce::Array<Page> pages;

        Page pst; pst.title = "Presets";
        pst.items.add(Item { {}, {}, Type::placeholder, {}, 0.0f });
        pages.add(pst);

        Page ovd; ovd.title = "Overdrive";
        ovd.items.add(Item { "overdriveModelIndex", "Model", Type::select, overdriveModelNames, 0.0f });
        pages.add(ovd);

        Page mod; mod.title = "Modifications";
        static const juce::Identifier modItemIDs[] =
        {
            "normalDecay", "accentDecay", "feedbackFilter",
            "softAttack", "slideTime", "sqrDriver",
            "lfoRate", "lfoDepth", "lfoWaveform", "lfoDestination"
        };
        static const juce::String modItemLabels[] =
        {
            "Normal decay", "Accent decay", "Filter Feedback",
            "Soft attack", "Slide time", "Square driver",
            "LFO rate", "LFO depth", "LFO wave", "LFO dest"
        };
        static constexpr uint8_t numModItems = 10;
        for (uint8_t i = 0; i < numModItems; ++i)
            mod.items.add(Item { modItemIDs[i].toString(), modItemLabels[i], Type::value, {}, 0.0f });

        for (auto& it : mod.items)
        {
            if (it.id == "lfoWaveform")
                it.valueNames = juce::StringArray { "Triangle", "Saw Up", "Saw Down", "Square", "Random", "Pink Noise" };
            else if (it.id == "lfoDestination")
                it.valueNames = juce::StringArray { "Cutoff", "Volume", "Pitch" };
        }
        pages.add(mod);

        Page seq; seq.title = "Sequencer";
        seq.items.add(Item { "seqLength",    "Length",     Type::value, {}, 0.0f });
        seq.items.add(Item { "seqShift",     "Shift",      Type::value, {}, 0.0f });
        seq.items.add(Item { "seqTempo",     "Tempo",      Type::value, {}, 0.0f });
        seq.items.add(Item { "seqSyncMode",  "Sync Mode",  Type::value, {}, 0.0f });
        seq.items.add(Item { "seqStartMode", "Start Mode", Type::value, {}, 0.0f });

        for (auto& it : seq.items)
        {
            if (it.id == "seqSyncMode")
                it.valueNames = juce::StringArray { "Internal", "Host", "Midi Clock" };
            else if (it.id == "seqStartMode")
                it.valueNames = juce::StringArray { "Transport", "Note Trigger" };
        }
        pages.add(seq);

        return pages;
    }

private:
    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::Array<Page> pageList;
    juce::StringArray paramIDs;
    AssignableSlot assignableSlots[numAssignableSlots];
    int currentPage = 0;
    int cursor = 0;
    int modPageIndex = -1;

    juce::Font customFont;
    juce::LookAndFeel_V4 popupLookAndFeel;

    juce::Label titleLabel;
    juce::Label itemLabel;
    juce::Label valueLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MenuPage)
};
