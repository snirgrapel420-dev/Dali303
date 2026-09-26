#include "Pattern.h"

#include <cctype>
#include <sstream>
#include <vector>

namespace dali
{
uint32_t Pattern::hash() const noexcept
{
    uint32_t h = 2166136261U;
    for (int i = 0; i < length; ++i)
    {
        h ^= packStep (steps[(size_t) i]);
        h *= 16777619U;
    }
    return h;
}

uint32_t packStep (const Step& s) noexcept
{
    return (uint32_t) (s.note & 0x7f)
         | (s.gate   ? (1U << 8)  : 0U)
         | (s.accent ? (1U << 9)  : 0U)
         | (s.slide  ? (1U << 10) : 0U);
}

Step unpackStep (uint32_t v) noexcept
{
    Step s;
    s.note   = (uint8_t) (v & 0x7f);
    s.gate   = (v & (1U << 8))  != 0;
    s.accent = (v & (1U << 9))  != 0;
    s.slide  = (v & (1U << 10)) != 0;
    return s;
}

static const char* const kNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

std::string noteName (int midiNote)
{
    if (midiNote < 0) return "--";
    return std::string (kNames[midiNote % 12]) + std::to_string (midiNote / 12 - 1);
}

std::string patternToString (const Pattern& p)
{
    std::string out;
    for (int i = 0; i < Pattern::kMaxSteps; ++i)
    {
        const auto& s = p.steps[(size_t) i];
        if (i > 0) out += ' ';
        out += noteName (s.note);
        if (! s.gate)  out += 'R';
        if (s.accent)  out += 'A';
        if (s.slide)   out += 'S';
    }
    return out;
}

static bool parseToken (const std::string& tok, int prevNote, Step& s)
{
    if (tok == "-" || tok == ".")
    {
        s = Step {};
        s.note = (uint8_t) prevNote;
        s.gate = false;
        return true;
    }

    size_t i = 0;
    const char letter = (char) std::toupper ((unsigned char) tok[i++]);
    static const int pcs[7] = { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
    if (letter < 'A' || letter > 'G') return false;
    int pc = pcs[letter - 'A'];

    if (i < tok.size() && tok[i] == '#') { ++pc; ++i; }
    else if (i < tok.size() && tok[i] == 'b') { --pc; ++i; }

    int sign = 1;
    if (i < tok.size() && tok[i] == '-') { sign = -1; ++i; }
    if (i >= tok.size() || ! std::isdigit ((unsigned char) tok[i])) return false;
    int octave = 0;
    while (i < tok.size() && std::isdigit ((unsigned char) tok[i])) octave = octave * 10 + (tok[i++] - '0');
    octave *= sign;

    const int note = (octave + 1) * 12 + pc;
    if (note < 0 || note > 127) return false;

    s = Step {};
    s.note = (uint8_t) note;
    s.gate = true;
    for (; i < tok.size(); ++i)
    {
        switch (std::toupper ((unsigned char) tok[i]))
        {
            case 'A': s.accent = true; break;
            case 'S': s.slide  = true; break;
            case 'R': s.gate   = false; break;
            default:  return false;
        }
    }
    return true;
}

bool patternFromString (const std::string& text, Pattern& out)
{
    std::istringstream in (text);
    std::vector<std::string> tokens;
    std::string t;
    while (in >> t) tokens.push_back (t);
    if (tokens.empty() || tokens.size() > (size_t) Pattern::kMaxSteps) return false;

    Pattern p;
    int prev = 36;
    for (size_t i = 0; i < tokens.size(); ++i)
    {
        Step s;
        if (! parseToken (tokens[i], prev, s)) return false;
        p.steps[i] = s;
        prev = s.note;
    }
    for (size_t i = tokens.size(); i < (size_t) Pattern::kMaxSteps; ++i)
    {
        p.steps[i] = Step {};
        p.steps[i].note = (uint8_t) prev;
        p.steps[i].gate = false;
    }
    p.length = Pattern::kMaxSteps;
    out = p;
    return true;
}
} // namespace dali
