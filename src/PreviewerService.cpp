#include "PreviewerService.h"

#include "PreviewWindow.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QFileInfo>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

namespace {
constexpr int kDefaultIdleMinutes = 10;
} // namespace

QString PreviewerService::defaultServiceName()
{
    return QStringLiteral("org.gnome.NautilusPreviewer");
}

QString PreviewerService::objectPath()
{
    return QStringLiteral("/org/gnome/NautilusPreviewer");
}

QString PreviewerService::interfaceName()
{
    return QStringLiteral("org.gnome.NautilusPreviewer2");
}

PreviewerService::PreviewerService(QObject *parent) : QObject(parent)
{
    // A resident process is what makes the preview feel instant, but an idle
    // one should not hold memory forever: the bus restarts it on demand.
    m_idleTimer = new QTimer(this);
    m_idleTimer->setSingleShot(true);
    m_idleTimer->setInterval(kDefaultIdleMinutes * 60 * 1000);
    connect(m_idleTimer, &QTimer::timeout, qApp, &QCoreApplication::quit);
    m_idleTimer->start();
}

void PreviewerService::setIdleTimeoutMinutes(int minutes)
{
    if (minutes <= 0) {
        m_idleTimer->stop();
        return;
    }
    m_idleTimer->setInterval(minutes * 60 * 1000);
    if (!m_visible)
        m_idleTimer->start();
}

PreviewerService::Registration PreviewerService::registerOnBus(
    QString *message, const QString &serviceName)
{
    const QString name = serviceName.isEmpty() ? defaultServiceName() : serviceName;
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        if (message)
            *message = tr("No session bus is available for the preview service.");
        return Registration::BusUnavailable;
    }

    // Adaptors live as long as the service; a retry must not add a second set.
    if (!findChild<NautilusPreviewer2Adaptor *>()) {
        new NautilusPreviewer2Adaptor(this);
        new NautilusPreviewerAdaptor(this);
    }

    // Publish the object before claiming the name so the service is never
    // reachable under a name it cannot answer.
    if (!bus.registerObject(objectPath(), this, QDBusConnection::ExportAdaptors)) {
        if (message)
            *message = tr("The preview service could not be published on %1.").arg(objectPath());
        return Registration::ExportFailed;
    }

    if (!bus.registerService(name)) {
        bus.unregisterObject(objectPath());
        if (message)
            *message = tr("Another preview service already owns %1.").arg(name);
        return Registration::NameTaken;
    }

    if (message)
        message->clear();
    return Registration::Registered;
}

void PreviewerService::ensureWindow()
{
    if (m_window)
        return;

    m_window = new PreviewWindow;
    connect(m_window, &PreviewWindow::closeRequested, this, &PreviewerService::closePreview);
    connect(m_window, &PreviewWindow::selectionRequested, this,
            [this](uint direction) { emit selectionEvent(direction); });
}

void PreviewerService::showFile(const QString &uri, const QString &windowHandle,
                                bool closeIfAlreadyShown)
{
    ensureWindow();

    QUrl url(uri);
    if (url.scheme().isEmpty())
        url = QUrl::fromLocalFile(uri); // tolerate a bare path from manual calls
    const QString path = url.isLocalFile() ? url.toLocalFile() : QString();

    // Pressing the preview key again on the file already shown dismisses it.
    if (closeIfAlreadyShown && m_visible && !path.isEmpty()
        && m_window->currentFile() == path) {
        closePreview();
        return;
    }

    if (path.isEmpty()) {
        // Remote locations are not readable by the viewer; say so rather than
        // leaving the file manager with a preview that never appears.
        m_window->showUnsupported(tr("FlashView can only preview local files"), uri);
    } else {
        m_window->showFile(path, windowHandle);
    }

    setVisible(true);
}

void PreviewerService::closePreview()
{
    if (m_window)
        m_window->hide();
    setVisible(false);
}

void PreviewerService::setVisible(bool visible)
{
    if (visible)
        m_idleTimer->stop();
    else if (m_idleTimer->interval() > 0)
        m_idleTimer->start();

    if (m_visible == visible)
        return;
    m_visible = visible;

    // QtDBus does not emit PropertiesChanged by itself, and the file manager
    // reads this property from its proxy cache to decide whether to keep
    // pushing selection updates. Without this signal the preview would stop
    // following the selection after the first move.
    QDBusMessage changed = QDBusMessage::createSignal(
        objectPath(), QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));
    changed << interfaceName()
            << QVariantMap{{QStringLiteral("Visible"), m_visible}}
            << QStringList();
    QDBusConnection::sessionBus().send(changed);
}

NautilusPreviewer2Adaptor::NautilusPreviewer2Adaptor(PreviewerService *service)
    : QDBusAbstractAdaptor(service), m_service(service)
{
    setAutoRelaySignals(false);
    connect(service, &PreviewerService::selectionEvent,
            this, &NautilusPreviewer2Adaptor::SelectionEvent);
}

bool NautilusPreviewer2Adaptor::visible() const
{
    return m_service->isVisible();
}

void NautilusPreviewer2Adaptor::ShowFile(const QString &uri, const QString &windowHandle,
                                         bool closeIfAlreadyShown)
{
    m_service->showFile(uri, windowHandle, closeIfAlreadyShown);
}

void NautilusPreviewer2Adaptor::Close()
{
    m_service->closePreview();
}

NautilusPreviewerAdaptor::NautilusPreviewerAdaptor(PreviewerService *service)
    : QDBusAbstractAdaptor(service), m_service(service)
{
    setAutoRelaySignals(false);
}

void NautilusPreviewerAdaptor::ShowFile(const QString &uri, int xid,
                                        bool closeIfAlreadyShown)
{
    // The old interface passes a bare X11 id; convert it to the handle format
    // the rest of the code understands.
    const QString handle = xid > 0
        ? QStringLiteral("x11:%1").arg(static_cast<uint>(xid), 0, 16)
        : QString();
    m_service->showFile(uri, handle, closeIfAlreadyShown);
}

void NautilusPreviewerAdaptor::Close()
{
    m_service->closePreview();
}
