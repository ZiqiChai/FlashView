#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QLocale>
#include <QSettings>
#include <QTranslator>
#include <QtGlobal>
#include "FlashViewStyle.h"
#include "MainWindow.h"
#include "ThemeManager.h"
#ifdef FLASHVIEW_HAVE_PREVIEWER
#include "PreviewerService.h"
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setStyle(new FlashViewStyle);
    app.setApplicationName("FlashView");
    app.setOrganizationName("FlashView");
    app.setApplicationVersion("1.0.0");
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/flashview.svg")));
    QApplication::setDesktopFileName(QStringLiteral("flashview"));

    // Command line
    QCommandLineParser parser;
    parser.setApplicationDescription("FlashView - Fast Image & Video Viewer");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(
        "file", "File or directory to open", "[file_or_directory]");
    // Accepted in every build so the option never turns into a parse error on
    // a viewer that was built without the preview service.
    QCommandLineOption previewerOption(
        QStringLiteral("previewer"),
        QStringLiteral("Run as the file manager preview service."));
    parser.addOption(previewerOption);
    parser.process(app);

    // Apply initial theme
    ThemeManager &tm = ThemeManager::instance();
    app.setStyleSheet(tm.themeStyleSheet());

    QStringList args = parser.positionalArguments();

    if (parser.isSet(previewerOption)) {
#ifdef FLASHVIEW_HAVE_PREVIEWER
        // There is no main window here to pick up the language setting, so the
        // preview has to translate itself.
        QSettings settings;
        const int defaultLanguage =
            QLocale::system().language() == QLocale::Chinese ? 0 : 1;
        const int language = qBound(0, settings.value("language", defaultLanguage).toInt(), 1);
        QTranslator translator;
        if (translator.load(language == 0 ? QStringLiteral(":/i18n/flashview_zh.qm")
                                          : QStringLiteral(":/i18n/flashview_en.qm")))
            app.installTranslator(&translator);

        PreviewerService previewer;
        QString message;
        const PreviewerService::Registration result = previewer.registerOnBus(&message);
        if (result == PreviewerService::Registration::Registered) {
            // The service outlives its preview window and waits for the next
            // request from the file manager.
            app.setQuitOnLastWindowClosed(false);
            return app.exec();
        }
        qWarning("%s", qPrintable(message));
        if (result == PreviewerService::Registration::NameTaken)
            return 0; // another previewer is in charge, do not fight over it
#else
        qWarning("This build has no preview service; opening the viewer instead.");
#endif
        // Fall back to the plain viewer so the file still gets shown.
        if (args.isEmpty())
            return 1;
    }

    // Create main window
    MainWindow w;

    // Open file/directory from command line
    if (!args.isEmpty()) {
        QString path = args.first();
        QFileInfo fi(path);
        if (fi.isDir())
            w.openDirectory(fi.absoluteFilePath());
        else
            w.openFile(fi.absoluteFilePath());
    }

    w.show();
    return app.exec();
}
