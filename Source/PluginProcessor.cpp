#include "UltimatePluckProcessor.h"
#include "UltimatePluckEditor.h"

//==============================================================================
// This is the plugin entry point - required by JUCE
//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UltimatePluckProcessor();
}
