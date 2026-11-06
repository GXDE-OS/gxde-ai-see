// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QWidget>

#include <functional>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

/**
 * Settings front-end for the provider accounts of GXDE AI Core.
 *
 * The accounts and their API keys belong to AI Core (shared with the
 * gxde-ai-core command line); this widget only lists, edits and selects them
 * over ACP.
 */
class AiCoreAccountsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AiCoreAccountsPanel(QWidget *parent = nullptr);

    /** "account · model" of the active account, empty without one. */
    static void describeActive(QObject *context, std::function<void(const QString &)> done);

    /**
     * Move the API key older versions kept in config.ini into an AI Core
     * account, once, when AI Core has no account yet.
     */
    static void importLegacyKey();

private:
    void reload();
    void showAccounts();
    void addAccount();
    void editAccount();
    void deleteAccount();
    void activateSelected();
    void runEditor(const QJsonObject &account);
    void setBusy(bool busy, const QString &message = QString());
    QJsonObject selectedAccount() const;

    QListWidget *m_list = nullptr;
    QPushButton *m_add = nullptr;
    QPushButton *m_edit = nullptr;
    QPushButton *m_delete = nullptr;
    QPushButton *m_activate = nullptr;
    QComboBox *m_model = nullptr;
    QLabel *m_status = nullptr;

    QJsonArray m_presets;
    QJsonArray m_accounts;
    QString m_activeId;
};

/** Add or edit one provider account. */
class AiCoreAccountDialog : public QDialog
{
    Q_OBJECT

public:
    /** \a account empty adds a new one. */
    AiCoreAccountDialog(const QJsonArray &presets, const QJsonObject &account,
                        QWidget *parent = nullptr);

    QJsonObject account() const;
    /** Empty when editing and the key should stay as it is. */
    QString apiKey() const;

    void accept() override;

private:
    void presetChanged();
    void endpointChanged();
    void modelsChanged();
    QJsonObject preset() const;

    QJsonArray m_presets;
    QJsonObject m_original;
    bool m_editing = false;

    QComboBox *m_preset = nullptr;
    QComboBox *m_endpoint = nullptr;
    QComboBox *m_protocol = nullptr;
    QLineEdit *m_name = nullptr;
    QLineEdit *m_baseUrl = nullptr;
    QLineEdit *m_key = nullptr;
    QLineEdit *m_models = nullptr;
    QComboBox *m_defaultModel = nullptr;
    QLabel *m_docs = nullptr;
    QLabel *m_error = nullptr;
};
