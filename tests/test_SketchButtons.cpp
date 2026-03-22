#include <QtTest/QtTest>
#include <QToolButton>
#include <QButtonGroup>
#include <QToolBar>
#include <QApplication>

// Test that QButtonGroup with checkable buttons works correctly
// (validates our approach to sketch tool button persistence)
class TestSketchButtons : public QObject
{
    Q_OBJECT

private slots:
    void testButtonGroupExclusive()
    {
        // Simulate the sketch toolbar setup
        QToolBar toolbar;
        auto* group = new QButtonGroup(&toolbar);
        group->setExclusive(true);

        auto* btnLine = new QToolButton;
        btnLine->setCheckable(true);
        btnLine->setToolTip("Line (L)");
        group->addButton(btnLine, 1);
        toolbar.addWidget(btnLine);

        auto* btnCircle = new QToolButton;
        btnCircle->setCheckable(true);
        btnCircle->setToolTip("Circle (C)");
        group->addButton(btnCircle, 2);
        toolbar.addWidget(btnCircle);

        auto* btnRect = new QToolButton;
        btnRect->setCheckable(true);
        btnRect->setToolTip("Rectangle (R)");
        group->addButton(btnRect, 3);
        toolbar.addWidget(btnRect);

        // Initially no button checked
        QVERIFY(!btnLine->isChecked());
        QVERIFY(!btnCircle->isChecked());

        // Check Line button
        btnLine->setChecked(true);
        QVERIFY(btnLine->isChecked());
        QVERIFY(!btnCircle->isChecked());
        QVERIFY(!btnRect->isChecked());

        // Check Circle — Line should uncheck
        btnCircle->setChecked(true);
        QVERIFY(!btnLine->isChecked());
        QVERIFY(btnCircle->isChecked());
        QVERIFY(!btnRect->isChecked());

        // Programmatic check via group->button(id)
        QAbstractButton* btn = group->button(3);
        QVERIFY(btn == btnRect);
        btn->blockSignals(true);
        btn->setChecked(true);
        btn->blockSignals(false);
        QVERIFY(btnRect->isChecked());
        QVERIFY(!btnCircle->isChecked()); // exclusive unchecks others
    }

    void testButtonStaysCheckedAfterClick()
    {
        QToolBar toolbar;
        auto* group = new QButtonGroup(&toolbar);
        group->setExclusive(true);

        auto* btn1 = new QToolButton;
        btn1->setCheckable(true);
        group->addButton(btn1, 1);
        toolbar.addWidget(btn1);

        auto* btn2 = new QToolButton;
        btn2->setCheckable(true);
        group->addButton(btn2, 2);
        toolbar.addWidget(btn2);

        // Simulate clicking btn1
        btn1->click();
        QVERIFY(btn1->isChecked());

        // Click btn1 again — with exclusive group, it should STAY checked
        // (can't uncheck the only checked button in exclusive group)
        btn1->click();
        QVERIFY(btn1->isChecked()); // stays checked!

        // Click btn2
        btn2->click();
        QVERIFY(btn2->isChecked());
        QVERIFY(!btn1->isChecked());
    }

    void testKeyboardToolSwitch()
    {
        // Simulate keyboard switching: when user presses L,
        // the Line button should become checked
        QToolBar toolbar;
        auto* group = new QButtonGroup(&toolbar);
        group->setExclusive(true);

        auto* btnLine = new QToolButton;
        btnLine->setCheckable(true);
        group->addButton(btnLine, 1); // DrawLine = 1
        toolbar.addWidget(btnLine);

        auto* btnCircle = new QToolButton;
        btnCircle->setCheckable(true);
        group->addButton(btnCircle, 4); // DrawCircle = 4
        toolbar.addWidget(btnCircle);

        // Simulate: user presses L -> tool changes to DrawLine (1)
        int newTool = 1;
        QAbstractButton* match = group->button(newTool);
        QVERIFY(match == btnLine);
        match->blockSignals(true);
        match->setChecked(true);
        match->blockSignals(false);
        QVERIFY(btnLine->isChecked());

        // Simulate: user presses C -> tool changes to DrawCircle (4)
        newTool = 4;
        match = group->button(newTool);
        QVERIFY(match == btnCircle);
        match->blockSignals(true);
        match->setChecked(true);
        match->blockSignals(false);
        QVERIFY(btnCircle->isChecked());
        QVERIFY(!btnLine->isChecked()); // unchecked by exclusive group
    }
};

QTEST_MAIN(TestSketchButtons)
#include "test_SketchButtons.moc"
