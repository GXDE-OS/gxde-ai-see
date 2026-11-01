// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "speechdialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QProcess>
#include <QStandardPaths>
#include <QVBoxLayout>

#include <DLabel>
#include <dslider.h>
#include <dtextedit.h>

SpeechDialog::SpeechDialog(const QString &text, QWidget *parent)
    : Dtk::Widget::DDialog(parent)
    , m_text(text)
{
    setTitle(tr("朗读"));
    setAttribute(Qt::WA_DeleteOnClose);

    auto *content = new QWidget;
    content->setMinimumWidth(420);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    m_preview = new Dtk::Widget::DTextEdit(content);
    m_preview->setReadOnly(true);
    m_preview->setPlainText(text.left(4000));
    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    const QColor baseColor = dark ? QColor(38, 38, 42) : QColor(255, 255, 255);
    const QColor textColor = dark ? QColor(235, 235, 235) : QColor(30, 30, 30);
    m_preview->setStyleSheet(
        QStringLiteral("QTextEdit { color: %1; background-color: %2;"
                       " border: 1px solid %3; border-radius: 6px; padding: 6px; }")
            .arg(textColor.name(), baseColor.name(),
                 dark ? QStringLiteral("rgba(255,255,255,0.14)")
                      : QStringLiteral("rgba(0,0,0,0.12)")));
    m_preview->setMinimumHeight(200);
    layout->addWidget(m_preview, 1);

    auto *rateRow = new QHBoxLayout;
    rateRow->addWidget(new Dtk::Widget::DLabel(tr("语速"), content));
    m_rate = new Dtk::Widget::DSlider(Qt::Horizontal, content);
    m_rate->setRange(50, 200);
    m_rate->setValue(100);
    rateRow->addWidget(m_rate, 1);
    layout->addLayout(rateRow);

    m_status = new Dtk::Widget::DLabel(content);
    m_status->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(m_status);

    setContentLayoutContentsMargins(QMargins(16, 12, 16, 4));
    addContent(content);

    const int playButton = addButton(tr("开始朗读"), true, ButtonRecommend);
    const int stopButton = addButton(tr("停止"));
    const int closeButton = addButton(tr("关闭"));
    setOnButtonClickedClose(false);
    connect(this, &Dtk::Widget::DDialog::buttonClicked, this,
            [this, playButton, stopButton, closeButton](int index, const QString &) {
                if (index == playButton) {
                    start();
                } else if (index == stopButton) {
                    stop();
                } else if (index == closeButton) {
                    close();
                }
            });
}

SpeechDialog::~SpeechDialog()
{
    stop();
}

void SpeechDialog::start()
{
    stop();
    QString program = QStandardPaths::findExecutable(QStringLiteral("spd-say"));
    QStringList args;
    if (!program.isEmpty()) {
        args = { QStringLiteral("-r"), QString::number(m_rate->value() - 100),
                 QStringLiteral("-w"), m_text.left(4000) };
    } else {
        program = QStandardPaths::findExecutable(QStringLiteral("espeak-ng"));
        if (program.isEmpty()) {
            m_status->setText(tr("未找到 spd-say 或 espeak-ng"));
            return;
        }
        args = { QStringLiteral("-s"), QString::number(m_rate->value()),
                 m_text.left(4000) };
    }

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        m_status->setText(tr("朗读失败：%1").arg(m_process->errorString()));
    });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int, QProcess::ExitStatus) {
                m_status->setText(tr("朗读结束"));
            });
    m_status->setText(tr("正在朗读…"));
    m_process->start(program, args);
}

void SpeechDialog::stop()
{
    if (m_process) {
        m_process->kill();
        m_process->waitForFinished(300);
        m_process->deleteLater();
        m_process = nullptr;
        m_status->setText(tr("已停止"));
    }
}
