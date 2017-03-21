#pragma once

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "utils.h"

#if defined ASSERT
#define ASSERT_(cond) ASSERT(cond)
#elif defined assert
#define ASSERT_(cond) assert(cond); if (false) std::stringstream()
#else
#define ASSERT_(cond) if (false) std::stringstream()
#endif


namespace settings {
    class Map;
    class Command;
    class Settings;

    /** Parses the entire command line, using the first argument as a command name. 

    Remaining arguments are parsed as values of the given command. Returns the command selected. 
    */
    Command & ParseCommandLine(int argc, char * argv[], Settings & settings);
    
    void ParseCommandLineInto(int argc, char * argv[], Command & into, int fromIndex = 1);

    void ParseINI(std::istream & s, Map & into);

    // first define some primitives for aggregating and naming variables and their collections

    class Value {
    public:
        /** Writes the value contents to the given stream. 

        This should be redefined in 
        */
        virtual void writeTo(std::ostream & s) const = 0;
        virtual ~Value() {}
    };
    
    class Variable : public Value {
    public:
        virtual void parseString(std::string const & str) = 0;
    };

    /** Variable which automatically converts to specified type. 
    */
    template<typename T>
    class TypedVariable : public Variable {
    public:

        TypedVariable() = default;

        TypedVariable(T const & defaultValue) :
            value_(defaultValue) {
        }

        operator T const & () const {
            return value_;
        }

        void parseString(std::string const & str) override {
            value_ = ParseString(str);
        }

        void writeTo(std::ostream & s) const override {
            s << value_;
        }

    private:

        static T ParseString(std::string const & str);

        T value_;
    };

    typedef TypedVariable<std::string> String;
    typedef TypedVariable<bool> Bool;
    typedef TypedVariable<int> Int;
    typedef TypedVariable<double> Double;


    template<>
    inline std::string TypedVariable<std::string>::ParseString(std::string const & str) {
        return str;
    }

    template<>
    inline bool TypedVariable<bool>::ParseString(std::string const & str) {
        throw "";
    }

    template<>
    inline int TypedVariable<int>::ParseString(std::string const & str) {
        throw "";
    }

    template<>
    inline double TypedVariable<double>::ParseString(std::string const & str) {
        throw "";
    }



    /** Array of values. 

    */
    class Array : public Value {
    public:
        // TODO add iterator

        size_t size() const {
            return items_.size();
        }

        /** Adds new value to the array. 
        */
        virtual Value & append() = 0;

        Value & operator [] (size_t index) {
            ASSERT_(index <= items_.size()) << "Array index out of bounds, index " << index << ", size " << items_.size();
            return * (items_[index]);
        }

        /** Array owns its values. 
        */
        ~Array() override {
            for (Value * v : items_)
                delete v;
        }

        void writeTo(std::ostream & s) const override {
            s << '[';
            auto i = items_.begin();
            if (i != items_.end()) {
                (*i)->writeTo(s);
                ++i;
                while (i != items_.end()) {
                    s << ",";
                    (*i)->writeTo(s);
                    ++i;
                }
            }
            s << "]";
        }


    protected:
        void push_back(Value * v) {
            items_.push_back(v);
        }

        std::vector<Value *> items_;

    };

    /** Array of values of given type. 
     */
    template<typename T>
    class TypedArray : public Array {
    public:
        Value & append() {
            Value * v = new TypedVariable<T>();
            push_back(v);
            return *v;
        }
    };


    /** Default map implementation
     */

    class Map : public Value {
    public:
        // TODO Add iterator

        size_t size() const {
            return items_.size();
        }

        bool hasItem(std::string const & name) const {
            return getItem(name) != nullptr;
        }

        /** Returns the value associated with given name.
        
        If the map supports adding new elements, calling the `[]` operator with new name should return new Value, otherwise the std::invalid_argument exception should be thrown.
        */
        virtual Value & operator [] (std::string const & name) = 0;

        /** Map owns its values. 
         */
        ~Map() override {
            for (auto i : items_)
                delete i.second;
        }

        void writeTo(std::ostream & s) const override {
            s << "{";
            auto i = items_.begin();
            if (i != items_.end()) {
                s << '"' << i->first << "\":";
                i->second->writeTo(s);
                ++i;
                while (i != items_.end()) {
                    s << '"' << i->first << "\":";
                    i->second->writeTo(s);
                    ++i;
                }
            }
            s << "}";
        }


    protected:
        void addItem(Value * item, std::string name) {
            ASSERT_(getItem(name) == nullptr) << "Map cannot have two items with identical name " << name;
            items_.insert(std::pair<std::string, Value*>(name, item));
        }

        Value * getItem(std::string name) const {
            auto i = items_.find(name);
            if (i == items_.end())
                return nullptr;
            return i->second;
        }

        std::unordered_map<std::string, Value *> items_;
    };


    /** Section in the settings section. 

    A section is a collection of values. Sections are static, the `[]` operator throws std::invalid_argument exception if an unknown value is requested, therefore all values must be registered with the section. 

    Upon registration, each value must be given a name and can be given a decription. 
     */
    class Section : public Map {
    public:
        Section() = default;

        Value & operator [] (std::string const & name) {
            Value * v = getItem(name);
            if (v == nullptr)
                throw std::invalid_argument(STR("Option " << name << " does not exist"));
            return *v;
        }

        /** Section does not own its items as they must be registered. 
         */
        ~Section() override {
            // so that they won't be deleted by Map's destructor
            items_.clear();
        }

        std::string const & description(Value const & value) const {
            auto i = itemDescriptions_.find(&value);
            ASSERT_(i != itemDescriptions_.end()) << "Value not registered in section";
            return i->second;
        }

        void outputAsINI(std::ostream & s, bool includeDescriptions = true) {
//            if (includeDescriptions and not description_.empty())
//                s << "; " << description_ << std::endl;
//            s << "[" << name_ << "]" << std::endl;
            for (auto i : items_) {
                Value * v = i.second;
                if (includeDescriptions) {
                    std::string const & desc = itemDescriptions_[v];
                    if (not desc.empty()) 
                        s << "; " << desc << std::endl; 
                }
                Variable * var = dynamic_cast<Variable*>(v);
                if (var != nullptr) {
                    s << i.first << " = ";
                    var->writeTo(s);
                    s << std::endl;

                } else {
                    Array * a = dynamic_cast<Array*>(v);
                    ASSERT_(a != nullptr) << "Section can only have variables and arrays";
                    for (size_t ii = 0, e = a->size(); ii != e; ++ii) {
                        s << i.first << " = ";
                        (*a)[ii].writeTo(s);
                        s << std::endl;
                    }
                }
            }
        }

    protected:
        void registerValue(Variable & value, std::string const & name, std::string const & description = "") {
            registerValue_(value, name, description);
        }

        void registerValue(Array & value, std::string const & name, std::string const & description = "") {
            registerValue_(value, name, description);
        }

    private:
        // so that command can access the registerValue_
        friend class Command;

        void registerValue_(Value & value, std::string const & name, std::string const & description) {
            addItem(&value, name);
            itemDescriptions_.insert(std::make_pair(&value, description));
        }


        std::unordered_map<Value const *, std::string> itemDescriptions_;
    };

    /** Command represents a type of command line arguments. 

    On top of section's functionality, command may also define a default value, which is the default target of unknown value names. A command also remembers for each of its values, whether they are optional or not, and if they were supplied during the parsing, or not. 

    Additionally, several aliases may be used for command line arguments, i.e. (--verbose, -v, verbose). 
    */
    class Command : public Section {
    public:
        enum class AllowRedefinition {
            Yes, 
            Warning,
            No
        };

        Command() :
            defaultValue_(nullptr) {
        }

        Value & defaultValue() {
            if (defaultValue_ == nullptr)
                throw std::invalid_argument("No default value provided for section");
            return * defaultValue_;
        }

        bool supplied(Value const & v) const {
            ASSERT_(hasValue(v)) << "Value not part of command";
            auto i = extras_.find(&v);
            if (i == extras_.end())
                return false;
            else return i->second.supplied;
        }

        bool optional(Value const & v) const {
            ASSERT_(hasValue(v)) << "Value not part of command";
            auto i = extras_.find(&v);
            if (i == extras_.end())
                return ValueExtras().optional; // so that default value gets returned
            else return i->second.optional;
        }

        AllowRedefinition allowRedefinition(Value const & v) {
            ASSERT_(hasValue(v)) << "Value not part of command";
            auto i = extras_.find(&v);
            if (i == extras_.end())
                return ValueExtras().allowRedefinition; // so that default value gets returned
            else return i->second.allowRedefinition;
        }

        void markAsSupplied(Value const & v) {
            extras_[&v].supplied = true;
        }

        /** Commands are inteded to be singletons. 
        */
        bool operator == (Command const & other) const {
            return this == &other;
        }

    protected:
        void registerValue(Variable & value, std::initializer_list<std::string> names, std::string const & description = "", bool optional = true, AllowRedefinition allowRedefinition = AllowRedefinition::Warning) {
            registerValue_(value, names, description, optional, allowRedefinition);
        }

        void registerValue(Array & value, std::initializer_list<std::string> names, std::string const & description = "", bool optional = true, AllowRedefinition allowRedefinition = AllowRedefinition::Warning) {
            registerValue_(value, names, description, optional, allowRedefinition);
        }

        void setDefaultValue(Value & v) {
            ASSERT_(hasValue(v)) << "Default value must be registered";
            defaultValue_ = &v;
        }

    private:

        void registerValue_(Value & value, std::initializer_list<std::string> names, std::string const & description, bool optional, AllowRedefinition allowRedefinition) {
            ASSERT_(names.size() > 0);
            std::string const & name = *names.begin();
            Section::registerValue_(value, name, description);
            extras_.insert(std::make_pair(&value, ValueExtras(optional, allowRedefinition)));
            // now add aliases
            auto i = names.begin();
            for (++i; i != names.end(); ++i)
                addItem(&value, *i);
        }


        /** A helper method that checks that given value has been registered. 
         */
        bool hasValue(Value const & v) const {
            if (extras_.find(&v) != extras_.end())
                return true;
            for (auto i : items_)
                if (i.second == & v)
                    return true;
            return false;
        }

        struct ValueExtras {
            bool supplied;
            bool optional;
            AllowRedefinition allowRedefinition;

            ValueExtras() :
                supplied(false),
                optional(true),
                allowRedefinition(AllowRedefinition::Warning) {
            }
            ValueExtras(bool optional, AllowRedefinition allowRedefinition) :
                supplied(false),
                optional(optional),
                allowRedefinition(allowRedefinition) {
            }
        };

        Value * defaultValue_;

        std::unordered_map<Value const *, ValueExtras> extras_;
    };

    class Settings : public Map {
    public:
        Section & operator [] (std::string const & name) {
            Section * v = dynamic_cast<Section *>(getItem(name));
            if (v == nullptr)
                throw std::invalid_argument(STR("Section " << name << " does not exist"));
            return *v;
        }
        
        void outputAsINI(std::ostream & s, bool includeDescriptions = true) {
            for (auto i : items_) {
                Section * section = dynamic_cast<Section *>(i.second);
                std::string const & desc = itemDescriptions_[section];
                if (includeDescriptions and not desc.empty())
                    s << "; " << desc << std::endl;
                s << "[" << i.first << "]" << std::endl;
                section->outputAsINI(s);
                s << std::endl;
            }
        }

        /** Settings does not own the registered sections. 
        */
        ~Settings() override {
            // so that they won't be deleted by Map's destructor
            items_.clear();
        }


    protected:

        void registerSection(Section & section, std::string const & name, std::string const & description = "") {
            addItem(&section, name);
            itemDescriptions_.insert(std::make_pair(&section, description));
        }

    private:

        std::unordered_map<Value const *, std::string> itemDescriptions_;

    };


}