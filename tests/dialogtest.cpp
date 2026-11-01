// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Headless smoke test for the DTK content-factory dialogs (segmentation and
// speech), including screenshots for manual inspection.

#include <QAbstractButton>
#include <QApplication>
#include <QFile>
#include <QTextEdit>
#include <QTextStream>

#include <DDialog>
#include <dslider.h>

#include "dialogs/segmentdialog.h"
#include "dialogs/speechdialog.h"

static int failures = 0;

static void check(bool condition, const QString &what)
{
    QTextStream out(stdout);
    out << (condition ? "PASS " : "FAIL ") << what << "\n";
    out.flush();
    if (!condition) {
        ++failures;
    }
}

static bool hasButton(QWidget *widget, const QString &text)
{
    /* DDialog inserts a thin space into two-character button labels. */
    const auto normalized = [](const QString &value) {
        QString result;
        for (const QChar &character : value) {
            if (!character.isSpace()) {
                result.append(character);
            }
        }
        return result;
    };
    const QString wanted = normalized(text);
    for (QAbstractButton *button : widget->findChildren<QAbstractButton *>()) {
        if (normalized(button->text()) == wanted) {
            return true;
        }
    }
    return false;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("GXDE"));
    app.setApplicationName(QStringLiteral("gxde-ai-see"));

    SegmentDialog segment({ QStringLiteral("小埃"), QStringLiteral("同学"),
                            QStringLiteral("北京"), QStringLiteral("天安门") });
    segment.show();
    QApplication::processEvents();
    check(qobject_cast<Dtk::Widget::DDialog *>(&segment) != nullptr,
          "segment dialog is a DTK DDialog");
    check(hasButton(&segment, QStringLiteral("全选")), "segment has 全选 button");
    check(hasButton(&segment, QStringLiteral("清空")), "segment has 清空 button");
    check(hasButton(&segment, QStringLiteral("复制选中")), "segment has 复制选中 button");
    check(hasButton(&segment, QStringLiteral("关闭")), "segment has 关闭 button");
    check(segment.grab().save(QStringLiteral("/tmp/gxde-ai-see-segment.png")),
          "segment dialog rendered");

    SpeechDialog speech(QString(120, QChar(0x5C0F)) + QStringLiteral(" 测试朗读"));
    speech.show();
    QApplication::processEvents();
    check(qobject_cast<Dtk::Widget::DDialog *>(&speech) != nullptr,
          "speech dialog is a DTK DDialog");
    check(speech.findChild<QTextEdit *>() != nullptr, "speech has a DTextEdit preview");
    check(speech.findChild<Dtk::Widget::DSlider *>() != nullptr,
          "speech has a DSlider");
    check(hasButton(&speech, QStringLiteral("开始朗读")), "speech has 开始朗读 button");
    check(hasButton(&speech, QStringLiteral("停止")), "speech has 停止 button");
    check(hasButton(&speech, QStringLiteral("关闭")), "speech has 关闭 button");
    check(speech.grab().save(QStringLiteral("/tmp/gxde-ai-see-speech.png")),
          "speech dialog rendered");

    QTextStream(stdout) << (failures == 0 ? "ALL PASS\n" : "FAILURES\n");
    return failures == 0 ? 0 : 1;
}
