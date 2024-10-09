#include "aoide/tracksearchoverlayfilterdlg.h"

#include "moc_tracksearchoverlayfilterdlg.cpp"

namespace {

const QChar kGenreLabelSeparator = QChar{';'};

} // anonymous namespace

namespace aoide {

TrackSearchOverlayFilterDlg::TrackSearchOverlayFilterDlg(
        QDateTime sessionStartedAt,
        TrackSearchOverlayFilter overlayFilter,
        QWidget* parent)
        : QDialog(parent),
          m_sessionStartedAt(std::move(sessionStartedAt)),
          m_overlayFilter(std::move(overlayFilter)) {
    setupUi(this);
    init();
    connect(sessionRestartButton,
            &QAbstractButton::clicked,
            this,
            [this](bool checked) {
                Q_UNUSED(checked)
                const auto now = QDateTime::currentDateTime();
                sessionStartInput->setDateTime(now);
            });
    connect(minBpmResetButton,
            &QAbstractButton::clicked,
            this,
            [this](bool checked) {
                Q_UNUSED(checked)
                minBpmDoubleSpinBox->setValue(mixxx::Bpm::kValueUndefined);
            });
    connect(maxBpmResetButton,
            &QAbstractButton::clicked,
            this,
            [this](bool checked) {
                Q_UNUSED(checked)
                maxBpmDoubleSpinBox->setValue(mixxx::Bpm::kValueUndefined);
            });
}

void TrackSearchOverlayFilterDlg::accept() {
    QDialog::accept();
    apply();
}

void TrackSearchOverlayFilterDlg::reject() {
    QDialog::reject();
    reset();
}

void TrackSearchOverlayFilterDlg::init() {
    minBpmDoubleSpinBox->setMinimum(mixxx::Bpm::kValueMin);
    minBpmDoubleSpinBox->setMaximum(mixxx::Bpm::kValueMax);
    minBpmDoubleSpinBox->setDecimals(0);
    minBpmDoubleSpinBox->setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
    minBpmDoubleSpinBox->setAccelerated(true);
    maxBpmDoubleSpinBox->setMinimum(mixxx::Bpm::kValueMin);
    maxBpmDoubleSpinBox->setMaximum(mixxx::Bpm::kValueMax);
    maxBpmDoubleSpinBox->setDecimals(0);
    maxBpmDoubleSpinBox->setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
    maxBpmDoubleSpinBox->setAccelerated(true);
    reset();
}

void TrackSearchOverlayFilterDlg::reset() {
    sessionStartInput->setDateTime(m_sessionStartedAt);
    minBpmDoubleSpinBox->setValue(m_overlayFilter.minBpm.valueOr(mixxx::Bpm::kValueUndefined));
    maxBpmDoubleSpinBox->setValue(m_overlayFilter.maxBpm.valueOr(mixxx::Bpm::kValueUndefined));
    genreTextLineEdit->setText(mixxx::library::tags::joinLabelsAsText(
            m_overlayFilter.anyGenreLabels, kGenreLabelSeparator));
    hashtagAllTextLineEdit->setText(mixxx::library::tags::joinLabelsAsText(
            m_overlayFilter.allHashtagLabels));
    commentAllTextLineEdit->setText(mixxx::library::tags::joinLabelsAsText(
            m_overlayFilter.allCommentTerms));
    commentAnyTextLineEdit->setText(mixxx::library::tags::joinLabelsAsText(
            m_overlayFilter.anyCommentTerms));
}

void TrackSearchOverlayFilterDlg::apply() {
    if (sessionStartInput->hasAcceptableInput()) {
        m_sessionStartedAt = sessionStartInput->dateTime();
    }
    if (minBpmDoubleSpinBox->hasAcceptableInput()) {
        m_overlayFilter.minBpm = mixxx::Bpm(minBpmDoubleSpinBox->value());
    }
    if (maxBpmDoubleSpinBox->hasAcceptableInput()) {
        m_overlayFilter.maxBpm = mixxx::Bpm(maxBpmDoubleSpinBox->value());
    }
    m_overlayFilter.anyGenreLabels = mixxx::library::tags::splitTextIntoLabels(
            genreTextLineEdit->text(), kGenreLabelSeparator);
    m_overlayFilter.allHashtagLabels =
            mixxx::library::tags::splitTextIntoLabelsAtWhitespace(
                    hashtagAllTextLineEdit->text());
    m_overlayFilter.allCommentTerms =
            mixxx::library::tags::splitTextIntoLabelsAtWhitespace(
                    commentAllTextLineEdit->text());
    m_overlayFilter.anyCommentTerms =
            mixxx::library::tags::splitTextIntoLabelsAtWhitespace(
                    commentAnyTextLineEdit->text());
    reset();
}

} // namespace aoide
