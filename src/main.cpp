#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QLocale>
#include <QTranslator>
#include "FlashViewStyle.h"
#include "MainWindow.h"
#include "ThemeManager.h"

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
    parser.process(app);

    // Apply initial theme
    ThemeManager &tm = ThemeManager::instance();
    app.setStyleSheet(tm.themeStyleSheet());

    // Create main window
    MainWindow w;

    // Open file/directory from command line
    QStringList args = parser.positionalArguments();
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
