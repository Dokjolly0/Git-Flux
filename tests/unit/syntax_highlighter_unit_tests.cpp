#include <catch2/catch_test_macros.hpp>

#include "core/modules/syntax/language_detector.hpp"
#include "core/modules/syntax/syntax_highlighter.hpp"

using namespace gf::core::syntax;

TEST_CASE("LanguageDetector rileva correttamente il linguaggio in base all'estensione e al nome file", "[unit][syntax][detector]") {
    SECTION("Rilevamento da estensione") {
        REQUIRE(LanguageDetector::detect("main.cpp").language == Language::Cpp);
        REQUIRE(LanguageDetector::detect("header.hpp").language == Language::Cpp);
        REQUIRE(LanguageDetector::detect("source.c").language == Language::C);
        REQUIRE(LanguageDetector::detect("script.py").language == Language::Python);
        REQUIRE(LanguageDetector::detect("index.ts").language == Language::TypeScript);
        REQUIRE(LanguageDetector::detect("component.tsx").language == Language::TypeScript);
        REQUIRE(LanguageDetector::detect("app.js").language == Language::JavaScript);
        REQUIRE(LanguageDetector::detect("ui.jsx").language == Language::JavaScript);
        REQUIRE(LanguageDetector::detect("main.rs").language == Language::Rust);
        REQUIRE(LanguageDetector::detect("server.go").language == Language::Go);
        REQUIRE(LanguageDetector::detect("App.java").language == Language::Java);
        REQUIRE(LanguageDetector::detect("Program.cs").language == Language::CSharp);
        REQUIRE(LanguageDetector::detect("index.html").language == Language::Html);
        REQUIRE(LanguageDetector::detect("styles.css").language == Language::Css);
        REQUIRE(LanguageDetector::detect("package.json").language == Language::Json);
        REQUIRE(LanguageDetector::detect("config.yaml").language == Language::Yaml);
        REQUIRE(LanguageDetector::detect("config.yml").language == Language::Yaml);
        REQUIRE(LanguageDetector::detect("query.sql").language == Language::Sql);
        REQUIRE(LanguageDetector::detect("script.sh").language == Language::Shell);
        REQUIRE(LanguageDetector::detect("script.bash").language == Language::Shell);
        REQUIRE(LanguageDetector::detect("View.qml").language == Language::Qml);
        REQUIRE(LanguageDetector::detect("README.md").language == Language::Markdown);
        REQUIRE(LanguageDetector::detect("script.rb").language == Language::Ruby);
        REQUIRE(LanguageDetector::detect("index.php").language == Language::Php);
        REQUIRE(LanguageDetector::detect("manifest.xml").language == Language::Xml);
        REQUIRE(LanguageDetector::detect("script.ps1").language == Language::PowerShell);
    }

    SECTION("Rilevamento da nomi file speciali") {
        REQUIRE(LanguageDetector::detect("CMakeLists.txt").language == Language::CMake);
        REQUIRE(LanguageDetector::detect("module.cmake").language == Language::CMake);
        REQUIRE(LanguageDetector::detect("Makefile").language == Language::Makefile);
        REQUIRE(LanguageDetector::detect("makefile").language == Language::Makefile);
        REQUIRE(LanguageDetector::detect("Dockerfile").language == Language::Docker);
        REQUIRE(LanguageDetector::detect("Dockerfile.dev").language == Language::Docker);
        REQUIRE(LanguageDetector::detect(".gitignore").language == Language::Git);
        REQUIRE(LanguageDetector::detect(".gitmodules").language == Language::Git);
    }

    SECTION("Path complessi con cartelle") {
        REQUIRE(LanguageDetector::detect("src/desktop/ui/qml/views/Main.qml").language == Language::Qml);
        REQUIRE(LanguageDetector::detect("/home/user/project/build/CMakeLists.txt").language == Language::CMake);
        REQUIRE(LanguageDetector::detect("nested/dir/app.py").language == Language::Python);
    }

    SECTION("File non riconosciuti o senza estensione") {
        auto unknown = LanguageDetector::detect("some_unknown_binary_file.xyz123");
        REQUIRE(unknown.language == Language::Unknown);
        REQUIRE(unknown.name == "Plain Text");

        auto empty = LanguageDetector::detect("");
        REQUIRE(empty.language == Language::Unknown);
    }

    SECTION("Verifica metadati icona e nome") {
        auto cppInfo = LanguageDetector::detect("main.cpp");
        REQUIRE(cppInfo.name == "C++");
        REQUIRE_FALSE(cppInfo.icon.empty());

        auto pyInfo = LanguageDetector::detect("main.py");
        REQUIRE(pyInfo.name == "Python");
        REQUIRE_FALSE(pyInfo.icon.empty());
    }
}

TEST_CASE("SyntaxHighlighter tokenizza correttamente i vari costrutti di codice", "[unit][syntax][tokenizer]") {
    SECTION("C++ Keywords, Tipi, Funzioni, Variabili e Commenti") {
        std::string line = "    return calculateTotal(int count); // helper";
        auto tokens = SyntaxHighlighter::tokenize(line, Language::Cpp);

        bool foundKeyword = false;
        bool foundFunc = false;
        bool foundType = false;
        bool foundVar = false;
        bool foundComment = false;

        for (const auto& t : tokens) {
            if (t.type == TokenType::Keyword && t.text == "return") foundKeyword = true;
            if (t.type == TokenType::Function && t.text == "calculateTotal") foundFunc = true;
            if (t.type == TokenType::Type && t.text == "int") foundType = true;
            if (t.type == TokenType::Variable && t.text == "count") foundVar = true;
            if (t.type == TokenType::Comment && t.text.find("// helper") != std::string::npos) foundComment = true;
        }

        REQUIRE(foundKeyword);
        REQUIRE(foundFunc);
        REQUIRE(foundType);
        REQUIRE(foundVar);
        REQUIRE(foundComment);
    }

    SECTION("C++ Classi PascalCase, member variables e costanti ALL_CAPS") {
        std::string line = "class SessionManager { int m_activeCount = MAX_LIMIT; };";
        auto tokens = SyntaxHighlighter::tokenize(line, Language::Cpp);

        bool foundClassDecl = false;
        bool foundClassName = false;
        bool foundMemberVar = false;
        bool foundConstant = false;

        for (const auto& t : tokens) {
            if (t.type == TokenType::Keyword && t.text == "class") foundClassDecl = true;
            if (t.type == TokenType::Type && t.text == "SessionManager") foundClassName = true;
            if (t.type == TokenType::Variable && t.text == "m_activeCount") foundMemberVar = true;
            if (t.type == TokenType::Constant && t.text == "MAX_LIMIT") foundConstant = true;
        }

        REQUIRE(foundClassDecl);
        REQUIRE(foundClassName);
        REQUIRE(foundMemberVar);
        REQUIRE(foundConstant);
    }

    SECTION("Numeri esadecimali, decimali e float") {
        std::string line = "0xFF 12345 3.14159f";
        auto tokens = SyntaxHighlighter::tokenize(line, Language::Cpp);

        int numCount = 0;
        for (const auto& t : tokens) {
            if (t.type == TokenType::Number) ++numCount;
        }
        REQUIRE(numCount == 3);
    }

    SECTION("Stringhe con apici doppi e singoli") {
        std::string line = R"(std::string msg = "Hello, World!";)";
        auto tokens = SyntaxHighlighter::tokenize(line, Language::Cpp);

        bool foundStr = false;
        for (const auto& t : tokens) {
            if (t.type == TokenType::String && t.text == "\"Hello, World!\"") foundStr = true;
        }
        REQUIRE(foundStr);
    }

    SECTION("Proprietà con operatore punto e freccia") {
        std::string line = "user.profile.age = ptr->weight;";
        auto tokens = SyntaxHighlighter::tokenize(line, Language::Cpp);

        bool foundProfile = false;
        bool foundAge = false;
        bool foundWeight = false;

        for (const auto& t : tokens) {
            if (t.type == TokenType::Property && t.text == "profile") foundProfile = true;
            if (t.type == TokenType::Property && t.text == "age") foundAge = true;
            if (t.type == TokenType::Property && t.text == "weight") foundWeight = true;
        }

        REQUIRE(foundProfile);
        REQUIRE(foundAge);
        REQUIRE(foundWeight);
    }

    SECTION("Direttive del preprocessore") {
        std::string line = "#include <vector>";
        auto tokens = SyntaxHighlighter::tokenize(line, Language::Cpp);

        REQUIRE_FALSE(tokens.empty());
        REQUIRE(tokens[0].type == TokenType::Preprocessor);
    }

    SECTION("Python commento # e def di funzione") {
        std::string line = "def process_data(item): # commento";
        auto tokens = SyntaxHighlighter::tokenize(line, Language::Python);

        bool foundDef = false;
        bool foundFunc = false;
        bool foundComment = false;

        for (const auto& t : tokens) {
            if (t.type == TokenType::Keyword && t.text == "def") foundDef = true;
            if (t.type == TokenType::Function && t.text == "process_data") foundFunc = true;
            if (t.type == TokenType::Comment) foundComment = true;
        }

        REQUIRE(foundDef);
        REQUIRE(foundFunc);
        REQUIRE(foundComment);
    }

    SECTION("SQL query tokenization") {
        std::string line = "SELECT id, name FROM users WHERE active = 1;";
        auto tokens = SyntaxHighlighter::tokenize(line, Language::Sql);

        bool foundSelect = false;
        bool foundFrom = false;
        bool foundWhere = false;

        for (const auto& t : tokens) {
            if (t.type == TokenType::Keyword && t.text == "SELECT") foundSelect = true;
            if (t.type == TokenType::Keyword && t.text == "FROM") foundFrom = true;
            if (t.type == TokenType::Keyword && t.text == "WHERE") foundWhere = true;
        }

        REQUIRE(foundSelect);
        REQUIRE(foundFrom);
        REQUIRE(foundWhere);
    }
}

TEST_CASE("SyntaxHighlighter genera HTML formattato con colori e preserva l'indentazione", "[unit][syntax][html]") {
    SECTION("Escape HTML di caratteri speciali e spazi") {
        REQUIRE(SyntaxHighlighter::escapeHtml("<foo & bar>") == "&lt;foo&nbsp;&amp;&nbsp;bar&gt;");
        REQUIRE(SyntaxHighlighter::escapeHtml("    text") == "&nbsp;&nbsp;&nbsp;&nbsp;text");
        REQUIRE(SyntaxHighlighter::escapeHtml("\tcode") == "&nbsp;&nbsp;&nbsp;&nbsp;code");
    }

    SECTION("highlightLine per keyword produce span colorato") {
        std::string highlighted = SyntaxHighlighter::highlightLine("class MyClass {", Language::Cpp);

        REQUIRE(highlighted.find("<span style=\"color:#C678DD;font-weight:bold;\">class</span>") != std::string::npos);
        REQUIRE(highlighted.find("<span style=\"color:#E5C07B;font-weight:600;\">MyClass</span>") != std::string::npos);
    }

    SECTION("highlightLine per stringhe e numeri") {
        std::string highlighted = SyntaxHighlighter::highlightLine("int x = 42;", Language::Cpp);

        REQUIRE(highlighted.find("<span style=\"color:#D19A66;\">42</span>") != std::string::npos);
    }

    SECTION("highlightLine per commenti include font-style italic") {
        std::string highlighted = SyntaxHighlighter::highlightLine("// this is a comment", Language::Cpp);

        REQUIRE(highlighted.find("font-style:italic") != std::string::npos);
        REQUIRE(highlighted.find("#7F848E") != std::string::npos);
    }

    SECTION("highlightLine per riga vuota") {
        REQUIRE(SyntaxHighlighter::highlightLine("", Language::Cpp).empty());
    }
}
