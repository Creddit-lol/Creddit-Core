// Copyright (c) 2019-2020 The CREDD developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MASTERNODESWIDGET_H
#define MASTERNODESWIDGET_H

#include "qt/creddit/pwidget.h"
#include "qt/creddit/furabstractlistitemdelegate.h"
#include "qt/creddit/mnmodel.h"
#include "qt/creddit/tooltipmenu.h"
#include "walletmodel.h"

#include <atomic>

#include <QTimer>
#include <QWidget>

class CREDDGUI;

namespace Ui {
class MasterNodesWidget;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

class MasterNodesWidget : public PWidget
{
    Q_OBJECT

public:

    explicit MasterNodesWidget(CREDDGUI *parent = nullptr);
    ~MasterNodesWidget();

    void loadWalletModel() override;

    void run(int type) override;
    void onError(QString error, int type) override;

    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private Q_SLOTS:
    void onCreateMNClicked();
    void onStartAllClicked(int type);
    void changeTheme(bool isLightTheme, QString &theme) override;
    void onMNClicked(const QModelIndex &index);
    void onEditMNClicked();
    void onDeleteMNClicked();
    void onInfoMNClicked();
    void updateListState();
    void updateModelAndInform(QString informText);

private:
    Ui::MasterNodesWidget *ui;
    FurAbstractListItemDelegate *delegate;
    MNModel *mnModel = nullptr;
    TooltipMenu* menu = nullptr;
    QModelIndex index;
    QTimer *timer = nullptr;

    std::atomic<bool> isLoading;

    bool checkMNsNetwork();
    void startAlias(QString strAlias);
    bool startAll(QString& failedMN, bool onlyMissing);
    bool startMN(const CMasternodeConfig::CMasternodeEntry& mne, std::string& strError);
};

#endif // MASTERNODESWIDGET_H
