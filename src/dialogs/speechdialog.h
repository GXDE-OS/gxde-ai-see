// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <DDialog>
#include <DLabel>
#include <dslider.h>
#include <dtextedit.h>
#include <QString>

class QProcess;

/** Text-to-speech dialog with a rate control (DTK dialog). */
class SpeechDialog : public Dtk::Widget::DDialog
{
    Q_OBJECT

public:
    explicit SpeechDialog(const QString &text, QWidget *parent = nullptr);
    ~SpeechDialog() override;

private:
    void start();
    void stop();

    QString m_text;
    Dtk::Widget::DTextEdit *m_preview = nullptr;
    Dtk::Widget::DSlider *m_rate = nullptr;
    Dtk::Widget::DLabel *m_status = nullptr;
    QProcess *m_process = nullptr;
};
