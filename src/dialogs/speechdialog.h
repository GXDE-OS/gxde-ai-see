// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <QString>

class QLabel;
class QPlainTextEdit;
class QProcess;
class QSlider;

/** Text-to-speech dialog with a rate control. */
class SpeechDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SpeechDialog(const QString &text, QWidget *parent = nullptr);
    ~SpeechDialog() override;

private:
    void start();
    void stop();

    QString m_text;
    QPlainTextEdit *m_preview = nullptr;
    QSlider *m_rate = nullptr;
    QLabel *m_status = nullptr;
    QProcess *m_process = nullptr;
};
