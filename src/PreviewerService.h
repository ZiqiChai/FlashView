#ifndef PREVIEWERSERVICE_H
#define PREVIEWERSERVICE_H

#include <QDBusAbstractAdaptor>
#include <QObject>
#include <QString>

class QTimer;
class PreviewWindow;

// Implements the preview protocol GNOME Files (nautilus) uses for its space-bar
// preview: the file manager calls ShowFile over D-Bus, the service pops up an
// overlay, and arrow keys in that overlay travel back as SelectionEvent so the
// file manager moves its own selection. Browsing therefore always follows the
// order shown in the file manager, whatever that order happens to be.
//
// The service is deliberately optional: when the session bus, the interface or
// the bus name is unavailable, FlashView keeps working as a plain viewer.
class PreviewerService : public QObject
{
    Q_OBJECT

public:
    enum class Registration {
        Registered,     // ready to serve the file manager
        BusUnavailable, // no session bus (tty, container, broken session)
        NameTaken,      // another previewer owns the name, leave it alone
        ExportFailed    // the object could not be published
    };
    Q_ENUM(Registration)

    explicit PreviewerService(QObject *parent = nullptr);

    static QString defaultServiceName();
    static QString objectPath();
    static QString interfaceName();

    // An explicit service name keeps tests off the name the desktop uses.
    Registration registerOnBus(QString *message = nullptr,
                               const QString &serviceName = QString());

    void showFile(const QString &uri, const QString &windowHandle,
                  bool closeIfAlreadyShown);
    void closePreview();
    bool isVisible() const { return m_visible; }
    PreviewWindow *previewWindow() const { return m_window; }

    // Minutes of inactivity after which the resident service exits; the bus
    // starts it again on the next preview.
    void setIdleTimeoutMinutes(int minutes);

signals:
    void selectionEvent(uint direction);

private:
    void ensureWindow();
    void setVisible(bool visible);

    PreviewWindow *m_window = nullptr;
    QTimer *m_idleTimer = nullptr;
    bool m_visible = false;
};

// Current interface, used by GNOME Files 3.34 and newer.
class NautilusPreviewer2Adaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.gnome.NautilusPreviewer2")
    Q_PROPERTY(bool Visible READ visible)

public:
    explicit NautilusPreviewer2Adaptor(PreviewerService *service);

    bool visible() const;

public slots:
    void ShowFile(const QString &uri, const QString &windowHandle,
                  bool closeIfAlreadyShown);
    void Close();

signals:
    void SelectionEvent(uint direction);

private:
    PreviewerService *m_service = nullptr;
};

// Legacy interface, kept so older callers that still pass a bare X11 window id
// keep working on distributions that ship an older file manager.
class NautilusPreviewerAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.gnome.NautilusPreviewer")

public:
    explicit NautilusPreviewerAdaptor(PreviewerService *service);

public slots:
    void ShowFile(const QString &uri, int xid, bool closeIfAlreadyShown);
    void Close();

private:
    PreviewerService *m_service = nullptr;
};

#endif // PREVIEWERSERVICE_H
