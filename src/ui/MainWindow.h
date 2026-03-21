#pragma once
#include <QMainWindow>
#include <memory>

namespace document { class Document; }

class Viewport3D;
class FeatureTree;
class TimelinePanel;
class PropertiesPanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void setupUI();
    void setupMenuBar();
    void setupDocks();
    void connectSignals();

    // Actions
    void onNewDocument();
    void onOpenDocument();
    void onSaveDocument();
    void onExportSTEP();

    std::unique_ptr<document::Document> m_document;

    // Panels
    Viewport3D*     m_viewport     = nullptr;
    FeatureTree*    m_featureTree  = nullptr;
    TimelinePanel*  m_timeline     = nullptr;
    PropertiesPanel* m_properties  = nullptr;
};
