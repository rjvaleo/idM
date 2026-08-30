#include "ProjectJson.h"

namespace mclassic
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

ProjectState projectFromJson (const juce::var& document)
{
    ProjectState state = createDefaultProject();

    if (! document.isObject())
        return state;

    // The interface sends a whole `.mclone` document — the musical project plus
    // the Variable Positions, Snapshots and Slideshows around it. Only the
    // project concerns the engine, but the rest has to survive a session, so
    // the document is what crosses the bridge and this reaches inside it.
    const auto nested = document.getProperty ("project", juce::var());
    const auto& root = nested.isObject() ? nested : document;

    state.tempo = num (root, "tempo", state.tempo);
    state.root = (int) num (root, "root", state.root);
    state.scaleSnap = flag (root, "scaleSnap");
    state.seed = (uint32_t) num (root, "seed", (double) state.seed);
    state.diatonicTranspose = flag (root, "diatonicTranspose");
    state.secondOrderTranspose = flag (root, "secondOrderTranspose");
    state.chordTones = flag (root, "chordTones");

    const auto scaleName = root.getProperty ("scale", juce::var()).toString();

    if (scaleName.isNotEmpty())
        scaleFromName (scaleName.toStdString(), state.scale);

    if (const auto* patterns = arrayOf (root, "patterns"))
    {
        state.patterns.clear();

        for (const auto& pattern : *patterns)
            state.patterns.push_back ({ stepsOf (pattern, "steps"),
                                        stepsOf (pattern, "scrambledSteps"),
                                        (int) num (pattern, "outputLength") });
    }

    if (const auto* voices = arrayOf (root, "voices"))
    {
        state.voices.clear();

        for (const auto& voice : *voices)
            state.voices.push_back (voiceOf (voice));
    }

    const auto cyclic = root.getProperty ("cyclic", juce::var());

    if (cyclic.isObject())
        state.cyclic = { cyclicRows (cyclic, "accent"),
                         cyclicRows (cyclic, "legato"),
                         cyclicRows (cyclic, "rhythm") };

    const auto lengths = root.getProperty ("cyclicLengths", juce::var());

    if (lengths.isObject())
        state.cyclicLengths = { intsOf (lengths, "accent"),
                                intsOf (lengths, "legato"),
                                intsOf (lengths, "rhythm") };

    const auto values = root.getProperty ("cyclicValues", juce::var());

    if (values.isObject())
        state.cyclicValues = { doublesOf (values, "legato"), doublesOf (values, "rhythm") };

    return state;
}

} // namespace mclassic
