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
//   select      - prev/next moves the cursor between items, inc/dec and the knob change the selection
//   value       - prev/next moves the cursor between items, inc/dec and the knob edit the current item's param
//   placeholder - inert row ("Soon to be implemented")
class MenuPage : public juce::Component,
                 public juce::AudioProcessorValueTreeState::Listener
{
public:
    enum class Type { value, select, placeholder, action };

    struct Item
    {
        juce::String id;            // APVTS parameter id (empty for placeholder)
        juce::String label;
        Type type = Type::placeholder;
        juce::StringArray options;  // select type only
        float step = 0.0f;          // value type only; 0 = auto (0.01 for 0..1 floats, 1 for ints/bools)
        juce::StringArray valueNames;  // value type only: index = integer value -> display name (empty = show number)
        juce::String hiddenWhenId;     // item is skipped while this choice/int param equals hiddenWhenIndex
        int hiddenWhenIndex = -1;
        int shownFromIndex = 0;        // item is skipped while hiddenWhenId's choice index is below this
        bool needsMod = false;         // greyed out and inert while the Mods switch is off
        int shownUntilIndex = 1 << 30; // item is skipped while hiddenWhenId's choice index is above this
        juce::String altLabel;         // shown instead of label while hiddenWhenId's choice index equals altLabelIndex
        int altLabelIndex = -1;
        juce::String alsoHiddenWhenId;  // a second condition: also skipped while this choice param equals alsoHiddenWhenIndex
        int alsoHiddenWhenIndex = -1;
        int alsoHiddenOnlyAtIndex = -1;  // if >= 0, the second condition applies only while hiddenWhenId's index equals this
        juce::Array<int> alsoHiddenAtIndices;  // also skipped while hiddenWhenId's choice index is one of these
        bool showParamText = false;    // value type only: show the parameter's own text (units) instead of 0..100
        bool wrap = false;             // value type only: dec/inc wrap around the param range (int params)
        juce::String wrapAtParam;      // with wrap: wrap at this int param's value (0..value-1) instead
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

        paramIDs.add("switchModState");
        valueTreeState.addParameterListener("switchModState", this);

        for (auto& pg : pageList)
            for (auto& it : pg.items)
            {
                juce::String id = it.id.trim();
                if (id.isEmpty() || it.type == Type::action || paramIDs.contains(id))
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

        for (int i = 0; i < pageList.size(); ++i)
            pageCursor.add(0);

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
        pageCursor.getReference(currentPage) = cursor;
        currentPage = idx;
        cursor = pageCursor.getReference(idx);
        updateDisplay();
        notifyCurrentItemChanged();
    }

    // Called by the theme's prev/next buttons.
    void indexStep(int delta)
    {
        auto& item = currentItem();
        if (item.type == Type::placeholder)
            return;

        const int next = nextVisibleIndex(cursor, delta < 0 ? -1 : 1);
        if (next == cursor)
            return;
        cursor = next;
        pageCursor.getReference(currentPage) = cursor;
        updateDisplay();
        notifyCurrentItemChanged();
    }

    // Called by the theme's dec/inc buttons. No-op on placeholder items.
    void valueStep(float direction)
    {
        auto& item = currentItem();
        if (item.type == Type::action)
        {
            auto it = actions.find(item.id);
            if (it != actions.end() && it->second.step)
                it->second.step(direction);
            return;
        }
        if (item.type == Type::select)
        {
            changeSelection(direction < 0.0f ? -1 : 1);
            return;
        }
        if (item.type != Type::value || ! isItemEnabled(item))
            return;
        int wrapSpan = 0;
        if (item.wrap)
            if (auto* p = intParam(item.wrapAtParam.isEmpty() ? item.id : item.wrapAtParam))
                wrapSpan = item.wrapAtParam.isEmpty() ? p->getRange().getLength() + 1 : p->get();
        changeValue(item.id, item.step, direction, wrapSpan);
    }

    // Called by the theme's value knob. Same 0..100 scale as the display;
    // /100 is the only conversion, needed because the APVTS API is 0..1 normalized.
    void setValue(float percentage)
    {
        auto& item = currentItem();
        if (item.type == Type::select)
        {
            if (auto* p = intParam(item.id))
            {
                const int index = p->getRange().getStart()
                                  + juce::roundToInt(percentage / 100.0f * (float) p->getRange().getLength());
                if (index != p->get())
                    changeSelection(index - p->get());
            }
            return;
        }
        if (item.type != Type::value || ! isItemEnabled(item))
            return;
        auto* param = valueTreeState.getParameter(item.id);
        if (param == nullptr)
            return;
        param->beginChangeGesture();
        param->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, percentage / 100.0f));
        param->endChangeGesture();
        updateDisplay();
    }

    // ---- action items: not parameters; dec/inc call step(), the value text comes from text() ----
    struct Action
    {
        std::function<void(float)> step;
        std::function<juce::String()> text;
    };

    void setAction(const juce::String& id, Action action)
    {
        actions[id] = std::move(action);
        updateDisplay();
    }

    bool currentItemIsAction() { return currentItem().type == Type::action; }

    // Polled by the theme's timer: action values can change outside the menu.
    void refreshAction()
    {
        if (currentItemIsAction() && valueLabel.getText() != actionText(currentItem()))
            updateDisplay();
    }

    // Fired when the cursor/page changes. Carries the current item's value in the
    // knob's 0..100 scale, or -1 when the knob has nothing to edit (placeholder).
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
                    return labelOf(it);
        return {};
    }

    // Fired when a slot is (re)bound so a theme can refresh anything extra.
    std::function<void(int)> onAssignableChanged;

    void parameterChanged(const juce::String& parameterID, float newValue) override
    {
        ignoreUnused(parameterID, newValue);
        refreshAssignableEnabled();
        if (isHidden(currentItem()))
        {
            cursor = nextVisibleIndex(cursor, -1);
            if (isHidden(currentItem()))
                cursor = nextVisibleIndex(cursor, 1);
            pageCursor.getReference(currentPage) = cursor;
        }
        updateDisplay();
        notifyCurrentItemChanged();
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (itemLabel.getBounds().contains(e.getPosition()))
            showItemMenu();
    }

private:
    juce::String labelOf(const Item& item) const
    {
        if (item.altLabel.isNotEmpty())
            if (auto* p = valueTreeState.getParameter(item.hiddenWhenId))
                if ((int) std::lround(p->convertFrom0to1(p->getValue())) == item.altLabelIndex)
                    return item.altLabel;
        return item.label;
    }

    /** The choice / int parameter's current index, or -1 if there is no such parameter. */
    int choiceIndex(const juce::String& id) const
    {
        if (auto* p = valueTreeState.getParameter(id))
            return (int) std::lround(p->convertFrom0to1(p->getValue()));
        return -1;
    }

    bool isHidden(const Item& item) const
    {
        if (item.alsoHiddenWhenId.isNotEmpty() && (item.alsoHiddenOnlyAtIndex < 0 || choiceIndex(item.hiddenWhenId) == item.alsoHiddenOnlyAtIndex))
            if (choiceIndex(item.alsoHiddenWhenId) == item.alsoHiddenWhenIndex)
                return true;
        if (item.hiddenWhenId.isEmpty())
            return false;
        auto* p = valueTreeState.getParameter(item.hiddenWhenId);
        if (p == nullptr)
            return false;
        const int index = (int) std::lround(p->convertFrom0to1(p->getValue()));
        return index == item.hiddenWhenIndex || index < item.shownFromIndex || index > item.shownUntilIndex
            || item.alsoHiddenAtIndices.contains(index);
    }

    bool modsOn() const
    {
        auto* p = valueTreeState.getParameter("switchModState");
        return p != nullptr && p->getValue() > 0.5f;
    }

    bool isItemEnabled(const Item& item) const
    {
        return ! item.needsMod || modsOn();
    }

    void refreshAssignableEnabled()
    {
        for (int i = 0; i < numAssignableSlots; ++i)
        {
            auto& s = assignableSlots[i];
            const bool bound = s.attachment != nullptr;
            const bool enabled = bound && isItemEnabledForParam(s.id);
            if (s.slider != nullptr)
                s.slider->setEnabled(enabled);
            if (s.label != nullptr)
            {
                s.label->setText(bound ? assignableLabel(i) : juce::String(), juce::dontSendNotification);
                s.label->setAlpha(bound && ! enabled ? 0.4f : 1.0f);
            }
        }
    }

    bool isItemEnabledForParam(const juce::String& id) const
    {
        for (auto& pg : pageList)
            for (auto& it : pg.items)
                if (it.id == id)
                    return isItemEnabled(it);
        return true;
    }

    // Next visible item in the given direction; stays put when there is none.
    int nextVisibleIndex(int from, int dir) const
    {
        auto& items = pageList.getReference(currentPage).items;
        for (int i = from + dir; i >= 0 && i < items.size(); i += dir)
            if (! isHidden(items.getReference(i)))
                return i;
        return from;
    }

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
        if (currentItem().type != Type::placeholder)
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
            s.slider->setEnabled(bound && isItemEnabledForParam(s.id));
        if (s.label != nullptr)
        {
            s.label->setText(bound ? assignableLabel(slot) : juce::String(), juce::dontSendNotification);
            s.label->setAlpha(bound && ! isItemEnabledForParam(s.id) ? 0.4f : 1.0f);
        }
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

    // wrapSpan > 0: dec/inc wrap over that many values from the range start
    void changeValue(const juce::String& paramID, float itemStep, float direction, int wrapSpan)
    {
        auto* param = valueTreeState.getParameter(paramID);
        if (param == nullptr)
            return;

        if (auto* p = dynamic_cast<juce::AudioParameterInt*>(param); p != nullptr && wrapSpan > 0)
        {
            const int lo = p->getRange().getStart();
            const int span = wrapSpan;
            const int next = lo + (((p->get() - lo + (int) direction) % span) + span) % span;
            p->beginChangeGesture();
            *p = next;
            p->endChangeGesture();
            updateDisplay();
            notifyCurrentItemChanged();
            return;
        }

        const auto range = param->getNormalisableRange();
        float step = itemStep > 0.0f
                         ? itemStep
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
        if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(param); p != nullptr && p->range.start < 0.0f)
            return p->getCurrentValueAsText();   // bipolar: the parameter's own signed text (e.g. "+24%")
        if (auto* p = dynamic_cast<juce::AudioParameterInt*>(param))
            return juce::String(p->get());
        if (auto* p = dynamic_cast<juce::AudioParameterBool*>(param))
            return p->get() ? juce::String("1") : juce::String("0");
        return juce::String((int) juce::roundToInt(param->getValue() * 100.0f));
    }

    juce::String displayValue(const Item& item)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(valueTreeState.getParameter(item.id)))
            return p->choices[juce::jlimit(0, p->choices.size() - 1, p->getIndex())];
        if (! item.valueNames.isEmpty())
            if (auto* p = intParam(item.id))
                return item.valueNames[juce::jlimit(0, item.valueNames.size() - 1, p->get())];
        if (item.showParamText)
            if (auto* p = valueTreeState.getParameter(item.id))
                return p->getCurrentValueAsText();
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

        return labelOf(item);
    }

    void updateDisplay()
    {
        auto& page = pageList.getReference(currentPage);
        titleLabel.setText(page.title, juce::dontSendNotification);

        auto& item = currentItem();
        if (item.type == Type::action)
        {
            itemLabel.setText(item.label, juce::dontSendNotification);
            valueLabel.setText(actionText(item), juce::dontSendNotification);
        }
        else if (item.type == Type::value)
        {
            itemLabel.setText(labelOf(item), juce::dontSendNotification);
            valueLabel.setText(displayValue(item), juce::dontSendNotification);
        }
        else
        {
            itemLabel.setText(itemText(item), juce::dontSendNotification);
            valueLabel.setText({}, juce::dontSendNotification);
        }

        const float alpha = isItemEnabled(item) ? 1.0f : 0.4f;
        itemLabel.setAlpha(alpha);
        valueLabel.setAlpha(alpha);

        const juce::Font f = fontToFitRow(itemLabel.getText(), valueLabel.getText(), getWidth() - 8);
        itemLabel.setFont(f);
        valueLabel.setFont(f);
        resized();
        repaint();
    }

// Picks the largest font at which item label and value fit side by side on one row.
    juce::Font fontToFitRow(const juce::String& label, const juce::String& value, int rowWidth)
    {
        juce::Font f = customFont;
        const int gap = 6;
        auto needed = [&] { return juce::TextLayout::getStringWidth(f, label) + juce::TextLayout::getStringWidth(f, value) + gap + 4; };
        while (rowWidth > 0 && f.getHeight() > 7.0f && needed() > (float) rowWidth)
            f.setHeight(f.getHeight() * 0.9f);
        return f;
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
            menu.showMenuAsync(juce::PopupMenu::Options()
                                   .withTargetComponent(this)
                                   .withMousePosition(),
                               juce::ModalCallbackFunction::create(onChoice));
            return;
        }

        juce::PopupMenu menu;
        auto& page = pageList.getReference(currentPage);
        for (int i = 0; i < page.items.size(); ++i)
        {
            auto& it = page.items.getReference(i);
            if (isHidden(it))
                continue;
            juce::String text = it.type == Type::placeholder ? "Soon to be implemented" : labelOf(it);
            menu.addItem(1 + i, text, isItemEnabled(it), i == cursor);
        }
        menu.setLookAndFeel(&popupLookAndFeel);
        menu.showMenuAsync(juce::PopupMenu::Options()
                               .withTargetComponent(this)
                               .withMousePosition(),
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
        pageCursor.getReference(currentPage) = cursor;
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

        const int textWidth = juce::roundToInt(juce::TextLayout::getStringWidth(valueLabel.getFont(), valueLabel.getText()));
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
        Item disperse { "overdriveDisperse", "Disperse", Type::value, {}, 0.0f };   // allpass corner before the overdrive
        disperse.showParamText = true;
        ovd.items.add(disperse);
        pages.add(ovd);

        Page mod; mod.title = "Modifications";
        static const juce::Identifier modItemIDs[] =
        {
            "normalDecay", "accentDecay", "accentSweep", "feedbackFilter",
            "softAttack", "slideTime",
            "lfoRate", "lfoDepth", "lfoWaveform", "lfoDestination",
            "lfoPhase", "lfoSync", "lfoContour",
            "modSlot2Dest", "modSlot2Amount",
            "modSlot3Source", "modSlot3Dest", "modSlot3Amount",
            "modSlot4Source", "modSlot4Dest", "modSlot4Amount"
        };
        static const juce::String modItemLabels[] =
        {
            "Normal decay", "Accent decay", "Accent sweep", "Filter Feedback",
            "Soft attack", "Slide time",
            "LFO rate", "LFO depth", "LFO wave", "LFO dest",
            "LFO phase", "LFO key sync", "LFO contour",
            "Env dest", "Env amount",
            "Mod 3 source", "Mod 3 dest", "Mod 3 amount",
            "Mod 4 source", "Mod 4 dest", "Mod 4 amount"
        };

        // every Mods-page item is greyed out and inert while the Mods switch is off
        mod.items.add(Item { "filterType",  "Filter Model",     Type::value, {}, 1.0f, {}, {}, -1, 0, true });
        mod.items.add(Item { "filterMorph", "Filter Morph",     Type::value, {}, 0.0f, {}, "filterType", 0, 0, true });   // LP / BP / HP; no effect on TeeBee
        mod.items.add(Item { "resTrim",     "Res Trim",         Type::value, {}, 0.0f, {}, "filterType", 0, 0, true, 1 });   // Spicy only
        mod.items.add(Item { "filterQuality", "Filter Quality", Type::value, {}, 0.0f, {}, "filterType", 0, 0, true, 1 });   // Spicy only
        {
            Item tension { "filterTension", "Filter Tension", Type::value, {}, 0.0f, {}, "filterType", 0, 0, true, 1 };   // Spicy only
            tension.alsoHiddenWhenId = "filterQuality";   // Hi-Fi only: hidden in Standard
            tension.alsoHiddenWhenIndex = 0;
            mod.items.add(tension);
        }
        mod.items.add(Item { "filterDrive", "Filter Saturation", Type::value, {}, 0.0f, {}, "filterType", 0, 0, true });   // no effect on TeeBee
        {
            Item bias { "filterBias", "Saturation Bias", Type::value, {}, 0.0f, {}, "filterType", 0, 0, true };   // no effect on TeeBee
            bias.alsoHiddenAtIndices = { 3, 6 };   // JuneBug, SemiSweet: too faint to hear, held at 0
            mod.items.add(bias);
        }
        {
            Item rail { "filterRail", "Saturation Rail", Type::value, {}, 0.0f, {}, "filterType", 0, 0, true, 1 };   // Spicy only
            rail.alsoHiddenWhenId = "filterQuality";   // Hi-Fi only: hidden in Standard
            rail.alsoHiddenWhenIndex = 0;
            mod.items.add(rail);
        }
        mod.items.add(Item { "bassComp",    "Bass Comp",        Type::value, {}, 0.0f, {}, "filterType", 0, 0, true, 1 });   // Spicy only
        mod.items.add(Item { "filterFm",    "Filter FM",        Type::value, {}, 0.0f, {}, "filterType", 0, 0, true });   // no effect on TeeBee
        mod.items.add(Item { "oscModel",    "Osc Model",        Type::value, {}, 1.0f, {}, {}, -1, 0, true });
        mod.items.add(Item { "squareModel", "Square Model",     Type::value, {}, 1.0f, {}, "oscModel", -1, 0, true, 0 });   // TB-303 only
        mod.items.add(Item { "sqrDriver",   "Square Drive",     Type::value, {}, 0.0f, {}, "oscModel", -1, 0, true, 0 });   // TB-303 only
        mod.items.add(Item { "junoWave",    "Juno Wave",        Type::value, {}, 1.0f, {}, "oscModel", -1, 1, true, 1 });   // Alpha Juno only
        mod.items.add(Item { "superDetune", "Super Detune",     Type::value, {}, 0.0f, {}, "oscModel", -1, 1, true, 1 });
        mod.items.add(Item { "superMix",    "Super Mix",        Type::value, {}, 0.0f, {}, "oscModel", -1, 1, true, 1 });
        mod.items.add(Item { "shaperMode",  "Shaper Mode",      Type::value, {}, 1.0f, {}, "oscModel", -1, 2, true, 2 });   // Phaseshaper only
        mod.items.add(Item { "shapeDrift",  "Shape Drift",      Type::value, {}, 0.0f, {}, "oscModel", -1, 2, true, 2 });
        mod.items.add(Item { "subOscGain",  "Sub Osc",          Type::value, {}, 0.0f, {}, {}, -1, 0, true });
        mod.items.add(Item { "subOscBlend", "Sub Octave -2/-1", Type::value, {}, 0.0f, {}, {}, -1, 0, true });

        static constexpr uint8_t numModItems = juce::numElementsInArray (modItemIDs);
        for (uint8_t i = 0; i < numModItems; ++i)
            mod.items.add(Item { modItemIDs[i].toString(), modItemLabels[i], Type::value, {}, 0.0f, {}, {}, -1, 0, true });

        for (auto& it : mod.items)
        {
            if (it.id == "normalDecay" || it.id == "accentDecay" || it.id == "accentSweep" || it.id == "softAttack"
                || it.id == "slideTime" || it.id == "lfoRate" || it.id == "feedbackFilter"
                || it.id == "lfoContour" || it.id.endsWith("Amount"))
                it.showParamText = true;   // these params format themselves with their units
            if (it.id == "lfoWaveform" || it.id == "lfoDestination"
                || it.id.endsWith("Source") || it.id.endsWith("Dest"))
                it.step = 1.0f;   // Choice params step one index at a time
        }

        pages.add(mod);

        Page seq; seq.title = "Sequencer";
        seq.items.add(Item { "seqLength",    "Length",     Type::value, {}, 0.0f });
        seq.items.add(Item { "seqTempo",     "Tempo",      Type::value, {}, 0.0f });
        seq.items.add(Item { "seqSyncMode",  "Sync Mode",  Type::value, {}, 1.0f });
        seq.items.add(Item { "seqStartMode", "Start Mode", Type::value, {}, 1.0f });
        seq.items.add(Item { "seqTranspose", "Transpose",  Type::value, {}, 1.0f });
        {
            // wrap at the pattern length: the engine reads each change the shorter way round it
            Item shift { "seqShift", "Shift", Type::value, {}, 1.0f };
            shift.wrap = true;
            shift.wrapAtParam = "seqLength";
            seq.items.add(shift);
            Item rotate { "seqPitchRotate", "Pitch Rot", Type::value, {}, 1.0f };
            rotate.wrap = true;
            rotate.wrapAtParam = "seqLength";
            seq.items.add(rotate);
            Item pitchLen { "seqPitchLength", "Pitch Len", Type::value, {}, 1.0f };
            pitchLen.valueNames.add("Off");
            for (int i = 1; i <= 16; ++i)
                pitchLen.valueNames.add(juce::String(i));
            seq.items.add(pitchLen);
        }
        {
            Item swing { "seqSwing", "Swing", Type::value, {}, 0.5f };
            swing.showParamText = true;   // e.g. 62.5%
            seq.items.add(swing);
        }
        {
            Item cpuTiming { "seqCpuTiming", "CPU Timing", Type::value, {}, 1.0f };
            cpuTiming.showParamText = true;   // On / Off
            seq.items.add(cpuTiming);
        }
        seq.items.add(Item { "seqUndo", "Undo", Type::action, {}, 0.0f });
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
    std::map<juce::String, Action> actions;

    juce::String actionText(const Item& item)
    {
        auto it = actions.find(item.id);
        return (it != actions.end() && it->second.text) ? it->second.text() : juce::String();
    }
    juce::Array<int> pageCursor;
    int modPageIndex = -1;

    juce::Font customFont;
    juce::LookAndFeel_V4 popupLookAndFeel;

    juce::Label titleLabel;
    juce::Label itemLabel;
    juce::Label valueLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MenuPage)
};
