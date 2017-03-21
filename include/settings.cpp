#include <fstream>

#include "settings.h"

#include <iostream>


#if defined ASSERT
#define ASSERT_(cond) ASSERT(cond)
#elif defined assert
#define ASSERT_(cond) assert(cond); if (false) std::stringstream()
#else
#define ASSERT_(cond) if (false) std::stringstream()
#endif


namespace settings {

    namespace {
        /** Parses single command line argument.

        The OS command line argument parsing already takes care of the quotes, therefore we only split the argument into its value and name parts.
        */
        void ParseCmdArg(char * arg, std::string & name, std::string & value) {
            size_t i = 0;
            size_t nameEnd = std::string::npos;
            while (arg[i] != 0) {
                if (arg[i] == '=' and nameEnd == std::string::npos)
                    nameEnd = i;
                ++i;
            }
            if (nameEnd == std::string::npos) {
                name = arg;
                value.clear();
            } else {
                name = std::string(arg, nameEnd);
                value = std::string(arg + nameEnd + 1, i - nameEnd - 1);
            }
        }

    } // anonymous namespace


    Command & ParseCommandLine(int argc, char * argv[], Settings & settings) {
        if (argc < 2)
            throw std::invalid_argument("No command specified");
        std::string cmd = argv[1];
        Command * command = dynamic_cast<Command*>(& settings[cmd]);
        if (command == nullptr)
            throw std::invalid_argument(STR("Unknown command " << cmd));

        ParseCommandLineInto(argc, argv, *command, 2);
        return *command;
    }


    void ParseCommandLineInto(int argc, char * argv[], Command & into, int fromIndex) {
        std::string argName;
        std::string argValue;
        for (int i = fromIndex; i < argc; ++i) {
            ParseCmdArg(argv[i], argName, argValue);
            Value * v = nullptr;
            if (into.hasItem(argName)) {
                v = &into[argName];
            } else {
                try {
                    v = &into.defaultValue();
                } catch (std::invalid_argument) {
                    throw std::invalid_argument(STR("Invalid argument name " << argName << " or no default arguments allowed"));
                }
                // when default value is used, take the entire command as given
                argValue = argv[i]; 
            }
            // try if the value is a variable
            Variable * var = dynamic_cast<Variable*>(v);
            if (var != nullptr) {
                if (not into.supplied(*v)) {
                    switch (into.allowRedefinition(*v)) {
                        case Command::AllowRedefinition::Warning:
                            // TODO the warning
                        case Command::AllowRedefinition::Yes:
                            break;
                        case Command::AllowRedefinition::No:
                            throw std::invalid_argument(STR("Argument " << argName << " already defined"));
                    }
                }
                var->parseString(argValue);
                continue;
            } else {
                Array * a = dynamic_cast<Array*>(v);
                if (a != nullptr) {
                    Value * item = & a->append();
                    Variable * vItem = dynamic_cast<Variable*>(item);
                    if (vItem != nullptr) {
                        vItem->parseString(argValue);
                        continue;
                    }
                } else {
                    ASSERT_(false) << "Invalid type for command line argument";
                }
            }
            throw std::invalid_argument(STR("Unknown command line argument " << argName));
        }
    }


    namespace {
        /** Parses INI file section name.

        The section name must start with `[` and end with `]`, must not span multiple lines and all characters inside the `[]` are part of the section name. Whitespace characters are allowed after the section name.
        */
        std::string ParseINISectionName(std::string const & from) {
            size_t i = 1;
            while (from[i] != ']') {
                ++i;
                if (i == from.size())
                    throw std::invalid_argument(STR("Unterminated section name " << from));
            }
            return from.substr(1, i - 1);
        }
        /** Parses escaped character and advances the counter.

        `\` followed immediately by a new line in the input just preserves the new line.
        */
        void ParseEscapedCharacter(std::string const & line, size_t & i, std::string & value) {
            ++i;
            // if the escape is followed by new line, just use the new line
            if (i < line.size()) {
                char c = line[i++];
                switch (c) {
                    case '0':
                        value += '\0';
                        break;
                    case '"':
                    case '\'':
                    case '%':
                    case '_':
                    case '\\':
                        value += line[i];
                        break;
                    case 'Z':
                        value += (char)26;
                        break;
                    case 'n':
                        value += '\n';
                        break;
                    case 'r':
                        value += '\r';
                        break;
                    case 't':
                        value += '\t';
                        break;
                    case 'b':
                        value += '\b';
                        break;
                    default:
                        throw std::invalid_argument(STR("Unknown escape character " << line[i]));
                }
            }
        }


        /** Parses a quoted value from the given stream.

        Takes the loaded line, index to the line where the value starts, the value to be returned, stream from which to read extra lines if the value spans multiple lines and line number.
        */
        void ParseQuotedValue(std::string & line, size_t & i, std::string & value, std::istream & f, size_t & ln) {
            // a flag which determines wheter a new line should be added to the value when new line is encountered
            char quote = line[i++];
            value.clear();
            while (true) {
                // if we are at the end of line, read next one
                if (i == line.size()) {
                    std::string l;
                    if (not std::getline(f, l))
                        throw std::ios_base::failure(STR("Unterminated quoted value at line " << ln));
                    ++ln;
                    value += '\n';
                    line += l; // append the line
                    continue;
                }
                // now we know there is something we can read
                if (line[i] == quote) {
                    ++i; // move past the quote
                    break;
                }
                // deal with escape characters
                if (line[i] == '\\')
                    ParseEscapedCharacter(line, i, value);
                // or add the character to the value
                else
                    value += line[i++];
            }
        }


        /** Parses a name = value line in the INI file.

        Deals with whitespace (allowed before and after the `=` and after the value as well as quoted values which may span multiple lines, by calling the ParseQuotedValue.
        */
        void ParseINILine(std::string & line, std::string & name, std::string & value, std::istream & f, size_t & ln) {
            size_t i = 0;
            size_t nameEnd = 0;
            while (true) {
                if (i == line.size())
                    throw std::invalid_argument(STR("Option name not found at line " << ln));
                if (line[i] == '=')
                    break;
                if (line[i] != ' ' and line[i] != '\t')
                    nameEnd = i;
                ++i;
            }
            name = line.substr(0, nameEnd + 1); // set the name
            ++i; // advance past '='
                 // get rid of any whitespace after the assignment sign
            while (line[i] == ' ' or line[i] == '\t')
                ++i;
            // this is where value starts
            size_t valueStart = i;
            // if there is an empty value, return
            if (valueStart == line.size())
                return;
            if (line[i] == '"' or line[i] == '\'') {
                ParseQuotedValue(line, i, value, f, ln);
            } else {
                // if not quoted then everything until the end of line is value, if not whitespace
                while (i < line.size()) {
                    if (line[i] != ' ' and line[i] != '\t')
                        nameEnd = i;
                    ++i;
                }
                value = line.substr(valueStart, nameEnd - valueStart + 1);
            }
        }


    } // anonymous namespace


    void ParseINI(std::istream & s, Map & into) {
        Section * section = nullptr;
        std::string line;
        size_t ln = 0;
        while (std::getline(s, line)) {
            ++ln;
            if (line.empty()) // empty lines are ignored
                continue;
            if (line[0] == ';' or line[0] == '#') // comments are ignored
                continue;
            if (line[0] == '[') { // parse section name
                line = ParseINISectionName(line);
                Value & v = into[line];
                section = dynamic_cast<Section *>(& into[line]);
                ASSERT_(section != nullptr) << "INI sections must map to Section classes";
            } else {
                std::string argName;
                std::string argValue;
                ParseINILine(line, argName, argValue, s, ln);
                if (section == nullptr)
                    throw std::invalid_argument(STR("No section specified when setting value " << argName << " at line " << ln));
                Value & v = (*section)[argName];
                Variable * var = dynamic_cast<Variable *>(&v);
                if (var != nullptr) {
                    var->parseString(argValue);
                } else {
                    Array * a = dynamic_cast<Array *>(&v);
                    ASSERT_(a != nullptr) << "INI sections should only contain variables and arrays";
                    Variable *item = dynamic_cast<Variable *>(&a->append());
                    ASSERT_(item != nullptr) << "INI sections should only contain arrays of variables";
                    item->parseString(argValue);
                }
            }
        }
    }
} // namespace settings


