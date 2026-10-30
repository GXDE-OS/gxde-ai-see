// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "speechdialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSlider>
#include <QStandardPaths>
#include <QVBoxLayout>

SpeechDialog::SpeechDialog(const QString &text, QWidget *parent)
    : QDialog(parent)
    , m_text(text)
{
    setWindowTitle(tr("朗读"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(480, 380);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    m_preview = new QPlainTextEdit(this);
    m_preview->setReadOnly(true);
    m_preview->setPlainText(text.left(4000));
    m_preview->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: rgba(255,255,255,0.05); color: rgba(255,255,255,0.9);"
        " border: none; border-radius: 8px; padding: 8px; }"));
    layout->addWidget(m_preview, 1);

    auto *rateRow = new QHBoxLayout;
    rateRow->addWidget(new QLabel(tr("语速"), this));
    m_rate = new QSlider(Qt::Horizontal, this);
    m_rate->setRange(50, 200);
    m_rate->setValue(100);
    rateRow->addWidget(m_rate, 1);
    layout->addLayout(rateRow);

    m_status = new QLabel(this);
    m_status->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.55); font-size: 12px;"));
    layout->addWidget(m_status);

    auto *buttons = new QHBoxLayout;
    auto *play = new QPushButton(tr("开始朗读"), this);
    play->setStyleSheet(QStringLiteral(
        "QPushButton { color: white; background: rgba(74,144,255,0.85); border: none;"
        " border-radius: 8px; padding: 7px 16px; }"
        "QPushButton:hover { background: rgba(90,160,255,0.95); }"));
    auto *stopButton = new QPushButton(tr("停止"), this);
    auto *close = new QPushButton(tr("关闭"), this);
    buttons->addWidget(play);
    buttons->addWidget(stopButton);
    buttons->addStretch();
    buttons->addWidget(close);
    layout->addLayout(buttons);

    connect(play, &QPushButton::clicked, this, &SpeechDialog::start);
    connect(stopButton, &QPushButton::clicked, this, &SpeechDialog::stop);
    connect(close, &QPushButton::clicked, this, &QDialog::close);
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
