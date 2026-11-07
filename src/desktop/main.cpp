#include "desktop/bootstrap/app_bootstrap.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlNetworkAccessManagerFactory>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <iostream>

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
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("GitFlux"));
    app.setOrganizationName(QStringLiteral("GitFlux"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Git Flux — Modern local-first desktop Git client"));
    parser.addHelpOption();
    parser.addVersionOption();

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
