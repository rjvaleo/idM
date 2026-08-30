#pragma once

#include "Cyclic.h"
#include "Music.h"
#include "TimeMap.h"
#include "Transform.h"

#include <cstddef>
#include <vector>

namespace idm
{

/** One step of a Pattern. Only the pitches matter to planning. */
struct StepEvent
{
    std::vector<int> pitches;
};

/** A Pattern is the raw note material — M's core unit. */
struct Pattern
{
    std::vector<StepEvent> steps;
    /** M's stored Cyclic Random copy of the Original list. */
    std::vector<StepEvent> scrambledSteps;
    /** Number of steps actually played; never more than `steps.size()`. */
    int outputLength = 0;
};

/** Live state of one Voice — a path through the program. */
struct VoiceState
{
    size_t patternIndex = 0;
    bool playEnabled = false;
    /** Semitones, or scale steps when `diatonicTranspose` is set. */
    int transposition = 0;
    NoteOrderMix noteOrderMix {};
    /** Probability in 0..1 that a step sounds. */
    double density = 1.0;
    VelocityRange velocityRange {};
    /** Multiplier that slows the Voice. */
    double timeBaseNumerator = 1.0;
    /** Division of a whole note: 4 = quarter, 8 = eighth. Zero stops the Voice. */
    double timeBaseDenominator = 8.0;
    /** Initial delay in M ticks; 96 M ticks are one quarter note. */
    double phase = 0.0;
    TimeMap timeDistort {};
    /** Per-Voice multiplier over the Cyclic Legato percentage. */
    double legato = 1.0;
    /** Orchestration: any combination of channels 1..16. */
    std::vector<int> outputChannels;
    /** When set, the Voice advances only by Input Control. */
    bool mouseAdvance = false;
};

enum class CyclicKind { accent, legato, rhythm };

/** Five-level, sixteen-step modulation cycles, per variable and per Voice. */
struct CyclicVariables
{
    std::vector<std::vector<CyclicStep>> accent;
    std::vector<std::vector<CyclicStep>> legato;
    std::vector<std::vector<CyclicStep>> rhythm;

    const std::vector<std::vector<CyclicStep>>& get (CyclicKind kind) const noexcept
    {
        return kind == CyclicKind::accent ? accent
             : kind == CyclicKind::legato ? legato
                                          : rhythm;
    }
};

struct CyclicLengths
{
    std::vector<int> accent, legato, rhythm;

    const std::vector<int>& get (CyclicKind kind) const noexcept
    {
        return kind == CyclicKind::accent ? accent
             : kind == CyclicKind::legato ? legato
                                          : rhythm;
    }
};

/** The values each level maps to. Accent has none: its level is the velocity
    step itself. */
struct CyclicValues
{
    std::vector<double> legato;
    std::vector<double> rhythm;
};

struct ProjectState
{
    double tempo = 120.0;
    std::vector<Pattern> patterns;
    std::vector<VoiceState> voices;
    /** Key root pitch class, 0..11. */
    int root = 0;
    Scale scale = Scale::chromatic;
    bool scaleSnap = false;
    uint32_t seed = 1;
    /** Interpret transposition as scale steps. */
    bool diatonicTranspose = false;
    /** Stack Voice transpositions cumulatively — a harmonizer feeding a
        harmonizer, which builds implied chords. */
    bool secondOrderTranspose = false;
    /** Snap final pitches to the tonic triad. */
    bool chordTones = false;
    CyclicVariables cyclic;
    CyclicLengths cyclicLengths;
    CyclicValues cyclicValues;

    /** The Voice count is `voices.size()`, never a separate field. */
    size_t voiceCount() const noexcept { return voices.size(); }
};

/** M's neutral cyclic level, the middle of the five. */
inline constexpr int cyclicNeutralLevel = 2;

/** Steps in a Pattern, and Patterns in a project: six groups of four. */
inline constexpr int stepCount = 16;
inline constexpr int patternCount = 24;

/** The project M opens with — four Voices, Pattern 0 carrying a C-major seed
    riff so the instrument makes sound the moment the transport starts. */
ProjectState createDefaultProject (int voices = 4);

} // namespace idm
