#include <catch2/catch_test_macros.hpp>

#include "core/modules/syntax/language_detector.hpp"
#include "core/modules/syntax/syntax_highlighter.hpp"
#include "core/modules/syntax/markdown_parser.hpp"

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
        REQUIRE(LanguageDetector::detect("App.xaml").language == Language::Xaml);
        REQUIRE(LanguageDetector::detect("MainWindow.axaml").language == Language::Xaml);
        REQUIRE(LanguageDetector::detect("icon.svg").language == Language::Xml);
        REQUIRE(LanguageDetector::detect("App.csproj").language == Language::Xml);
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

    SECTION("XAML Tag, Attributi, Namespace, Stringhe e Testo") {
        std::string line = R"(    <Button x:Name="SubmitBtn" Click="OnClick" Margin="10,20">Click Me</Button>)";
        auto tokens = SyntaxHighlighter::tokenize(line, Language::Xaml);

        bool foundTag = false;
        bool foundNameAttr = false;
        bool foundClickAttr = false;
        bool foundStringVal = false;
        bool foundTextContent = false;

        for (const auto& t : tokens) {
            if (t.type == TokenType::Type && t.text == "Button") foundTag = true;
            if (t.type == TokenType::Property && t.text == "x:Name") foundNameAttr = true;
            if (t.type == TokenType::Property && t.text == "Click") foundClickAttr = true;
            if (t.type == TokenType::String && t.text == "\"SubmitBtn\"") foundStringVal = true;
            if (t.type == TokenType::PlainText && t.text == "Click Me") foundTextContent = true;
        }

        REQUIRE(foundTag);
        REQUIRE(foundNameAttr);
        REQUIRE(foundClickAttr);
        REQUIRE(foundStringVal);
        REQUIRE(foundTextContent);
    }

    SECTION("XML Direttiva, Commenti e Tag con attributi") {
        std::string xmlDecl = R"(<?xml version="1.0" encoding="utf-8"?>)";
        auto declTokens = SyntaxHighlighter::tokenize(xmlDecl, Language::Xml);
        REQUIRE_FALSE(declTokens.empty());
        REQUIRE(declTokens[0].type == TokenType::Preprocessor);

        std::string xmlComment = "<!-- Config settings -->";
        auto commentTokens = SyntaxHighlighter::tokenize(xmlComment, Language::Xml);
        REQUIRE_FALSE(commentTokens.empty());
        REQUIRE(commentTokens[0].type == TokenType::Comment);

        std::string xmlTag = R"(<User id="42" active="true" />)";
        auto tagTokens = SyntaxHighlighter::tokenize(xmlTag, Language::Xml);

        bool foundUser = false;
        bool foundId = false;
        bool foundActive = false;

        for (const auto& t : tagTokens) {
            if (t.type == TokenType::Type && t.text == "User") foundUser = true;
            if (t.type == TokenType::Property && t.text == "id") foundId = true;
            if (t.type == TokenType::Property && t.text == "active") foundActive = true;
        }

        REQUIRE(foundUser);
        REQUIRE(foundId);
        REQUIRE(foundActive);
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

    SECTION("highlightLine per XAML produce tag colorati e attributi") {
        std::string highlighted = SyntaxHighlighter::highlightLine(R"(<Grid Background="#1E1E1E">)", Language::Xaml);
        REQUIRE(highlighted.find("<span style=\"color:#E5C07B;font-weight:600;\">Grid</span>") != std::string::npos);
        REQUIRE(highlighted.find("Background") != std::string::npos);
        REQUIRE(highlighted.find("#98C379") != std::string::npos);
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


TEST_CASE("LanguageDetector rileva estensioni Markdown extra e code fence info strings", "[unit][syntax][detector][markdown]") {
    SECTION("Estensioni Markdown aggiuntive") {
        REQUIRE(LanguageDetector::detect("CHANGELOG.mdown").language == Language::Markdown);
        REQUIRE(LanguageDetector::detect("docs/guide.mkd").language == Language::Markdown);
        REQUIRE(LanguageDetector::detect("pages/intro.mdx").language == Language::Markdown);
        REQUIRE(LanguageDetector::detect("SPEC.markdown").language == Language::Markdown);
    }

    SECTION("Code fence info string verso Language") {
        REQUIRE(LanguageDetector::fromCodeFenceInfo("") == Language::Unknown);
        REQUIRE(LanguageDetector::fromCodeFenceInfo("text") == Language::Unknown);
        REQUIRE(LanguageDetector::fromCodeFenceInfo("TEXT") == Language::Unknown);
        REQUIRE(LanguageDetector::fromCodeFenceInfo("txt") == Language::Unknown);
        REQUIRE(LanguageDetector::fromCodeFenceInfo("plain") == Language::Unknown);
        REQUIRE(LanguageDetector::fromCodeFenceInfo("plaintext") == Language::Unknown);
        REQUIRE(LanguageDetector::fromCodeFenceInfo("ascii") == Language::Unknown);
        REQUIRE(LanguageDetector::fromCodeFenceInfo("tree") == Language::Unknown);
        REQUIRE(LanguageDetector::fromCodeFenceInfo("console") == Language::Unknown);
        REQUIRE(LanguageDetector::fromCodeFenceInfo("output") == Language::Unknown);
        REQUIRE(LanguageDetector::fromCodeFenceInfo("log") == Language::Unknown);
        REQUIRE(LanguageDetector::fromCodeFenceInfo("diff") == Language::Unknown);
        REQUIRE(LanguageDetector::fromCodeFenceInfo("mermaid") == Language::Unknown);

        REQUIRE(LanguageDetector::fromCodeFenceInfo("cpp") == Language::Cpp);
        REQUIRE(LanguageDetector::fromCodeFenceInfo("c++") == Language::Cpp);
        REQUIRE(LanguageDetector::fromCodeFenceInfo("go") == Language::Go);
        REQUIRE(LanguageDetector::fromCodeFenceInfo("python") == Language::Python);
        REQUIRE(LanguageDetector::fromCodeFenceInfo("yaml") == Language::Yaml);
        REQUIRE(LanguageDetector::fromCodeFenceInfo("json") == Language::Json);
        REQUIRE(LanguageDetector::fromCodeFenceInfo("bash") == Language::Shell);
        REQUIRE(LanguageDetector::fromCodeFenceInfo("bash title=\"setup\"") == Language::Shell);
        REQUIRE(LanguageDetector::fromCodeFenceInfo("ts") == Language::TypeScript);
        REQUIRE(LanguageDetector::fromCodeFenceInfo("rust") == Language::Rust);
        REQUIRE(LanguageDetector::fromCodeFenceInfo("sql") == Language::Sql);
        REQUIRE(LanguageDetector::fromCodeFenceInfo("qml") == Language::Qml);
    }
}

TEST_CASE("SyntaxHighlighter protegge UTF-8 box-drawing e Plain Text da evidenziazione errata", "[unit][syntax][utf8]") {
    SECTION("Albero file con box-drawing UTF-8 e commento # in Plain Text non viene frammentato ne evidenziato come codice") {
        const std::string line = "│   ├── tree/            # folders, risoluzione path, move, cestino";
        auto tokens = SyntaxHighlighter::tokenize(line, Language::Unknown);
        REQUIRE_FALSE(tokens.empty());

        std::string reconstructed;
        bool foundBoxDrawingGroup = false;
        for (const auto& t : tokens) {
            reconstructed.append(t.text);
            REQUIRE(t.type != TokenType::Comment);
            REQUIRE(t.type != TokenType::Keyword);
            REQUIRE(t.type != TokenType::Type);
            REQUIRE(t.type != TokenType::String);
            REQUIRE(t.type != TokenType::Function);
            REQUIRE(t.type != TokenType::Preprocessor);
            if (t.text.find("├──") != std::string::npos) {
                foundBoxDrawingGroup = true;
            }
        }
        REQUIRE(reconstructed == line);
        REQUIRE(foundBoxDrawingGroup);
    }

    SECTION("highlightCodeBlockHtml per blocco tree produce <pre> pulito e senza riga vuota finale") {
        const std::string tree =
            "file-service/\n"
            "├── cmd/\n"
            "│   └── main.go\n"
            "├── tree/   # folders\n"
            "└── README.md\n";
        std::string html = SyntaxHighlighter::highlightCodeBlockHtml(tree, Language::Unknown);

        REQUIRE(html.starts_with("<pre style=\"margin:0;white-space:pre;\">"));
        REQUIRE(html.ends_with("</pre>"));
        REQUIRE(html.find("├──") != std::string::npos);
        REQUIRE(html.find("└──") != std::string::npos);
        REQUIRE(html.find("#7F848E") == std::string::npos);
        REQUIRE(html.find("\n</pre>") == std::string::npos);
    }

    SECTION("Commento C++ con caratteri UTF-8 multi-byte preserva le sequenze UTF-8") {
        const std::string line = "// Albero dir: ├── già verificato (èòà)";
        auto tokens = SyntaxHighlighter::tokenize(line, Language::Cpp);
        REQUIRE(tokens.size() == 1);
        REQUIRE(tokens[0].type == TokenType::Comment);
        REQUIRE(tokens[0].text == line);
    }
}

TEST_CASE("SyntaxHighlighter tokenizza Markdown con stato multi-riga senza rompere la prosa", "[unit][syntax][markdown]") {
    SECTION("Apostrofi e URL in Markdown restano testo leggibile senza falsi token codice") {
        MarkdownLineState state{};
        const std::string line = "L'utente visita https://example.com/v1 e don't break \"quotes\" o `if`";
        auto tokens = SyntaxHighlighter::tokenizeMarkdownLine(line, state);

        std::string reconstructed;
        bool foundCode = false;
        bool foundLink = false;
        for (const auto& t : tokens) {
            reconstructed.append(t.text);
            REQUIRE(t.type != TokenType::Keyword);
            REQUIRE(t.type != TokenType::String);
            REQUIRE(t.type != TokenType::Comment);
            if (t.type == TokenType::MarkdownCode) foundCode = true;
            if (t.type == TokenType::MarkdownLink) foundLink = true;
        }
        REQUIRE(reconstructed == line);
        REQUIRE(foundCode);
        REQUIRE(foundLink);
        REQUIRE_FALSE(state.inCodeFence);
    }

    SECTION("computeMarkdownLineStates propaga fence Go multi-riga e si chiude sul fence finale") {
        const std::string doc =
            "# Titolo\n"
            "```go\n"
            "func main() {}\n"
            "```\n"
            "Prosa con import class return\n";

        auto states = SyntaxHighlighter::computeMarkdownLineStates(doc);
        REQUIRE(states.size() >= 5);
        REQUIRE_FALSE(states[0].inCodeFence);
        REQUIRE_FALSE(states[1].inCodeFence);
        REQUIRE(states[2].inCodeFence);
        REQUIRE(states[2].fenceLanguage == Language::Go);
        REQUIRE(states[3].inCodeFence);
        REQUIRE_FALSE(states[4].inCodeFence);

        MarkdownLineState insideState = states[2];
        std::string goLineHtml = SyntaxHighlighter::highlightLine("func main() {}", Language::Markdown, insideState);
        REQUIRE(goLineHtml.find("func") != std::string::npos);
        REQUIRE(goLineHtml.find("#C678DD") != std::string::npos);

        MarkdownLineState outsideState = states[4];
        auto proseTokens = SyntaxHighlighter::tokenizeMarkdownLine("Prosa con import class return", outsideState);
        for (const auto& t : proseTokens) {
            REQUIRE(t.type != TokenType::Keyword);
        }
    }
}

TEST_CASE("MarkdownParser estrae blocchi GitHub-style, TOC, badge, tabelle e Rich Diff", "[unit][syntax][markdown][parser]") {
    SECTION("toGithubSlug replica il comportamento GitHub con emoji, punteggiatura ed entita") {
        REQUIRE(MarkdownParser::toGithubSlug("Hello World") == "hello-world");
        REQUIRE(MarkdownParser::toGithubSlug("Architecture & Clean Design") == "architecture--clean-design");
        REQUIRE(MarkdownParser::toGithubSlug("Step 1: Setup & Run!") == "step-1-setup--run");
    }

    SECTION("Parsing completo con badge row, alert GitHub, code fence tree, lista e tabella") {
        const std::string md =
            "<div align=\"center\">\n"
            "\n"
            "[![Build](https://img.shields.io/badge/build-passing-brightgreen)](https://ci.example.com) [![Go](https://img.shields.io/badge/Go-1.22-00ADD8)](https://go.dev)\n"
            "\n"
            "</div>\n"
            "\n"
            "# Panoramica del Servizio\n"
            "\n"
            "> [!NOTE]\n"
            "> Nota importante sulla configurazione.\n"
            "\n"
            "## Panoramica del Servizio\n"
            "\n"
            "- [x] Completato `tree`\n"
            "- [ ] Da fare\n"
            "\n"
            "```text\n"
            "file-service/\n"
            "├── cmd/\n"
            "└── README.md\n"
            "```\n"
            "\n"
            "| Servizio | Stato | Porta |\n"
            "| :--- | :---: | ---: |\n"
            "| API | **Attivo** | `8080` |\n";

        auto parsed = MarkdownParser::parse(md);
        REQUIRE(parsed.toc.size() == 2);
        CHECK(parsed.toc[0].title == "Panoramica del Servizio");
        CHECK(parsed.toc[0].anchor == "panoramica-del-servizio");
        CHECK(parsed.toc[1].anchor == "panoramica-del-servizio-1");

        bool hasBadgeRow = false;
        bool hasAlert = false;
        bool hasTreeCode = false;
        bool hasTable = false;
        int taskCount = 0;

        for (const auto& blk : parsed.blocks) {
            if (blk.kind == MarkdownBlockKind::BadgeRow) {
                hasBadgeRow = true;
                CHECK(blk.centered);
                REQUIRE(blk.badges.size() == 2);
                CHECK(blk.badges[0].label == "Build");
                CHECK(blk.badges[0].color == "#238636");
                CHECK(blk.badges[0].href == "https://ci.example.com");
                CHECK(blk.badges[1].label == "Go");
            } else if (blk.kind == MarkdownBlockKind::Blockquote && blk.alertType == MarkdownAlertType::Note) {
                hasAlert = true;
                CHECK(blk.html.find("Nota importante") != std::string::npos);
            } else if (blk.kind == MarkdownBlockKind::ListItem && blk.isTask) {
                ++taskCount;
            } else if (blk.kind == MarkdownBlockKind::CodeBlock) {
                hasTreeCode = true;
                CHECK(blk.detectedLanguage == Language::Unknown);
                CHECK(blk.rawText.find("├── cmd/") != std::string::npos);
                CHECK(blk.html.find("├──") != std::string::npos);
                CHECK(blk.html.find("\n</pre>") == std::string::npos);
            } else if (blk.kind == MarkdownBlockKind::Table) {
                hasTable = true;
                REQUIRE(blk.tableHeaders.size() == 3);
                REQUIRE(blk.tableAlignments == std::vector<std::string>{"left", "center", "right"});
                REQUIRE(blk.tableRows.size() == 1);
            }
        }

        CHECK(hasBadgeRow);
        CHECK(hasAlert);
        CHECK(taskCount == 2);
        CHECK(hasTreeCode);
        CHECK(hasTable);
    }

    SECTION("parseWithDiff associa blocchi aggiunti, modificati e snippet eliminati") {
        const std::string newMd =
            "# Titolo\n"
            "\n"
            "Paragrafo modificato\n"
            "\n"
            "Nuova sezione finale\n";

        gf::git::bo::FileDiff diff;
        diff.oldPath = "README.md";
        diff.newPath = "README.md";
        gf::git::bo::DiffHunk hunk;
        hunk.header = "@@ -1,5 +1,5 @@";
        hunk.oldStartLine = 1;
        hunk.newStartLine = 1;
        hunk.lines = {
            {.kind = gf::git::bo::DiffLineKind::Context,  .oldLineNumber = 1, .newLineNumber = 1, .content = "# Titolo"},
            {.kind = gf::git::bo::DiffLineKind::Context,  .oldLineNumber = 2, .newLineNumber = 2, .content = ""},
            {.kind = gf::git::bo::DiffLineKind::Deletion, .oldLineNumber = 3, .newLineNumber = std::nullopt, .content = "Paragrafo originale"},
            {.kind = gf::git::bo::DiffLineKind::Addition, .oldLineNumber = std::nullopt, .newLineNumber = 3, .content = "Paragrafo modificato"},
            {.kind = gf::git::bo::DiffLineKind::Context,  .oldLineNumber = 4, .newLineNumber = 4, .content = ""},
            {.kind = gf::git::bo::DiffLineKind::Addition, .oldLineNumber = std::nullopt, .newLineNumber = 5, .content = "Nuova sezione finale"},
            {.kind = gf::git::bo::DiffLineKind::Deletion, .oldLineNumber = 5, .newLineNumber = std::nullopt, .content = "Riga rimossa in coda"}
        };
        diff.hunks.push_back(std::move(hunk));

        auto doc = MarkdownParser::parseWithDiff(newMd, &diff);
        REQUIRE(doc.blocks.size() >= 3);

        CHECK(doc.blocks[0].changeStatus == MarkdownChangeStatus::Unchanged);
        CHECK(doc.blocks[1].changeStatus == MarkdownChangeStatus::Modified);
        CHECK(doc.blocks[1].addedLineCount == 1);
        CHECK(doc.blocks[1].deletedLineCount == 1);
        REQUIRE(doc.blocks[1].deletedLines.size() == 1);
        CHECK(doc.blocks[1].deletedLines[0] == "Paragrafo originale");

        CHECK(doc.blocks[2].changeStatus == MarkdownChangeStatus::Added);

        bool foundEofDeletedSnippet = false;
        for (const auto& blk : doc.blocks) {
            if (blk.kind == MarkdownBlockKind::DeletedSnippet) {
                foundEofDeletedSnippet = true;
                REQUIRE(blk.deletedLines.size() == 1);
                CHECK(blk.deletedLines[0] == "Riga rimossa in coda");
            }
        }
        CHECK(foundEofDeletedSnippet);
    }

    SECTION("Blocco codice indentato a 4 spazi (file tree senza backtick)") {
        const std::string md =
            "# Struttura\n"
            "\n"
            "    file-service/\n"
            "    ├── cmd/\n"
            "    │   └── main.go\n"
            "    # commento file\n"
            "\n"
            "    - sottoelemento lista\n";

        auto doc = MarkdownParser::parse(md);
        REQUIRE(doc.toc.size() == 1);
        CHECK(doc.toc[0].title == "Struttura");

        bool foundIndentedTree = false;
        bool foundListItem = false;
        for (const auto& blk : doc.blocks) {
            if (blk.kind == MarkdownBlockKind::CodeBlock) {
                foundIndentedTree = true;
                CHECK(blk.detectedLanguage == Language::Unknown);
                CHECK(blk.rawText.find("├── cmd/") != std::string::npos);
                CHECK(blk.rawText.find("# commento file") != std::string::npos);
            } else if (blk.kind == MarkdownBlockKind::ListItem) {
                foundListItem = true;
            }
        }
        CHECK(foundIndentedTree);
        CHECK(foundListItem);
    }
}
