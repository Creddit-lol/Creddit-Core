// Copyright (c) 2019 The CREDD developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SETTINGSWALLETOPTIONSWIDGET_H
#define SETTINGSWALLETOPTIONSWIDGET_H

#include <QWidget>
#include <QDataWidgetMapper>
#include "qt/creddit/pwidget.h"
namespace Ui {
class SettingsWalletOptionsWidget;
}

class SettingsWalletOptionsWidget : public PWidget
{
    Q_OBJECT

public:
    explicit SettingsWalletOptionsWidget(CREDDGUI* _window, QWidget *parent = nullptr);
    ~SettingsWalletOptionsWidget();

    void setMapper(QDataWidgetMapper *mapper);

    void discardWalletOnlyOptions();
    bool saveWalletOnlyOptions();

Q_SIGNALS:
    void saveSettings();
    void discardSettings();

public Q_SLOTS:
    void onResetClicked();

private:
    Ui::SettingsWalletOptionsWidget *ui;

    void loadWalletModel() override;
    void reloadWalletOptions();

    void setSpinBoxStakeSplitThreshold(double val);
    double getSpinBoxStakeSplitThreshold() const;
};

#endif // SETTINGSWALLETOPTIONSWIDGET_H
