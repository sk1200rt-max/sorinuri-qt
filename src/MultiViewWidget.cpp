#include "MultiViewWidget.h"
#include "MpvWidget.h"
#include "MpvCore.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QResizeEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QButtonGroup>

static const char* MULTIVIEW_STYLE = R"(
QWidget#layoutBar {
    background: rgba(10,10,10,200); border-radius: 6px;
}
QPushButton#layoutBtn {
    background: transparent; border: 1px solid #333; border-radius: 4px;
    color: #888; font-size: 10px; padding: 3px 8px; min-width: 50px;
}
QPushButton#layoutBtn:checked {
    background: #00c8b4; border-color: #00c8b4; color: #000; font-weight: bold;
}
QPushButton#layoutBtn:hover { border-color: #00c8b4; color: #00c8b4; }
)";

MultiViewWidget::MultiViewWidget(MpvWidget* primary, QWidget* parent)
    : QWidget(parent), primary_(primary)
{
    setAcceptDrops(true);
    setStyleSheet(MULTIVIEW_STYLE);
    buildUI();
}

MultiViewWidget::~MultiViewWidget() {
    for (auto* w : secondary_) {
        w->deleteLater();
    }
}

void MultiViewWidget::buildUI() {
    // primary를 이 위젯의 자식으로 설정
    if (primary_) {
        primary_->setParent(this);
        primary_->show();
    }

    // 레이아웃 컨트롤 바 (하단 오버레이)
    layoutBar_ = new QWidget(this);
    layoutBar_->setObjectName("layoutBar");
    layoutBar_->setFixedHeight(34);

    auto* barLayout = new QHBoxLayout(layoutBar_);
    barLayout->setContentsMargins(8, 4, 8, 4);
    barLayout->setSpacing(4);

    auto* lblLayout = new QLabel("레이아웃:");
    lblLayout->setStyleSheet("color:#666; font-size:10px;");
    barLayout->addWidget(lblLayout);

    btnSingle_ = new QPushButton("단일");
    btnPip_    = new QPushButton("PIP");
    btnDualH_  = new QPushButton("듀얼(좌우)");
    btnDualV_  = new QPushButton("듀얼(상하)");
    btnQuad_   = new QPushButton("쿼드");

    for (auto* btn : {btnSingle_, btnPip_, btnDualH_, btnDualV_, btnQuad_}) {
        btn->setObjectName("layoutBtn");
        btn->setCheckable(true);
        barLayout->addWidget(btn);
    }
    btnSingle_->setChecked(true);
    barLayout->addStretch();

    auto* grp = new QButtonGroup(this);
    grp->addButton(btnSingle_, (int)MultiViewLayout::Single);
    grp->addButton(btnPip_,    (int)MultiViewLayout::PIP);
    grp->addButton(btnDualH_,  (int)MultiViewLayout::DualH);
    grp->addButton(btnDualV_,  (int)MultiViewLayout::DualV);
    grp->addButton(btnQuad_,   (int)MultiViewLayout::Quad);

    connect(grp, QOverload<int>::of(&QButtonGroup::idClicked), this, [this](int id){
        setLayout((MultiViewLayout)id);
    });

    positionWidgets();
}

void MultiViewWidget::setLayout(MultiViewLayout l) {
    layout_ = l;

    // 필요한 보조 위젯 수 계산
    int needed = 0;
    switch (l) {
    case MultiViewLayout::Single: needed = 0; break;
    case MultiViewLayout::PIP:    needed = 1; break;
    case MultiViewLayout::DualH:
    case MultiViewLayout::DualV:  needed = 1; break;
    case MultiViewLayout::Quad:   needed = 3; break;
    }

    // 보조 위젯 생성 (필요시)
    while (secondary_.size() < needed) {
        auto* w = new MpvWidget(this);
        w->show();
        secondary_.append(w);
    }
    // 불필요한 보조 위젯 숨기기
    for (int i = needed; i < secondary_.size(); ++i) {
        secondary_[i]->hide();
    }

    positionWidgets();
    emit layoutChanged(l);
}

void MultiViewWidget::positionWidgets() {
    if (!primary_) return;

    QRect r = rect();
    // 레이아웃 바 위치
    layoutBar_->setGeometry(r.x() + 8, r.bottom() - 42, r.width() - 16, 34);

    int barH = 42; // 레이아웃 바 높이 + 여백
    QRect videoArea(r.x(), r.y(), r.width(), r.height() - barH);

    switch (layout_) {
    case MultiViewLayout::Single:
        primary_->setGeometry(videoArea);
        break;

    case MultiViewLayout::PIP: {
        primary_->setGeometry(videoArea);
        if (!secondary_.isEmpty()) {
            int pipW = videoArea.width() / 4;
            int pipH = videoArea.height() / 4;
            secondary_[0]->setGeometry(
                videoArea.right() - pipW - 12,
                videoArea.bottom() - pipH - 12,
                pipW, pipH);
            secondary_[0]->raise();
            secondary_[0]->show();
        }
        break;
    }

    case MultiViewLayout::DualH: {
        int half = videoArea.width() / 2;
        primary_->setGeometry(videoArea.x(), videoArea.y(), half, videoArea.height());
        if (!secondary_.isEmpty()) {
            secondary_[0]->setGeometry(videoArea.x() + half, videoArea.y(), half, videoArea.height());
            secondary_[0]->show();
        }
        break;
    }

    case MultiViewLayout::DualV: {
        int half = videoArea.height() / 2;
        primary_->setGeometry(videoArea.x(), videoArea.y(), videoArea.width(), half);
        if (!secondary_.isEmpty()) {
            secondary_[0]->setGeometry(videoArea.x(), videoArea.y() + half, videoArea.width(), half);
            secondary_[0]->show();
        }
        break;
    }

    case MultiViewLayout::Quad: {
        int hw = videoArea.width() / 2;
        int hh = videoArea.height() / 2;
        primary_->setGeometry(videoArea.x(), videoArea.y(), hw, hh);
        for (int i = 0; i < qMin(3, (int)secondary_.size()); ++i) {
            int col = (i + 1) % 2;
            int row = (i + 1) / 2;
            secondary_[i]->setGeometry(videoArea.x() + col * hw, videoArea.y() + row * hh, hw, hh);
            secondary_[i]->show();
        }
        break;
    }
    }

    layoutBar_->raise();
}

void MultiViewWidget::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    positionWidgets();
}

void MultiViewWidget::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void MultiViewWidget::dropEvent(QDropEvent* e) {
    if (!e->mimeData()->hasUrls()) return;
    QString path = e->mimeData()->urls().first().toLocalFile();
    if (!path.isEmpty()) loadSecondaryFile(path);
}

void MultiViewWidget::loadSecondaryFile(const QString& path) {
    if (secondary_.isEmpty()) {
        setLayout(MultiViewLayout::PIP);
    }
    if (!secondary_.isEmpty()) {
        secondary_[0]->core()->loadFile(path);
    }
}

void MultiViewWidget::closeSecondary(int idx) {
    if (idx < 0 || idx >= secondary_.size()) return;
    secondary_[idx]->hide();
}

MpvWidget* MultiViewWidget::addSecondaryFile(const QString& path) {
    auto* w = new MpvWidget(this);
    w->core()->loadFile(path);
    secondary_.append(w);
    positionWidgets();
    return w;
}

MpvWidget* MultiViewWidget::primaryWidget() const {
    return primary_;
}
