/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2022 - Raw Material Software Limited

   JUCE is an open source library subject to commercial or open-source
   licensing.

   By using JUCE, you agree to the terms of both the JUCE 7 End-User License
   Agreement and JUCE Privacy Policy.

   End User License Agreement: www.juce.com/juce-7-licence
   Privacy Policy: www.juce.com/juce-privacy-policy

   Or: You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

#pragma once

#include <juce_audio_plugin_client/utility/juce_IncludeModuleHeaders.h>

namespace juce::detail
{

struct PluginUtilities
{
    PluginUtilities() = delete;

    static int getDesktopFlags (const AudioProcessorEditor& editor)
    {
        return editor.wantsLayerBackedView()
             ? 0
             : ComponentPeer::windowRequiresSynchronousCoreGraphicsRendering;
    }

    static int getDesktopFlags (const AudioProcessorEditor* editor)
    {
        return editor != nullptr ? getDesktopFlags (*editor) : 0;
    }

    static void addToDesktop (AudioProcessorEditor& editor, void* parent)
    {
        editor.addToDesktop (getDesktopFlags (editor), parent);
    }
};

} // namespace juce::detail
