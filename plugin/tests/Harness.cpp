#include "Harness.h"

namespace idm::conformance
{

Totals& totals()
{
    static Totals shared;
    return shared;
}

std::string readFixture (const std::string& name)
{
    const auto path = std::string (IDM_GOLDENS_DIR) + "/" + name;
    std::ifstream file (path);

    if (! file)
    {
        std::printf ("cannot read %s\n  run: npm run goldens\n", path.c_str());
        std::exit (2);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::vector<std::string> split (const std::string& line, char delimiter)
{
    std::vector<std::string> out;
    std::stringstream stream (line);
    std::string part;

    while (std::getline (stream, part, delimiter))
        out.push_back (part);

    return out;
}

void walk (const std::string& name,
           const std::function<void (const std::string&,
                                     const std::vector<std::string>&,
                                     const std::string&)>& visit)
{
    const auto text = readFixture (name);
    std::stringstream stream (text);
    std::string line;
    std::string section;
    int lineNumber = 0;

    while (std::getline (stream, line))
    {
        ++lineNumber;

        if (line.empty())
            continue;

        if (line[0] == '#')
        {
            section = line.substr (1);
            continue;
        }

        visit (section, split (line, ','), name + ":" + std::to_string (lineNumber));
    }
}

} // namespace idm::conformance
