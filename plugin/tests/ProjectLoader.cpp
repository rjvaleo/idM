#include "ProjectLoader.h"
#include "Harness.h"

#include <juce_core/juce_core.h>

namespace mclassic::conformance
{

namespace
{

double num (const juce::var& v, const char* key, double fallback = 0.0)
{
    const auto value = v.getProperty (key, juce::var());
    return value.isVoid() ? fallback : (double) value;
}

bool flag (const juce::var& v, const char* key)
{
    return (bool) v.getProperty (key, juce::var (false));
}

const juce::Array<juce::var>* arrayOf (const juce::var& v, const char* key)
{
    return v.getProperty (key, juce::var()).getArray();
}

std::vector<StepEvent> stepsOf (const juce::var& pattern, const char* key)
{
    std::vector<StepEvent> out;

    if (const auto* steps = arrayOf (pattern, key))
        for (const auto& step : *steps)
        {
            StepEvent event;

            if (const auto* pitches = arrayOf (step, "pitches"))
                for (const auto& pitch : *pitches)
                    event.pitches.push_back ((int) pitch);

            out.push_back (std::move (event));
        }

    return out;
}

CyclicStep cyclicStepOf (const juce::var& v)
{
    // A bare number is the older form; a {min,max} object is the newer one.
    if (v.isObject())
        return CyclicStep::range (num (v, "min"), num (v, "max"));

    return CyclicStep::level ((double) v);
}

std::vector<std::vector<CyclicStep>> cyclicRows (const juce::var& cyclic, const char* kind)
{
    std::vector<std::vector<CyclicStep>> out;

    if (const auto* voices = arrayOf (cyclic, kind))
        for (const auto& voice : *voices)
        {
            std::vector<CyclicStep> row;

            if (const auto* steps = voice.getArray())
                for (const auto& step : *steps)
                    row.push_back (cyclicStepOf (step));

            out.push_back (std::move (row));
        }

    return out;
}

std::vector<int> intsOf (const juce::var& v, const char* key)
{
    std::vector<int> out;

    if (const auto* values = arrayOf (v, key))
        for (const auto& value : *values)
            out.push_back ((int) value);

    return out;
}

std::vector<double> doublesOf (const juce::var& v, const char* key)
{
    std::vector<double> out;

    if (const auto* values = arrayOf (v, key))
        for (const auto& value : *values)
            out.push_back ((double) value);

    return out;
}

TimeMap timeMapOf (const juce::var& v)
{
    TimeMap map;
    map.length = num (v, "length", 1.0);
    map.denominator = num (v, "denominator", 4.0);

    if (const auto* points = arrayOf (v, "points"))
        for (const auto& point : *points)
            map.points.push_back ({ num (point, "x"), num (point, "y") });

    return map;
}

VoiceState voiceOf (const juce::var& v)
{
    VoiceState voice;
    voice.patternIndex = (size_t) num (v, "patternIndex");
    voice.playEnabled = flag (v, "playEnabled");
    voice.transposition = (int) num (v, "transposition");

    const auto mix = v.getProperty ("noteOrderMix", juce::var());
    voice.noteOrderMix = { (int) num (mix, "original"),
                           (int) num (mix, "cyclic"),
                           (int) num (mix, "utterly") };

    voice.density = num (v, "density");

    const auto range = v.getProperty ("velocityRange", juce::var());
    voice.velocityRange = { num (range, "low"), num (range, "high") };

    voice.timeBaseNumerator = num (v, "timeBaseNumerator", 1.0);
    voice.timeBaseDenominator = num (v, "timeBaseDenominator", 8.0);
    voice.phase = num (v, "phase");
    voice.timeDistort = timeMapOf (v.getProperty ("timeDistort", juce::var()));
    voice.legato = num (v, "legato", 1.0);
    voice.outputChannels = intsOf (v, "outputChannels");
    voice.mouseAdvance = flag (v, "mouseAdvance");

    return voice;
}

} // namespace

ProjectState loadProject (const std::string& fixtureName)
{
    const auto text = readFixture (fixtureName);
    const auto root = juce::JSON::parse (juce::String (text));

    if (! root.isObject())
    {
        std::printf ("  %s is not valid JSON\n", fixtureName.c_str());
        std::exit (2);
    }

    ProjectState state;
    state.tempo = num (root, "tempo", 120.0);
    state.root = (int) num (root, "root");
    state.scaleSnap = flag (root, "scaleSnap");
    state.seed = (uint32_t) num (root, "seed", 1.0);
    state.diatonicTranspose = flag (root, "diatonicTranspose");
    state.secondOrderTranspose = flag (root, "secondOrderTranspose");
    state.chordTones = flag (root, "chordTones");

    const auto scaleName = root.getProperty ("scale", juce::var ("chromatic")).toString();

    if (! scaleFromName (scaleName.toStdString(), state.scale))
    {
        std::printf ("  unknown scale %s\n", scaleName.toRawUTF8());
        std::exit (2);
    }

    if (const auto* patterns = arrayOf (root, "patterns"))
        for (const auto& pattern : *patterns)
            state.patterns.push_back ({ stepsOf (pattern, "steps"),
                                        stepsOf (pattern, "scrambledSteps"),
                                        (int) num (pattern, "outputLength") });

    if (const auto* voices = arrayOf (root, "voices"))
        for (const auto& voice : *voices)
            state.voices.push_back (voiceOf (voice));

    const auto cyclic = root.getProperty ("cyclic", juce::var());
    state.cyclic = { cyclicRows (cyclic, "accent"),
                     cyclicRows (cyclic, "legato"),
                     cyclicRows (cyclic, "rhythm") };

    const auto lengths = root.getProperty ("cyclicLengths", juce::var());
    state.cyclicLengths = { intsOf (lengths, "accent"),
                            intsOf (lengths, "legato"),
                            intsOf (lengths, "rhythm") };

    const auto values = root.getProperty ("cyclicValues", juce::var());
    state.cyclicValues = { doublesOf (values, "legato"), doublesOf (values, "rhythm") };

    return state;
}

} // namespace mclassic::conformance
