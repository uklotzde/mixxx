#include "aoide/tracklistview.h"

#include <QtQml/QQmlContext>
#include <QtQml/QQmlError>

#include "aoide/tracklistmodel.h"

#include "util/assert.h"
#include "util/logger.h"

namespace mixxx {

namespace aoide {

namespace {

const Logger kLogger("aoide TrackListView");

} // anonymous namespace

TrackListView::TrackListView(
        TrackListModel* model,
        QWidget* parent)
        : QQuickWidget(parent),
          m_model(model) {
    connect(this, &QQuickWidget::statusChanged,
            this, &TrackListView::quickWidgetStatusChanged);
    connect(this, &QQuickWidget::sceneGraphError,
            this, &TrackListView::sceneGraphError);
    QQmlContext* ctx = rootContext();
    ctx->setContextProperty("trackListModel", m_model);
    setSource(QUrl("qrc:/qml/tracklistview.qml"));
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    kLogger.debug() << "Created instance" << this;
}

TrackListView::~TrackListView() {
    kLogger.debug() << "Destroying instance" << this;
}

void TrackListView::quickWidgetStatusChanged(QQuickWidget::Status status)
{
    VERIFY_OR_DEBUG_ASSERT(status != QQuickWidget::Error) {
        const auto widgetErrors = errors();
        for (const QQmlError &error : widgetErrors) {
            kLogger.warning() << "Widget error:" << error.toString();
        }
    }
}

void TrackListView::sceneGraphError(QQuickWindow::SceneGraphError /*error*/, const QString& message) {
    kLogger.warning() << "Scene graph error:" << message;
}

void TrackListView::onShow() {
    DEBUG_ASSERT(m_model);
}

bool TrackListView::hasFocus() const {
    return QQuickWidget::hasFocus();
}

void TrackListView::onSearch(const QString& text) {
    m_model->searchTracks(text);
}

} // namespace aoide

} // namespace mixxx
