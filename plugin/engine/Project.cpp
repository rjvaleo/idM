#include "Project.h"

#include <string>

namespace idm
{

namespace
{

/** A C-major seed riff, so the instrument makes sound the moment you press
    play. Ported from `createDefaultPattern`. */
constexpr int seedRiff[] = { 60, 62, 64, 65, 67, 69, 71, 72 };
constexpr int seedRiffLength = 8;

/** M's stored Cyclic Random copy, built the way `scrambleSteps` builds it:
    a Fisher-Yates shuffle of the played region, seeded at 1, with the unplayed
    tail carried over untouched. */
std::vector<StepEvent> scrambleSteps (const std::vector<StepEvent>& steps, int outputLength)
{
    std::vector<StepEvent> played (steps.begin(), steps.begin() + outputLength);
    const auto order = makeCyclicOrder (outputLength, 1);

    std::vector<StepEvent> out;
    out.reserve (steps.size());

    for (const auto index : order)
        out.push_back (played[(size_t) index]);

    for (size_t i = (size_t) outputLength; i < steps.size(); ++i)
        out.push_back (steps[i]);

    return out;
}

Pattern createDefaultPattern (bool withSeedRiff)
{
    Pattern pattern;
    pattern.steps.resize (stepCount);

    if (withSeedRiff)
        for (int i = 0; i < seedRiffLength; ++i)
            pattern.steps[(size_t) i].pitches = { seedRiff[i] };

    pattern.outputLength = withSeedRiff ? seedRiffLength : stepCount;
    pattern.scrambledSteps = scrambleSteps (pattern.steps, pattern.outputLength);

    return pattern;
}

VoiceState createDefaultVoice (int index)
{
    VoiceState voice;
    voice.patternIndex = (size_t) index;
    voice.playEnabled = index == 0;
    voice.transposition = 0;
    voice.noteOrderMix = { 100, 0, 0 };
    voice.density = 1.0;
    voice.velocityRange = { 48.0, 110.0 };
    voice.timeBaseNumerator = 1.0;
    voice.timeBaseDenominator = 8.0;
    voice.phase = 0.0;
    voice.timeDistort = {};
    voice.legato = 1.0;
    voice.outputChannels = { index + 1 };
    voice.mouseAdvance = false;

    return voice;
}

} // namespace

ProjectState createDefaultProject (int voices)
{
    const auto count = voices < 1 ? 1 : (voices > 16 ? 16 : voices);

    ProjectState state;
    state.tempo = 120.0;
    state.root = 0;
    state.scale = Scale::major;
    state.scaleSnap = false;
    state.seed = 1;
    state.diatonicTranspose = false;
    state.secondOrderTranspose = false;
    state.chordTones = false;

    state.patterns.reserve (patternCount);

    for (int i = 0; i < patternCount; ++i)
        state.patterns.push_back (createDefaultPattern (i == 0));

    state.voices.reserve ((size_t) count);

    for (int i = 0; i < count; ++i)
        state.voices.push_back (createDefaultVoice (i));

    const auto neutralCycle = [count]
    {
        std::vector<std::vector<CyclicStep>> rows;
        rows.reserve ((size_t) count);

        for (int i = 0; i < count; ++i)
            rows.push_back (std::vector<CyclicStep> ((size_t) stepCount,
                                                     CyclicStep::level (cyclicNeutralLevel)));

        return rows;
    };

    state.cyclic = { neutralCycle(), neutralCycle(), neutralCycle() };

    const std::vector<int> lengths ((size_t) count, stepCount);
    state.cyclicLengths = { lengths, lengths, lengths };

    state.cyclicValues = { { 6.0, 25.0, 50.0, 75.0, 100.0 }, { 1.0, 1.0, 1.5, 2.0, 5.0 } };

    return state;
}

} // namespace idm
