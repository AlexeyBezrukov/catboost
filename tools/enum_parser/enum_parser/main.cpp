#include <library/json/writer/json_value.h>
#include <library/json/writer/json.h>
#include <library/getopt/small/last_getopt.h>

#include <tools/enum_parser/parse_enum/parse_enum.h>

#include <util/stream/file.h>
#include <util/stream/output.h>
#include <util/stream/input.h>
#include <util/stream/mem.h>

#include <util/charset/wide.h>
#include <util/string/builder.h>
#include <util/string/strip.h>
#include <util/string/cast.h>
#include <util/string/join.h>
#include <util/string/subst.h>
#include <util/generic/map.h>
#include <util/generic/string.h>
#include <util/generic/vector.h>
#include <util/generic/ptr.h>
#include <util/generic/yexception.h>
#include <util/generic/maybe.h>
#include <util/system/fs.h>
#include <util/folder/path.h>

void WriteHeader(const TString& headerName, IOutputStream& out, IOutputStream* headerOutPtr = nullptr) {
    out << "// This file was auto-generated. Do not edit!!!\n";
    out << "#include " << headerName << "\n";
    out << "#include <tools/enum_parser/enum_serialization_runtime/enum_runtime.h>\n\n";
    out << "#include <util/generic/typetraits.h>\n";
    out << "#include <util/generic/singleton.h>\n";
    out << "#include <util/generic/string.h>\n";
    out << "#include <util/generic/vector.h>\n";
    out << "#include <util/generic/map.h>\n";
    out << "#include <util/generic/serialized_enum.h>\n";
    out << "#include <util/string/cast.h>\n";
    out << "#include <util/stream/output.h>\n\n";
    out << "#include <array>\n";
    out << "#include <initializer_list>\n";
    out << "#include <utility>\n\n";

    if (headerOutPtr) {
        auto& outHeader = *headerOutPtr;
        outHeader << "// This file was auto-generated. Do not edit!!!\n";
        outHeader << "#pragma once\n\n";
        outHeader << "#include <util/generic/serialized_enum.h>\n";
        outHeader << "#include " << headerName << "\n";
    }
}

static inline void JsonEscape(TString& s) {
    SubstGlobal(s, "\\", "\\\\");
    SubstGlobal(s, "\"", "\\\"");
    SubstGlobal(s, "\r", "\\r");
    SubstGlobal(s, "\n", "\\n");
    SubstGlobal(s, "\t", "\\t");
}

static inline TString JsonQuote(const TString& s) {
    TString quoted = s;
    JsonEscape(quoted);
    return "\"" + quoted + "\""; // do not use .Quote() here, it performs escaping!
}


/// Simplifed JSON map encoder for generic types
template<typename T>
void OutKey(IOutputStream& out, const TString& key, const T& value, bool escape = true) {
    TString quoted = ToString(value);
    if (escape) {
        quoted = JsonQuote(quoted);
    }
    out << "\"" << key << "\": " << quoted << ",\n";
}

/// Simplifed JSON map encoder for TMaybe
void OutKey(IOutputStream& out, const TString& key, const TMaybe<TString>& value) {
    TString quoted;
    if (value) {
        quoted = JsonQuote(ToString(*value));
    } else {
        quoted = "null";
    }
    out << "\"" << key << "\": " << quoted << ",\n";
}


/// Simplifed JSON map encoder for bool values
void OutKey(IOutputStream& out, const TString& key, const bool& value) {
    out << "\"" << key << "\": " << (value ? "true" : "false") << ",\n";
}


/// Simplifed JSON map encoder for array items
template<typename T>
void OutItem(IOutputStream& out, const T& value, bool escape = true) {
    TString quoted = ToString(value);
    if (escape) {
        quoted = JsonQuote(quoted);
    }
    out << quoted << ",\n";
}

/// Cut trailing ",\n" or ","
static inline void FinishItems(TStringStream& out) {
    TString& s = out.Str();
    if (s.EndsWith(",\n")) {
        s.remove(s.size() - 2, 2);
    }
    if (s.EndsWith(",")) {
        s.pop_back();
    }
}


static inline void OpenMap(TStringStream& out) {
    out << "{\n";
}

static inline void CloseMap(TStringStream& out) {
    out << "}\n";
}

static inline void OpenArray(TStringStream& out) {
    out << "[\n";
}

static inline void CloseArray(TStringStream& out) {
    out << "]\n";
}

static TString WrapStringBuf(const TStringBuf str) {
    return TString::Join("AsStringBuf(\"", str, "\")");
}

void GenerateEnum(
    const TEnumParser::TEnum& en,
    IOutputStream& out,
    IOutputStream* jsonEnumOut = nullptr,
    IOutputStream* headerOutPtr = nullptr
) {
    TStringStream jEnum;
    OpenMap(jEnum);

    size_t count = en.Items.size();
    OutKey(jEnum, "count", count);
    const TString name = TEnumParser::ScopeStr(en.Scope) + en.CppName;
    OutKey(jEnum, "full_name", name);
    OutKey(jEnum, "cpp_name", en.CppName);
    TStringStream scopeJson;
    OpenArray(scopeJson);
    for (const auto& scopeItem : en.Scope) {
        OutItem(scopeJson, scopeItem);
    }
    FinishItems(scopeJson);
    CloseArray(scopeJson);

    OutKey(jEnum, "scope", scopeJson.Str(), false);
    OutKey(jEnum, "enum_class", en.EnumClass);

    TEnumParser::TScope outerScope = en.Scope;
    if (en.EnumClass) {
        outerScope.push_back(en.CppName);
    }

    TString outerScopeStr = TEnumParser::ScopeStr(outerScope);

    TString cName = name;
    SubstGlobal(cName, "::", "");

    out << "// I/O for " << name << "\n";

    TString nsName = "N" + cName + "Private";

    out << "namespace { namespace " << nsName << " {\n";

    TVector<TString> nameInitializerPairs;
    TVector<TString> valueInitializerPairs;
    TVector<TString> cppNamesInitializer;

    TStringStream jItems;
    OpenArray(jItems);

    for (const auto& it : en.Items) {
        TStringStream jEnumItem;
        OpenMap(jEnumItem);

        OutKey(jEnumItem, "cpp_name", it.CppName);
        OutKey(jEnumItem, "value", it.Value);
        OutKey(jEnumItem, "comment_text", it.CommentText);

        TStringStream jAliases;
        OpenArray(jAliases);

        TString strValue = it.CppName;
        if (it.Aliases) {
            // first alias is main
            strValue = it.Aliases[0];
            OutKey(jEnumItem, "str_value", strValue);
        }
        nameInitializerPairs.push_back("TNameBufs::EnumStringPair(" + outerScopeStr + it.CppName + ", " + WrapStringBuf(strValue) + ")");
        cppNamesInitializer.push_back(WrapStringBuf(it.CppName));

        for (const auto& alias : it.Aliases) {
            valueInitializerPairs.push_back("TNameBufs::EnumStringPair(" + outerScopeStr + it.CppName + ", " + WrapStringBuf(alias) + ")");
            OutItem(jAliases, alias);
        }
        FinishItems(jAliases);
        CloseArray(jAliases);

        if (!it.Aliases) {
            valueInitializerPairs.push_back("TNameBufs::EnumStringPair(" + outerScopeStr + it.CppName + ", " + WrapStringBuf(it.CppName) + ")");
        }
        OutKey(jEnumItem, "aliases", jAliases.Str(), false);

        FinishItems(jEnumItem);
        CloseMap(jEnumItem);

        OutItem(jItems, jEnumItem.Str(), false);
    }
    FinishItems(jItems);
    CloseArray(jItems);
    OutKey(jEnum, "items", jItems.Str(), false);

    auto defineConstArray = [&out, payloadCache = TMap<std::pair<TString, TVector<TString>>, TString>()](const TStringBuf indent, const TStringBuf elementType, const TStringBuf name, const TVector<TString>& items) mutable {
        if (items.empty()) { // ISO C++ forbids zero-size array
            out << indent << "static constexpr const TArrayRef<const " << elementType << "> " << name << ";\n";
        } else {
            // try to reuse one of the previous payload arrays
            const auto inserted = payloadCache.emplace(std::make_pair(elementType, items), ToString(name) + "_PAYLOAD");
            const TString& payloadStorageName = inserted.first->second;
            if (inserted.second) { // new array content or type
                out << indent << "static constexpr const " << elementType << " " << payloadStorageName << "[" << items.size() << "]{\n";
                for (const auto& it : items) {
                    out << indent << "    " << it << ",\n";
                }
                out << indent << "};\n";
            }
            out << indent << "static constexpr const TArrayRef<const " << elementType << "> " << name << "{" << payloadStorageName << "};\n";
        }
        out << "\n";
    };

    out << "    class TNameBufs : public ::NEnumSerializationRuntime::TEnumDescription<" << name << "> {\n";
    out << "    public:\n";
    out << "        using TBase = ::NEnumSerializationRuntime::TEnumDescription<" << name << ">;\n\n";
    out << "        inline TNameBufs();\n\n";

    // Instance
    out << "        static inline const TNameBufs& Instance() {\n";
    out << "            return *Singleton<TNameBufs>();\n";
    out << "        }\n";
    out << "    };\n\n";

    // Initialization data
    defineConstArray("    ", "TNameBufs::TBase::TEnumStringPair", "NAMES_INITIALIZATION_PAIRS", nameInitializerPairs);
    defineConstArray("    ", "TNameBufs::TBase::TEnumStringPair", "VALUES_INITIALIZATION_PAIRS", valueInitializerPairs);
    defineConstArray("    ", "TStringBuf", "CPP_NAMES_INITIALIZATION_ARRAY", cppNamesInitializer);

    out << "    static constexpr const TNameBufs::TInitializationData ENUM_INITIALIZATION_DATA{\n";
    out << "        NAMES_INITIALIZATION_PAIRS,\n";
    out << "        VALUES_INITIALIZATION_PAIRS,\n";
    out << "        CPP_NAMES_INITIALIZATION_ARRAY,\n";
    out << "        " << WrapStringBuf(outerScopeStr) << ",\n";
    out << "        " << WrapStringBuf(name) << "\n";
    out << "    };\n\n";

    // Constructor
    out << "    inline TNameBufs::TNameBufs()\n";
    out << "        : TBase(ENUM_INITIALIZATION_DATA)\n";
    out << "    {\n";
    out << "    }\n\n";

    out << "}}\n\n";

    if (headerOutPtr) {
        (*headerOutPtr) << "// I/O for " << name << "\n";
    }

    // outer ToString
    if (headerOutPtr) {
        (*headerOutPtr) << "const TString& ToString(" << name << ");\n";
    }
    out << "const TString& ToString(" << name << " x) {\n";
    out << "    const " << nsName << "::TNameBufs& names = " << nsName << "::TNameBufs::Instance();\n";
    out << "    return names.ToString(x);\n";
    out << "}\n\n";

    // outer FromString
    if (headerOutPtr) {
        (*headerOutPtr) << "bool FromString(const TString& name, " << name << "& ret);\n";
    }
    out << "bool FromString(const TString& name, " << name << "& ret) {\n";
    out << "    const " << nsName << "::TNameBufs& names = " << nsName << "::TNameBufs::Instance();\n";
    out << "    return names.FromString(name, ret);\n";
    out << "}\n\n";

    // outer FromString
    if (headerOutPtr) {
        (*headerOutPtr) << "bool FromString(const TStringBuf& name, " << name << "& ret);\n";
    }
    out << "bool FromString(const TStringBuf& name, " << name << "& ret) {\n";
    out << "    const " << nsName << "::TNameBufs& names = " << nsName << "::TNameBufs::Instance();\n";
    out << "    return names.FromString(name, ret);\n";
    out << "}\n\n";

    // specialization for internal FromStringImpl
    out << "template<>\n";
    out << name << " FromStringImpl<" << name << ">(const char* data, size_t len) {\n";
    out << "    const " << nsName << "::TNameBufs& names = " << nsName << "::TNameBufs::Instance();\n";
    out << "    return names.FromString(TStringBuf(data, len));\n";
    out << "}\n\n";

    // specialization for internal TryFromStringImpl
    out << "template<>\n";
    out << "bool TryFromStringImpl<" << name << ">(const char* data, size_t len, " << name << "& result) {\n";
    out << "    return FromString(TStringBuf(data, len), result);\n";
    out << "}\n\n";

    // outer Out
    out << "template<>\n";
    out << "void Out<" << name << ">(IOutputStream& os, TTypeTraits<" << name << ">::TFuncParam n) {\n";
    out << "    const " << nsName << "::TNameBufs& names = " << nsName << "::TNameBufs::Instance();\n";
    out << "    return names.Out(&os, n);\n";
    out << "}\n\n";

    // specializations for NEnumSerializationRuntime function family
    out << "namespace NEnumSerializationRuntime {\n";
    // template<> GetEnumAllValues
    out << "    template<>\n";
    out << "    TMappedArrayView<" << name <<"> GetEnumAllValuesImpl<" << name << ">() {\n";
    out << "        const " << nsName << "::TNameBufs& names = " << nsName << "::TNameBufs::Instance();\n";
    out << "        return names.AllEnumValues();\n";
    out << "    }\n\n";

    // template<> GetEnumAllNames
    out << "    template<>\n";
    out << "    const TString& GetEnumAllNamesImpl<" << name << ">() {\n";
    out << "        const " << nsName << "::TNameBufs& names = " << nsName << "::TNameBufs::Instance();\n";
    out << "        return names.AllEnumNames();\n";
    out << "    }\n\n";

    // template<> GetEnumNames<EnumType>
    out << "    template<>\n";
    out << "    TMappedDictView<" << name << ", TString> GetEnumNamesImpl<" << name << ">() {\n";
    out << "        const " << nsName << "::TNameBufs& names = " << nsName << "::TNameBufs::Instance();\n";
    out << "        return names.EnumNames();\n";
    out << "    }\n\n";

    // template<> GetEnumAllCppNames, see IGNIETFERRO-534
    out << "    template<>\n";
    out << "    const TVector<TString>& GetEnumAllCppNamesImpl<" << name << ">() {\n";
    out << "        const " << nsName << "::TNameBufs& names = " << nsName << "::TNameBufs::Instance();\n";
    out << "        return names.AllEnumCppNames();\n";
    out << "    }\n";

    out << "}\n\n";

    if (headerOutPtr) {
        // <EnumType>Count
        auto& outHeader = *headerOutPtr;
        outHeader << "template <>\n";
        outHeader << "constexpr size_t GetEnumItemsCount<" << name << ">() {\n";
        outHeader << "    return " << en.Items.size() << ";\n";
        outHeader << "}\n";
    }

    FinishItems(jEnum);
    jEnum << "}\n";

    if (jsonEnumOut) {
        *jsonEnumOut << jEnum.Str();
    }
}

int main(int argc, char** argv) {
    try {
        using namespace NLastGetopt;
        TOpts opts = NLastGetopt::TOpts::Default();
        opts.AddHelpOption();

        TString outputFileName;
        TString outputHeaderFileName;
        TString outputJsonFileName;
        TString includePath;
        opts.AddLongOption('o', "output").OptionalArgument("<output-file>").StoreResult(&outputFileName)
            .Help(
                "Output generated code to specified file.\n"
                "When not set, standard output is used."
            );
        opts.AddLongOption('h', "header").OptionalArgument("<output-header>").StoreResult(&outputHeaderFileName)
            .Help(
                "Generate appropriate header to specified file.\n"
                "Works only if output file specified."
            );
        opts.AddLongOption("include-path").OptionalArgument("<header-path>").StoreResult(&includePath)
            .Help(
                "Include input header using this path in angle brackets.\n"
                "When not set, header basename is used in double quotes."
            );

        opts.AddLongOption('j', "json-output").OptionalArgument("<json-output>").StoreResult(&outputJsonFileName)
            .Help(
                "Generate enum data in JSON format."
            );

        opts.SetFreeArgsNum(1);
        opts.SetFreeArgTitle(0, "<input-file>", "Input header file with enum declarations");

        TOptsParseResult res(&opts, argc, argv);

        TVector<TString> freeArgs = res.GetFreeArgs();
        TString inputFileName = freeArgs[0];

        THolder<IOutputStream> hOut;
        IOutputStream* out = &Cout;

        THolder<IOutputStream> headerOut;

        THolder<IOutputStream> jsonOut;


        if (outputFileName) {
            NFs::Remove(outputFileName);
            hOut.Reset(new TFileOutput(outputFileName));
            out = hOut.Get();

            if (outputHeaderFileName) {
                headerOut.Reset(new TFileOutput(outputHeaderFileName));
            }

            if (outputJsonFileName) {
                jsonOut.Reset(new TFileOutput(outputJsonFileName));
            }
        }

        if (!includePath) {
            includePath = TString() + '"' + TFsPath(inputFileName).Basename() + '"';
        } else {
            includePath = TString() + '<' + includePath + '>';
        }

        TEnumParser parser(inputFileName);
        WriteHeader(includePath, *out, headerOut.Get());

        TStringStream jEnums;
        OpenArray(jEnums);

        for (const auto& en : parser.Enums) {
            if (!en.CppName) {
                // skip unnamed enum declarations
                continue;
            }

            TStringStream jEnum;
            GenerateEnum(en, *out, &jEnum, headerOut.Get());
            OutItem(jEnums, jEnum.Str(), false);
        }
        FinishItems(jEnums);
        CloseArray(jEnums);

        if (jsonOut) {
            *jsonOut << jEnums.Str() << Endl;
        }

        return 0;
    } catch (...) {
        Cerr << CurrentExceptionMessage() << Endl;
    }

    return 1;
}
