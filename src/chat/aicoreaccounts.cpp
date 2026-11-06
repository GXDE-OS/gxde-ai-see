// SPDX-FileCopyrightText: 2026 GXDE OS Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "aicoreaccounts.h"

#include "aicore.h"
#include "chattheme.h"
#include "config.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {

constexpr int kAccountRole = Qt::UserRole + 1;
const QString kCustom = QStringLiteral("custom");

QJsonObject findById(const QJsonArray &array, const QString &id)
{
    for (const QJsonValue &value : array) {
        if (value.toObject().value(QStringLiteral("id")).toString() == id) {
            return value.toObject();
        }
    }
    return {};
}

/** A preset's base URL is either one URL or a list of regional endpoints. */
QJsonArray endpointsOf(const QJsonObject &preset)
{
    const QJsonValue base = preset.value(QStringLiteral("baseUrl"));
    if (base.isArray()) {
        return base.toArray();
    }
    if (base.isString()) {
        return QJsonArray{ QJsonObject{ { QStringLiteral("url"), base.toString() } } };
    }
    return {};
}

QStringList modelIds(const QJsonArray &models)
{
    QStringList ids;
    for (const QJsonValue &value : models) {
        const QString id = value.toObject().value(QStringLiteral("id")).toString();
        if (!id.isEmpty()) {
            ids.append(id);
        }
    }
    return ids;
}

QString secondaryStyle()
{
    return QStringLiteral("color: %1; font-size: 12px;")
        .arg(ChatTheme::css(ChatTheme::palette().textSecondary));
}

} // namespace

/* ------------------------------------------------------------------------- */
/* Panel                                                                     */
/* ------------------------------------------------------------------------- */

AiCoreAccountsPanel::AiCoreAccountsPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_list = new QListWidget(this);
    m_list->setMinimumHeight(120);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_list);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(6);
    m_add = new QPushButton(tr("添加…"), this);
    m_edit = new QPushButton(tr("编辑…"), this);
    m_delete = new QPushButton(tr("删除"), this);
    m_activate = new QPushButton(tr("设为当前"), this);
    buttons->addWidget(m_add);
    buttons->addWidget(m_edit);
    buttons->addWidget(m_delete);
    buttons->addStretch();
    buttons->addWidget(m_activate);
    layout->addLayout(buttons);

    auto *modelRow = new QHBoxLayout;
    modelRow->setSpacing(6);
    modelRow->addWidget(new QLabel(tr("当前模型"), this));
    m_model = new QComboBox(this);
    modelRow->addWidget(m_model, 1);
    layout->addLayout(modelRow);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setStyleSheet(secondaryStyle());
    layout->addWidget(m_status);

    connect(m_add, &QPushButton::clicked, this, &AiCoreAccountsPanel::addAccount);
    connect(m_edit, &QPushButton::clicked, this, &AiCoreAccountsPanel::editAccount);
    connect(m_delete, &QPushButton::clicked, this, &AiCoreAccountsPanel::deleteAccount);
    connect(m_activate, &QPushButton::clicked, this, &AiCoreAccountsPanel::activateSelected);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &AiCoreAccountsPanel::editAccount);
    connect(m_list, &QListWidget::currentRowChanged, this, [this]() {
        const QJsonObject account = selectedAccount();
        const bool any = !account.isEmpty();
        const bool active = any && account.value(QStringLiteral("id")).toString() == m_activeId;
        m_edit->setEnabled(any);
        /* AI Core refuses to delete the account in use */
        m_delete->setEnabled(any && !active);
        m_activate->setEnabled(any && !active);
    });
    connect(m_model, &QComboBox::activated, this, [this]() {
        const QString model = m_model->currentText();
        if (m_activeId.isEmpty() || model.isEmpty()) {
            return;
        }
        setBusy(true, tr("正在切换模型…"));
        QPointer<AiCoreAccountsPanel> self(this);
        AiCore::instance()->activateAccount(m_activeId, model,
                                            [self](const QJsonValue &, const QString &error) {
                                                if (!self) {
                                                    return;
                                                }
                                                self->setBusy(false, error);
                                                self->reload();
                                            });
    });

    reload();
}

void AiCoreAccountsPanel::reload()
{
    setBusy(true, tr("正在读取 GXDE AI Core 账号…"));
    QPointer<AiCoreAccountsPanel> self(this);
    AiCore::instance()->listAccounts([self](const QJsonValue &result, const QString &error) {
        if (!self) {
            return;
        }
        if (!error.isEmpty()) {
            self->setBusy(false, tr("无法连接 GXDE AI Core：%1").arg(error));
            self->m_add->setEnabled(false);
            return;
        }
        const QJsonObject object = result.toObject();
        self->m_presets = object.value(QStringLiteral("presets")).toArray();
        self->m_accounts = object.value(QStringLiteral("accounts")).toArray();
        self->m_activeId = object.value(QStringLiteral("activeAccountId")).toString();
        self->setBusy(false);
        self->showAccounts();
    });
}

void AiCoreAccountsPanel::showAccounts()
{
    const QString selected = selectedAccount().value(QStringLiteral("id")).toString();
    m_list->clear();
    int selectRow = -1;
    for (const QJsonValue &value : std::as_const(m_accounts)) {
        const QJsonObject account = value.toObject();
        const QString id = account.value(QStringLiteral("id")).toString();
        const bool active = id == m_activeId;
        const QJsonObject preset =
            findById(m_presets, account.value(QStringLiteral("providerId")).toString());
        const QString provider = preset.isEmpty()
                                     ? tr("自定义")
                                     : preset.value(QStringLiteral("label")).toString();

        auto *item = new QListWidgetItem(
            QStringLiteral("%1%2\n%3 · %4")
                .arg(active ? QStringLiteral("✓ ") : QString(),
                     account.value(QStringLiteral("name")).toString(), provider,
                     account.value(QStringLiteral("model")).toString()),
            m_list);
        item->setToolTip(account.value(QStringLiteral("baseUrl")).toString());
        item->setData(kAccountRole, id);
        if (active) {
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
        }
        if (id == selected || (selected.isEmpty() && active)) {
            selectRow = m_list->count() - 1;
        }
    }
    m_list->setCurrentRow(selectRow >= 0 ? selectRow : (m_list->count() ? 0 : -1));
    if (m_list->count() == 0) {
        m_edit->setEnabled(false);
        m_delete->setEnabled(false);
        m_activate->setEnabled(false);
        m_status->setText(tr("还没有账号。点「添加…」选择 DeepSeek 等供应商并填入 API Key。"));
    } else if (m_activeId.isEmpty()) {
        m_status->setText(tr("还没有选中账号，请选一个账号后点「设为当前」。"));
    }

    const QJsonObject active = findById(m_accounts, m_activeId);
    m_model->clear();
    m_model->addItems(modelIds(active.value(QStringLiteral("models")).toArray()));
    m_model->setCurrentText(active.value(QStringLiteral("model")).toString());
    m_model->setEnabled(!active.isEmpty());
}

QJsonObject AiCoreAccountsPanel::selectedAccount() const
{
    const QListWidgetItem *item = m_list ? m_list->currentItem() : nullptr;
    return item ? findById(m_accounts, item->data(kAccountRole).toString()) : QJsonObject();
}

void AiCoreAccountsPanel::setBusy(bool busy, const QString &message)
{
    for (QWidget *widget : { static_cast<QWidget *>(m_add), static_cast<QWidget *>(m_list),
                             static_cast<QWidget *>(m_model) }) {
        widget->setEnabled(!busy);
    }
    if (busy) {
        m_edit->setEnabled(false);
        m_delete->setEnabled(false);
        m_activate->setEnabled(false);
    }
    m_status->setText(AiCore::friendlyError(message));
}

void AiCoreAccountsPanel::addAccount()
{
    runEditor(QJsonObject());
}

void AiCoreAccountsPanel::editAccount()
{
    const QJsonObject account = selectedAccount();
    if (!account.isEmpty()) {
        runEditor(account);
    }
}

void AiCoreAccountsPanel::runEditor(const QJsonObject &original)
{
    AiCoreAccountDialog dialog(m_presets, original, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QJsonObject account = dialog.account();
    const bool makeActive = m_activeId.isEmpty();
    setBusy(true, tr("正在保存账号…"));

    QPointer<AiCoreAccountsPanel> self(this);
    AiCore::instance()->saveAccount(
        account, dialog.apiKey(), [self, makeActive](const QJsonValue &result, const QString &error) {
            if (!self) {
                return;
            }
            if (!error.isEmpty()) {
                self->setBusy(false, tr("保存失败：%1").arg(error));
                self->reload();
                return;
            }
            const QJsonObject saved =
                result.toObject().value(QStringLiteral("account")).toObject();
            const QString id = saved.value(QStringLiteral("id")).toString();
            if (!makeActive || id.isEmpty()) {
                self->reload();
                return;
            }
            /* the first account is the one to use */
            AiCore::instance()->activateAccount(
                id, QString(), [self](const QJsonValue &, const QString &activateError) {
                    if (!self) {
                        return;
                    }
                    self->setBusy(false, activateError);
                    self->reload();
                });
        });
}

void AiCoreAccountsPanel::deleteAccount()
{
    const QJsonObject account = selectedAccount();
    const QString id = account.value(QStringLiteral("id")).toString();
    if (id.isEmpty() || id == m_activeId) {
        return;
    }
    setBusy(true, tr("正在删除账号…"));
    QPointer<AiCoreAccountsPanel> self(this);
    AiCore::instance()->deleteAccount(id, [self](const QJsonValue &, const QString &error) {
        if (!self) {
            return;
        }
        self->setBusy(false, error.isEmpty() ? QString() : tr("删除失败：%1").arg(error));
        self->reload();
    });
}

void AiCoreAccountsPanel::activateSelected()
{
    const QString id = selectedAccount().value(QStringLiteral("id")).toString();
    if (id.isEmpty()) {
        return;
    }
    setBusy(true, tr("正在切换账号…"));
    QPointer<AiCoreAccountsPanel> self(this);
    AiCore::instance()->activateAccount(id, QString(),
                                        [self](const QJsonValue &, const QString &error) {
                                            if (!self) {
                                                return;
                                            }
                                            self->setBusy(false, error);
                                            self->reload();
                                        });
}

void AiCoreAccountsPanel::describeActive(QObject *context,
                                         std::function<void(const QString &)> done)
{
    QPointer<QObject> guard(context);
    AiCore::instance()->currentAccount(
        [guard, done](const QJsonValue &result, const QString &error) {
            if (!guard) {
                return;
            }
            const QJsonObject account =
                result.toObject().value(QStringLiteral("account")).toObject();
            if (!error.isEmpty() || account.isEmpty()) {
                done(QString());
                return;
            }
            done(QStringLiteral("%1 · %2").arg(account.value(QStringLiteral("name")).toString(),
                                               account.value(QStringLiteral("model")).toString()));
        });
}

void AiCoreAccountsPanel::importLegacyKey()
{
    Config *config = Config::instance();
    const QString apiKey = config->get(QStringLiteral("chat/apiKey")).toString().trimmed();
    if (apiKey.isEmpty()) {
        return;
    }

    AiCore::instance()->listAccounts([apiKey](const QJsonValue &result, const QString &error) {
        if (!error.isEmpty()) {
            return; // AI Core unavailable: keep the key for a later try
        }
        Config *config = Config::instance();
        auto forgetKey = [config]() {
            /* from now on the key lives in AI Core's keychain only */
            config->remove(QStringLiteral("chat/apiKey"));
            config->remove(QStringLiteral("chat/baseUrl"));
            config->remove(QStringLiteral("chat/model"));
            config->sync();
        };
        const QJsonObject list = result.toObject();
        if (!list.value(QStringLiteral("accounts")).toArray().isEmpty()) {
            forgetKey();
            return;
        }

        QString baseUrl = config->get(QStringLiteral("chat/baseUrl"),
                                      QStringLiteral("https://api.deepseek.com"))
                              .toString()
                              .trimmed();
        while (baseUrl.endsWith(QLatin1Char('/'))) {
            baseUrl.chop(1);
        }
        if (baseUrl.endsWith(QStringLiteral("/chat/completions"))) {
            baseUrl.chop(int(qstrlen("/chat/completions")));
        }
        const QString model =
            config->get(QStringLiteral("chat/model"), QStringLiteral("deepseek-flash")).toString();

        const QJsonObject preset =
            findById(list.value(QStringLiteral("presets")).toArray(), QStringLiteral("deepseek"));
        const bool isDeepSeek = baseUrl.contains(QStringLiteral("deepseek.com"));
        QJsonArray models = isDeepSeek ? preset.value(QStringLiteral("models")).toArray()
                                       : QJsonArray();
        if (!modelIds(models).contains(model)) {
            models.append(QJsonObject{ { QStringLiteral("id"), model } });
        }

        const QJsonObject account{
            { QStringLiteral("name"), isDeepSeek ? QStringLiteral("DeepSeek") : tr("小埃同学") },
            { QStringLiteral("providerId"), isDeepSeek ? QStringLiteral("deepseek") : kCustom },
            { QStringLiteral("protocol"), QStringLiteral("openai") },
            { QStringLiteral("baseUrl"), baseUrl },
            { QStringLiteral("models"), models },
            { QStringLiteral("model"), model },
        };
        AiCore::instance()->saveAccount(
            account, apiKey, [forgetKey](const QJsonValue &saved, const QString &saveError) {
                if (!saveError.isEmpty()) {
                    return;
                }
                const QString id = saved.toObject()
                                       .value(QStringLiteral("account"))
                                       .toObject()
                                       .value(QStringLiteral("id"))
                                       .toString();
                AiCore::instance()->activateAccount(
                    id, QString(), [forgetKey](const QJsonValue &, const QString &activateError) {
                        if (activateError.isEmpty()) {
                            forgetKey();
                        }
                    });
            });
    });
}

/* ------------------------------------------------------------------------- */
/* Editor                                                                    */
/* ------------------------------------------------------------------------- */

AiCoreAccountDialog::AiCoreAccountDialog(const QJsonArray &presets, const QJsonObject &account,
                                         QWidget *parent)
    : QDialog(parent)
    , m_presets(presets)
    , m_original(account)
    , m_editing(!account.isEmpty())
{
    setWindowTitle(m_editing ? tr("编辑账号") : tr("添加账号"));
    setMinimumWidth(480);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;

    m_preset = new QComboBox(this);
    for (const QJsonValue &value : presets) {
        const QJsonObject preset = value.toObject();
        m_preset->addItem(preset.value(QStringLiteral("label")).toString(),
                          preset.value(QStringLiteral("id")).toString());
    }
    m_preset->addItem(tr("自定义（OpenAI 兼容 / Anthropic / Gemini）"), kCustom);
    form->addRow(tr("供应商"), m_preset);

    m_endpoint = new QComboBox(this);
    form->addRow(tr("区域 / 套餐"), m_endpoint);

    m_protocol = new QComboBox(this);
    m_protocol->addItem(tr("OpenAI 兼容（Chat Completions）"), QStringLiteral("openai"));
    m_protocol->addItem(tr("Anthropic Messages"), QStringLiteral("anthropic"));
    m_protocol->addItem(tr("Gemini API"), QStringLiteral("gemini"));
    form->addRow(tr("协议"), m_protocol);

    m_baseUrl = new QLineEdit(this);
    m_baseUrl->setPlaceholderText(QStringLiteral("https://api.example.com/v1"));
    form->addRow(tr("API 地址"), m_baseUrl);

    m_name = new QLineEdit(this);
    m_name->setPlaceholderText(tr("例如：DeepSeek 个人"));
    form->addRow(tr("账号名称"), m_name);

    m_key = new QLineEdit(this);
    m_key->setEchoMode(QLineEdit::Password);
    m_key->setPlaceholderText(m_editing ? tr("留空则保留原有 Key") : QStringLiteral("sk-…"));
    form->addRow(tr("API Key"), m_key);

    m_models = new QLineEdit(this);
    m_models->setPlaceholderText(tr("多个模型用英文逗号分隔"));
    form->addRow(tr("模型列表"), m_models);

    m_defaultModel = new QComboBox(this);
    m_defaultModel->setEditable(true);
    form->addRow(tr("默认模型"), m_defaultModel);

    layout->addLayout(form);

    m_docs = new QLabel(this);
    m_docs->setOpenExternalLinks(true);
    m_docs->setStyleSheet(secondaryStyle());
    layout->addWidget(m_docs);

    auto *hint = new QLabel(tr("账号和 API Key 由 GXDE AI Core 保存在系统密钥环中，"
                               "与 gxde-ai-core 命令行共用。"),
                            this);
    hint->setWordWrap(true);
    hint->setStyleSheet(secondaryStyle());
    layout->addWidget(hint);

    m_error = new QLabel(this);
    m_error->setWordWrap(true);
    m_error->setStyleSheet(QStringLiteral("color: #d9534f; font-size: 12px;"));
    m_error->hide();
    layout->addWidget(m_error);

    auto *buttons =
        new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &AiCoreAccountDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_preset, &QComboBox::currentIndexChanged, this, &AiCoreAccountDialog::presetChanged);
    connect(m_endpoint, &QComboBox::currentIndexChanged, this,
            &AiCoreAccountDialog::endpointChanged);
    connect(m_models, &QLineEdit::textChanged, this, &AiCoreAccountDialog::modelsChanged);

    if (m_editing) {
        const int index = m_preset->findData(account.value(QStringLiteral("providerId")).toString());
        {
            const QSignalBlocker blocker(m_preset);
            m_preset->setCurrentIndex(index >= 0 ? index : m_preset->count() - 1);
        }
        presetChanged();
        /* the account's own values win over the preset defaults */
        const QString baseUrl = account.value(QStringLiteral("baseUrl")).toString();
        const int endpoint = m_endpoint->findData(baseUrl);
        if (endpoint >= 0) {
            m_endpoint->setCurrentIndex(endpoint);
        }
        m_baseUrl->setText(baseUrl);
        m_protocol->setCurrentIndex(
            qMax(0, m_protocol->findData(account.value(QStringLiteral("protocol")).toString())));
        m_name->setText(account.value(QStringLiteral("name")).toString());
        m_models->setText(
            modelIds(account.value(QStringLiteral("models")).toArray()).join(QStringLiteral(", ")));
        m_defaultModel->setCurrentText(account.value(QStringLiteral("model")).toString());
    } else {
        const int deepseek = m_preset->findData(QStringLiteral("deepseek"));
        m_preset->setCurrentIndex(deepseek >= 0 ? deepseek : 0);
        presetChanged();
    }
}

QJsonObject AiCoreAccountDialog::preset() const
{
    return findById(m_presets, m_preset->currentData().toString());
}

void AiCoreAccountDialog::presetChanged()
{
    const QJsonObject current = preset();
    const bool custom = current.isEmpty();

    m_protocol->setEnabled(custom);
    if (!custom) {
        m_protocol->setCurrentIndex(
            qMax(0, m_protocol->findData(current.value(QStringLiteral("protocol")).toString())));
    }

    {
        const QSignalBlocker blocker(m_endpoint);
        m_endpoint->clear();
        for (const QJsonValue &value : endpointsOf(current)) {
            const QJsonObject endpoint = value.toObject();
            const QString url = endpoint.value(QStringLiteral("url")).toString();
            const QString label = endpoint.value(QStringLiteral("label")).toString();
            m_endpoint->addItem(label.isEmpty() ? url : label, url);
            m_endpoint->setItemData(m_endpoint->count() - 1,
                                    endpoint.value(QStringLiteral("documentationUrl")).toString(),
                                    Qt::UserRole + 1);
        }
    }
    m_endpoint->setEnabled(m_endpoint->count() > 1);
    m_endpoint->setVisible(!custom);
    if (custom) {
        m_baseUrl->clear();
        m_docs->clear();
    } else {
        endpointChanged();
    }

    if (!m_editing || m_name->text().isEmpty()) {
        m_name->setText(custom ? QString() : current.value(QStringLiteral("label")).toString());
    }
    m_models->setText(
        modelIds(current.value(QStringLiteral("models")).toArray()).join(QStringLiteral(", ")));
}

void AiCoreAccountDialog::endpointChanged()
{
    const int index = m_endpoint->currentIndex();
    if (index < 0) {
        return;
    }
    m_baseUrl->setText(m_endpoint->itemData(index).toString());
    QString docs = m_endpoint->itemData(index, Qt::UserRole + 1).toString();
    m_docs->setText(docs.isEmpty() ? QString()
                                   : tr("<a href='%1'>如何获取 API Key</a>").arg(docs.toHtmlEscaped()));
}

void AiCoreAccountDialog::modelsChanged()
{
    const QString current = m_defaultModel->currentText();
    const QStringList ids = m_models->text().split(QLatin1Char(','), Qt::SkipEmptyParts);
    m_defaultModel->clear();
    for (const QString &id : ids) {
        if (!id.trimmed().isEmpty()) {
            m_defaultModel->addItem(id.trimmed());
        }
    }
    const int index = m_defaultModel->findText(current);
    m_defaultModel->setCurrentIndex(index >= 0 ? index : 0);
}

QJsonObject AiCoreAccountDialog::account() const
{
    const QJsonObject current = preset();
    const QJsonArray presetModels = current.value(QStringLiteral("models")).toArray();
    const QJsonArray ownModels = m_original.value(QStringLiteral("models")).toArray();

    /* keep what AI Core knows about a model (context window, reasoning …) */
    QJsonArray models;
    for (const QString &raw : m_models->text().split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString id = raw.trimmed();
        if (id.isEmpty() || modelIds(models).contains(id)) {
            continue;
        }
        QJsonObject model = findById(ownModels, id);
        if (model.isEmpty()) {
            model = findById(presetModels, id);
        }
        if (model.isEmpty()) {
            model.insert(QStringLiteral("id"), id);
        }
        models.append(model);
    }

    QJsonObject account{
        { QStringLiteral("name"), m_name->text().trimmed() },
        { QStringLiteral("providerId"), current.isEmpty()
                                            ? kCustom
                                            : current.value(QStringLiteral("id")).toString() },
        { QStringLiteral("protocol"), m_protocol->currentData().toString() },
        { QStringLiteral("baseUrl"), m_baseUrl->text().trimmed() },
        { QStringLiteral("models"), models },
        { QStringLiteral("model"), m_defaultModel->currentText().trimmed() },
    };
    if (m_editing) {
        account.insert(QStringLiteral("id"), m_original.value(QStringLiteral("id")));
    }
    return account;
}

QString AiCoreAccountDialog::apiKey() const
{
    return m_key->text().trimmed();
}

void AiCoreAccountDialog::accept()
{
    const QJsonObject result = account();
    QString problem;
    const QUrl url(result.value(QStringLiteral("baseUrl")).toString());
    const QStringList models = modelIds(result.value(QStringLiteral("models")).toArray());
    if (result.value(QStringLiteral("name")).toString().isEmpty()) {
        problem = tr("请填写账号名称");
    } else if (!url.isValid() ||
               (url.scheme() != QStringLiteral("https") && url.scheme() != QStringLiteral("http")) ||
               url.host().isEmpty() || url.hasQuery() || url.hasFragment()) {
        problem = tr("API 地址需要是 http(s) 地址，且不能带查询参数");
    } else if (!m_editing && apiKey().isEmpty()) {
        problem = tr("请填写 API Key");
    } else if (models.isEmpty()) {
        problem = tr("至少需要一个模型");
    } else if (!models.contains(result.value(QStringLiteral("model")).toString())) {
        problem = tr("默认模型必须在模型列表里");
    }
    if (!problem.isEmpty()) {
        m_error->setText(problem);
        m_error->show();
        return;
    }
    QDialog::accept();
}
