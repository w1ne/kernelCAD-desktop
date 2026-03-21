#include <QtTest/QtTest>
#include <cmath>
#include "document/ParameterStore.h"
#include "document/ExpressionUtil.h"

class TestParameterStore : public QObject
{
    Q_OBJECT

private slots:
    // ---------------------------------------------------------------
    // Basic set/get/has/remove (pre-existing)
    // ---------------------------------------------------------------
    void testSetAndGet()
    {
        document::ParameterStore store;
        store.set("width", "50", "mm");
        QCOMPARE(store.get("width"), 50.0);
    }

    void testHas()
    {
        document::ParameterStore store;
        QVERIFY(!store.has("width"));
        store.set("width", "50");
        QVERIFY(store.has("width"));
    }

    void testRemove()
    {
        document::ParameterStore store;
        store.set("width", "50");
        store.remove("width");
        QVERIFY(!store.has("width"));
    }

    void testEvaluateNumeric()
    {
        document::ParameterStore store;
        QCOMPARE(store.evaluate("42.5"), 42.5);
    }

    void testEvaluateParamReference()
    {
        document::ParameterStore store;
        store.set("height", "30");
        QCOMPARE(store.evaluate("height"), 30.0);
    }

    void testGetThrowsOnMissing()
    {
        document::ParameterStore store;
        QVERIFY_EXCEPTION_THROWN(store.get("nonexistent"), std::runtime_error);
    }

    void testEvaluateThrowsOnBadExpr()
    {
        document::ParameterStore store;
        QVERIFY_EXCEPTION_THROWN(store.evaluate("not_a_number"), std::runtime_error);
    }

    void testOverwrite()
    {
        document::ParameterStore store;
        store.set("width", "50");
        store.set("width", "100");
        QCOMPARE(store.get("width"), 100.0);
    }

    void testAllParams()
    {
        document::ParameterStore store;
        store.set("a", "1");
        store.set("b", "2");
        store.set("c", "3");
        QCOMPARE(store.all().size(), size_t(3));
    }

    // ---------------------------------------------------------------
    // Expression evaluation -- arithmetic & precedence
    // ---------------------------------------------------------------
    void testArithmeticAddSub()
    {
        document::ParameterStore store;
        QCOMPARE(store.evaluate("3 + 4"), 7.0);
        QCOMPARE(store.evaluate("10 - 3"), 7.0);
        QCOMPARE(store.evaluate("1 + 2 + 3"), 6.0);
    }

    void testArithmeticMulDiv()
    {
        document::ParameterStore store;
        QCOMPARE(store.evaluate("3 * 4"), 12.0);
        QCOMPARE(store.evaluate("10 / 4"), 2.5);
    }

    void testPrecedence()
    {
        document::ParameterStore store;
        // * binds tighter than +
        QCOMPARE(store.evaluate("2 + 3 * 4"), 14.0);
        QCOMPARE(store.evaluate("2 * 3 + 4"), 10.0);
    }

    void testParentheses()
    {
        document::ParameterStore store;
        QCOMPARE(store.evaluate("(2 + 3) * 4"), 20.0);
        QCOMPARE(store.evaluate("((1 + 2) * (3 + 4))"), 21.0);
    }

    void testNegativeNumbers()
    {
        document::ParameterStore store;
        QCOMPARE(store.evaluate("-5"), -5.0);
        QCOMPARE(store.evaluate("-5 + 3"), -2.0);
        QCOMPARE(store.evaluate("-(3 + 2)"), -5.0);
    }

    void testDecimalNumbers()
    {
        document::ParameterStore store;
        QCOMPARE(store.evaluate("3.14"), 3.14);
        QCOMPARE(store.evaluate(".5"), 0.5);
        QCOMPARE(store.evaluate("2."), 2.0);
    }

    // ---------------------------------------------------------------
    // Expression evaluation -- parameter references
    // ---------------------------------------------------------------
    void testParamRefInExpression()
    {
        document::ParameterStore store;
        store.set("width", "100");
        QCOMPARE(store.evaluate("width / 2 + 1"), 51.0);
    }

    void testMultipleParamRefs()
    {
        document::ParameterStore store;
        store.set("a", "10");
        store.set("b", "20");
        QCOMPARE(store.evaluate("a + b"), 30.0);
        QCOMPARE(store.evaluate("a * b"), 200.0);
    }

    void testNestedParamExpression()
    {
        document::ParameterStore store;
        store.set("width", "100");
        store.set("half_w", "width / 2");
        QCOMPARE(store.get("half_w"), 50.0);
        QCOMPARE(store.evaluate("(width + 10) * 2"), 220.0);
    }

    // ---------------------------------------------------------------
    // Expression evaluation -- functions
    // ---------------------------------------------------------------
    void testFunctionAbs()
    {
        document::ParameterStore store;
        QCOMPARE(store.evaluate("abs(-7)"), 7.0);
        QCOMPARE(store.evaluate("abs(3)"), 3.0);
    }

    void testFunctionSqrt()
    {
        document::ParameterStore store;
        QCOMPARE(store.evaluate("sqrt(9)"), 3.0);
        QCOMPARE(store.evaluate("sqrt(2)"), std::sqrt(2.0));
    }

    void testTrigFunctions()
    {
        document::ParameterStore store;
        QVERIFY(qFuzzyCompare(store.evaluate("sin(0)") + 1.0, 1.0));   // sin(0) == 0
        QCOMPARE(store.evaluate("cos(0)"), 1.0);
        QVERIFY(qFuzzyCompare(store.evaluate("tan(0)") + 1.0, 1.0));   // tan(0) == 0
    }

    void testFunctionMinMax()
    {
        document::ParameterStore store;
        QCOMPARE(store.evaluate("min(3, 7)"), 3.0);
        QCOMPARE(store.evaluate("max(3, 7)"), 7.0);
    }

    void testFunctionPow()
    {
        document::ParameterStore store;
        QCOMPARE(store.evaluate("pow(2, 10)"), 1024.0);
    }

    void testConstantPI()
    {
        document::ParameterStore store;
        QCOMPARE(store.evaluate("PI"), M_PI);
        QCOMPARE(store.evaluate("pi"), M_PI);
    }

    // ---------------------------------------------------------------
    // Dependent propagation
    // ---------------------------------------------------------------
    void testPropagation()
    {
        document::ParameterStore store;
        store.set("base", "10");
        store.set("derived", "base * 2");
        QCOMPARE(store.get("derived"), 20.0);

        // Changing base should propagate to derived
        store.set("base", "50");
        QCOMPARE(store.get("derived"), 100.0);
    }

    void testChainedPropagation()
    {
        document::ParameterStore store;
        store.set("a", "5");
        store.set("b", "a + 1");     // b = 6
        store.set("c", "b * 2");     // c = 12
        QCOMPARE(store.get("c"), 12.0);

        store.set("a", "10");        // b -> 11, c -> 22
        QCOMPARE(store.get("b"), 11.0);
        QCOMPARE(store.get("c"), 22.0);
    }

    // ---------------------------------------------------------------
    // Circular dependency detection
    // ---------------------------------------------------------------
    void testSelfReference()
    {
        document::ParameterStore store;
        store.set("x", "10");
        QVERIFY_EXCEPTION_THROWN(store.set("x", "x + 1"), std::runtime_error);
    }

    void testCircularTwoParams()
    {
        document::ParameterStore store;
        store.set("a", "10");
        store.set("b", "a + 1");
        // Now trying to make a depend on b creates a cycle: a -> b -> a
        QVERIFY_EXCEPTION_THROWN(store.set("a", "b + 1"), std::runtime_error);
    }

    void testCircularThreeParams()
    {
        document::ParameterStore store;
        store.set("x", "1");
        store.set("y", "x + 1");
        store.set("z", "y + 1");
        // z -> y -> x, trying x = z + 1 creates cycle
        QVERIFY_EXCEPTION_THROWN(store.set("x", "z + 1"), std::runtime_error);
    }

    void testWouldCreateCycle()
    {
        document::ParameterStore store;
        store.set("a", "10");
        store.set("b", "a + 1");
        QVERIFY(store.wouldCreateCycle("a", "b + 1"));
        QVERIFY(!store.wouldCreateCycle("a", "42"));
    }

    // ---------------------------------------------------------------
    // referencedParams
    // ---------------------------------------------------------------
    void testReferencedParams()
    {
        document::ParameterStore store;
        store.set("width", "100");
        store.set("height", "50");
        auto refs = store.referencedParams("width / 2 + height");
        QVERIFY(refs.count("width"));
        QVERIFY(refs.count("height"));
        QCOMPARE(refs.size(), size_t(2));
    }

    // ---------------------------------------------------------------
    // ExpressionUtil
    // ---------------------------------------------------------------
    void testParseDistanceExpr()
    {
        document::ParameterStore store;
        // plain number with mm suffix -> mm
        QCOMPARE(document::parseDistanceExpr("50 mm", store), 50.0);
        // cm -> mm conversion
        QCOMPARE(document::parseDistanceExpr("1 cm", store), 10.0);
        // inch -> mm conversion
        QCOMPARE(document::parseDistanceExpr("1 in", store), 25.4);
        // no suffix -> as-is
        QCOMPARE(document::parseDistanceExpr("25", store), 25.0);
    }

    void testParseAngleExpr()
    {
        document::ParameterStore store;
        // 180 deg -> PI rad
        QVERIFY(qFuzzyCompare(document::parseAngleExpr("180 deg", store), M_PI));
        // 1 rad -> 1
        QCOMPARE(document::parseAngleExpr("1 rad", store), 1.0);
    }

    void testParseDimensionWithParamExpr()
    {
        document::ParameterStore store;
        store.set("offset", "5");
        // "offset cm" -> 5 * 10 = 50 mm
        QCOMPARE(document::parseDistanceExpr("offset cm", store), 50.0);
    }

    // ---------------------------------------------------------------
    // Unit stripping inside the parser itself
    // ---------------------------------------------------------------
    void testUnitStrippingInParser()
    {
        document::ParameterStore store;
        // The parser strips unit suffixes after numeric literals
        QCOMPARE(store.evaluate("50 mm"), 50.0);
        QCOMPARE(store.evaluate("10 deg"), 10.0);
    }

    // ---------------------------------------------------------------
    // Division by zero
    // ---------------------------------------------------------------
    void testDivisionByZero()
    {
        document::ParameterStore store;
        QVERIFY_EXCEPTION_THROWN(store.evaluate("1 / 0"), std::runtime_error);
    }

    // ---------------------------------------------------------------
    // Empty expression
    // ---------------------------------------------------------------
    void testEmptyExpression()
    {
        document::ParameterStore store;
        QVERIFY_EXCEPTION_THROWN(store.evaluate(""), std::runtime_error);
    }
};

QTEST_MAIN(TestParameterStore)
#include "test_ParameterStore.moc"
