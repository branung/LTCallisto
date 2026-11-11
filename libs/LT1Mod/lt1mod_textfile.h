#ifndef __LT1MOD_TEXTFILE_H__
#define __LT1MOD_TEXTFILE_H__

// A text file read from the engine's mounted trees as if fopen(..., "r") had opened it, so a
// loop written for fgets also reads the copy inside a rez.
//
// Gets follows fgets: at most n - 1 characters, up to and including a newline, terminated,
// NULL only when nothing is left. A CR right before an LF is dropped, as a Windows text-mode
// stream drops it; any other CR is kept.

#include <string>

namespace LT1Mod
{

class TextLines
{
public:
    TextLines() : m_pos(0) {}
    explicit TextLines(const std::string &text) : m_text(text), m_pos(0) {}

    void Reset(const std::string &text) { m_text = text; m_pos = 0; }

    char *Gets(char *buf, int n)
    {
        if (!buf || n <= 1 || m_pos >= m_text.size()) return 0;
        int k = 0;
        while (k < n - 1 && m_pos < m_text.size())
        {
            const char c = m_text[m_pos++];
            if (c == '\r' && m_pos < m_text.size() && m_text[m_pos] == '\n') continue;
            buf[k++] = c;
            if (c == '\n') break;
        }
        buf[k] = '\0';
        return buf;
    }

private:
    std::string m_text;
    size_t      m_pos;
};

} // namespace LT1Mod

#endif // __LT1MOD_TEXTFILE_H__
