#include "desktop/bootstrap/app_bootstrap.hpp"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QQmlApplicationEngine>
#include <QQmlNetworkAccessManagerFactory>
#include <QQuickWindow>
#include <iostream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdio>
#include <initializer_list>
#include <string_view>

namespace {

void setupWindowsConsole(bool forceNewConsole = false) {
    bool attached = false;
    if (!forceNewConsole) {
        attached = (AttachConsole(ATTACH_PARENT_PROCESS) != 0);
    }
    if (!attached) {
        if (!AllocConsole()) {
            return;
        }
    }

    FILE* fp = nullptr;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONIN$", "r", stdin);

    std::cout.clear();
    std::cerr.clear();
    std::cin.clear();

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

bool hasCliFlag(int argc, char* argv[], std::initializer_list<std::string_view> flags) {
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        for (auto flag : flags) {
            if (arg == flag) {
                return true;
            }
        }
    }
    return false;
}

} // namespace
#endif

namespace {

class RedirectNetworkAccessManagerFactory final : public QQmlNetworkAccessManagerFactory {
public:
    QNetworkAccessManager* create(QObject* parent) override {
        auto* manager = new QNetworkAccessManager(parent);
        manager->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
        return manager;
    }
};

} // namespace

int main(int argc, char* argv[]) {
#ifdef _WIN32
    const bool wantsDebug = hasCliFlag(argc, argv, {"--debug", "-debug", "--console", "-console", "--verbose", "-v"});
    const bool wantsCliInfo = hasCliFlag(argc, argv, {"--help", "-h", "-?", "/?", "--version", "--test-startup"});

    if (wantsDebug) {
        setupWindowsConsole(false);
        qputenv("QT_ASSUME_STDERR_HAS_CONSOLE", "1");
        std::cout << "[DEBUG] Git Flux console logging enabled." << std::endl;
    } else if (wantsCliInfo) {
        if (AttachConsole(ATTACH_PARENT_PROCESS)) {
            FILE* fp = nullptr;
            freopen_s(&fp, "CONOUT$", "w", stdout);
            freopen_s(&fp, "CONOUT$", "w", stderr);
            std::cout.clear();
            std::cerr.clear();
        }
    }
#endif

    QCoreApplication::setOrganizationName(QStringLiteral("GitFlux"));
    QCoreApplication::setApplicationName(QStringLiteral("GitFlux"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    // Ensure smooth, fluid scrolling across all Flickables, ListViews, and ScrollViews (Qt 6.6+ physics)
    if (qEnvironmentVariableIsEmpty("QT_QUICK_FLICKABLE_WHEEL_DECELERATION")) {
        qputenv("QT_QUICK_FLICKABLE_WHEEL_DECELERATION", "2500");
    }

    QGuiApplication app(argc, argv);
    app.setApplicationDisplayName(QStringLiteral("Git Flux"));
    app.setDesktopFileName(QStringLiteral("git-flux"));

    // Set application icon from assets
    QIcon appIcon(QStringLiteral(":/qt/qml/GitFlux/Desktop/assets/logo.png"));
    if (appIcon.isNull()) {
        appIcon = QIcon(QStringLiteral("assets/logo.png"));
    }
    if (appIcon.isNull()) {
        appIcon = QIcon(QStringLiteral("../assets/logo.png"));
    }
    if (appIcon.isNull()) {
        appIcon = QIcon(QStringLiteral("/projects/GitFlux/assets/logo.png"));
    }
    if (!appIcon.isNull()) {
        app.setWindowIcon(appIcon);
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Git Flux — Modern local-first desktop Git client"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption debugOption(
        QStringList() << QStringLiteral("debug") << QStringLiteral("console"),
        QStringLiteral("Enables debug console output and logs")
    );
    parser.addOption(debugOption);

    QCommandLineOption dbOption(
        QStringList() << QStringLiteral("d") << QStringLiteral("db"),
        QStringLiteral("Specifies SQLite database path"),
        QStringLiteral("path")
    );
    parser.addOption(dbOption);

    QCommandLineOption smokeTestOption(
        QStringLiteral("test-startup"),
        QStringLiteral("Verifies complete bootstrap and QML loading then exits immediately")
    );
    parser.addOption(smokeTestOption);

    parser.process(app);

    std::filesystem::path dbPath;
    if (parser.isSet(dbOption)) {
        dbPath = std::filesystem::path(parser.value(dbOption).toStdString());
    } else if (parser.isSet(smokeTestOption)) {
        dbPath = ":memory:";
    } else {
        dbPath = gf::desktop::AppBootstrap::defaultDatabasePath();
    }

    try {
        gf::desktop::AppBootstrap bootstrap(dbPath);
        bootstrap.initialize();

        QQmlApplicationEngine engine;
        engine.setNetworkAccessManagerFactory(new RedirectNetworkAccessManagerFactory());
        bootstrap.registerWithQml(engine);

        const QUrl url(QStringLiteral("qrc:/qt/qml/GitFlux/Desktop/src/desktop/ui/qml/main.qml"));
        QObject::connect(
            &engine,
            &QQmlApplicationEngine::objectCreated,
            &app,
            [url](QObject* obj, const QUrl& objUrl) {
                if (!obj && url == objUrl) {
                    std::cerr << "Critical error: unable to load main QML file: "
                              << url.toString().toStdString() << std::endl;
                    QCoreApplication::exit(-1);
                }
            },
            Qt::QueuedConnection
        );

        engine.load(url);

        if (engine.rootObjects().isEmpty()) {
            std::cerr << "Error: no QML root objects loaded." << std::endl;
            return 1;
        }

        if (!appIcon.isNull()) {
            for (auto* rootObj : engine.rootObjects()) {
                if (auto* win = qobject_cast<QQuickWindow*>(rootObj)) {
                    win->setIcon(appIcon);
                }
            }
        }

        if (parser.isSet(smokeTestOption)) {
            std::cout << "[SMOKE_TEST] Git Flux bootstrap & QML engine initialized successfully." << std::endl;
            return 0;
        }

        return app.exec();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error starting Git Flux: " << ex.what() << std::endl;
        return 1;
    }
}
