#pragma once
#include <QWidget>
#include <QVector>

class MpvWidget;
class MpvCore;
class QPushButton;
class QLabel;
class QButtonGroup;

enum class MultiViewLayout { Single, PIP, DualH, DualV, Quad };

class MultiViewWidget : public QWidget {
    Q_OBJECT
public:
    explicit MultiViewWidget(MpvWidget* primary, QWidget* parent = nullptr);
    ~MultiViewWidget();

    MultiViewLayout layout() const { return layout_; }
    MpvWidget* primaryWidget() const;
    MpvWidget* addSecondaryFile(const QString& path);

public slots:
    void setLayout(MultiViewLayout l);
    void loadSecondaryFile(const QString& path, int idx = 0);
    void closeSecondary(int idx);

signals:
    void layoutChanged(MultiViewLayout l);
    void secondaryFileRequested();

protected:
    void resizeEvent(QResizeEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private:
    void buildUI();
    void buildLayoutButtons();
    void applyLayout();
    void positionWidgets();

    MpvWidget*           primary_  = nullptr;
    QVector<MpvWidget*>  secondary_;
    MultiViewLayout      layout_   = MultiViewLayout::Single;

    // Layout control bar (overlay)
    QWidget*     layoutBar_   = nullptr;
    QPushButton* btnPip_      = nullptr;
    QPushButton* btnDualH_    = nullptr;
    QPushButton* btnDualV_    = nullptr;
    QPushButton* btnTriple_   = nullptr;
    QPushButton* btnQuad_     = nullptr;
    QPushButton* btnSingle_   = nullptr;
};
