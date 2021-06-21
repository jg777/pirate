// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "sendcoinsentry.h"
#include "ui_sendcoinsentry.h"

#include "addressbookpage.h"
#include "addresstablemodel.h"
#include "zaddressbookpage.h"
#include "zaddresstablemodel.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "platformstyle.h"
#include "walletmodel.h"

#include "key_io.h"

#include <QApplication>
#include <QClipboard>

SendCoinsEntry::SendCoinsEntry(const PlatformStyle *_platformStyle, QWidget *parent, bool allowZAddresses) :
    QStackedWidget(parent),
    ui(new Ui::SendCoinsEntry),
    model(0),
    platformStyle(_platformStyle),
    _allowZAddresses(allowZAddresses)
{
    ui->setupUi(this);

    ui->addressBookButton->setIcon(platformStyle->SingleColorIcon(":/icons/address-book"));
    ui->pasteButton->setIcon(platformStyle->SingleColorIcon(":/icons/editpaste"));
    ui->deleteButton->setIcon(platformStyle->SingleColorIcon(":/icons/remove"));
    ui->deleteButton_is->setIcon(platformStyle->SingleColorIcon(":/icons/remove"));
    ui->deleteButton_s->setIcon(platformStyle->SingleColorIcon(":/icons/remove"));
    ui->useAvailableBalanceButton->setIcon(platformStyle->SingleColorIcon(":/icons/all_balance"));

    //hide addressbook, needs work...
    ui->addressBookButton->hide();

    setCurrentWidget(ui->SendCoins);

    if (platformStyle->getUseExtraSpacing())
        ui->payToLayout->setSpacing(4);
#if QT_VERSION >= 0x040700
    ui->addAsLabel->setPlaceholderText(tr("Enter a label for this address to add it to your address book (only for taddrs)"));
#endif

    // normal komodo address field
    GUIUtil::setupAddressWidget(ui->payTo, this, _allowZAddresses);
    // just a label for displaying komodo address(es)
    ui->payTo_is->setFont(GUIUtil::fixedPitchFont());

    // Connect signals
    connect(ui->payAmount, SIGNAL(valueChanged()), this, SIGNAL(payAmountChanged()));
    connect(ui->checkboxSubtractFeeFromAmount, SIGNAL(toggled(bool)), this, SIGNAL(subtractFeeFromAmountChanged()));
    connect(ui->deleteButton, SIGNAL(clicked()), this, SLOT(deleteClicked()));
    connect(ui->deleteButton_is, SIGNAL(clicked()), this, SLOT(deleteClicked()));
    connect(ui->deleteButton_s, SIGNAL(clicked()), this, SLOT(deleteClicked()));
    connect(ui->useAvailableBalanceButton, SIGNAL(clicked()), this, SLOT(useAvailableBalanceClicked()));
    connect(ui->memo,SIGNAL(textEdited()),this,SLOT(showHexError()));
    connect(ui->radioEncodeHex,SIGNAL(clicked()),this,SLOT(showHexError()));
    connect(ui->radioEncodeString,SIGNAL(clicked()),this,SLOT(showHexError()));

    ui->messageLabel->setObjectName("messageLabel");
    ui->messageTextLabel->setObjectName("messageTextLabel");
}

SendCoinsEntry::~SendCoinsEntry()
{
    delete ui;
}

void SendCoinsEntry::on_pasteButton_clicked()
{
    // Paste text from clipboard into recipient field
    ui->payTo->setText(QApplication::clipboard()->text());
}

void SendCoinsEntry::on_addressBookButton_clicked()
{
    // if(!model)
    //     return;
    // ZAddressBookPage dlg(platformStyle, ZAddressBookPage::ForSelection, ZAddressBookPage::SendingTab, this);
    // dlg.setModel(model->getZAddressTableModel());
    // if(dlg.exec())
    // {
    //     ui->payTo->setText(dlg.getReturnValue());
    //     ui->payAmount->setFocus();
    // }
}

void SendCoinsEntry::on_payTo_textChanged(const QString &address)
{
    updateLabel(address);
}

void SendCoinsEntry::setModel(WalletModel *_model)
{
    this->model = _model;

    if (_model && _model->getOptionsModel()) {
        connect(_model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
        connect(_model->getOptionsModel(), SIGNAL(optionHexMemo(bool)), this, SLOT(updateHexMemo()));
    }

    clear();
}

void SendCoinsEntry::clear()
{
    // clear UI elements for normal payment
    ui->payTo->clear();
    ui->addAsLabel->clear();
    ui->payAmount->clear();
    ui->checkboxSubtractFeeFromAmount->setCheckState(Qt::Unchecked);
    ui->messageTextLabel->clear();
    ui->messageTextLabel->hide();
    ui->messageLabel->hide();
    ui->addAsLabel->clear();
    ui->addAsLabel->hide();
    ui->labellLabel->hide();
    ui->memo->clear();
    // clear UI elements for unauthenticated payment request
    ui->payTo_is->clear();
    ui->memoTextLabel_is->clear();
    ui->payAmount_is->clear();
    // clear UI elements for authenticated payment request
    ui->payTo_s->clear();
    ui->memoTextLabel_s->clear();
    ui->payAmount_s->clear();

    // update the display unit, to not use the default ("KMD")
    updateDisplayUnit();
    updateHexMemo();
}

void SendCoinsEntry::checkSubtractFeeFromAmount()
{
    ui->checkboxSubtractFeeFromAmount->setChecked(true);
}

bool SendCoinsEntry::getSubmitMemoAsHex() {
    if (ui->radioEncodeHex->isChecked()) {
        return true;
    }
    return false;
}

void SendCoinsEntry::showHexError()
{
    ui->messageLabel->setVisible(false);
    ui->messageTextLabel->setVisible(false);

    if (!ui->radioEncodeHex->isChecked()) {
        return;
    }

    if (!model) {
        return;
    }

    QString memo = ui->memo->text();
    if (memo.length() > 0) {
        if (!IsHex(memo.toStdString())) {
            ui->messageLabel->setVisible(true);
            ui->messageTextLabel->setVisible(true);
            ui->messageTextLabel->setText("memo is not a valid hex string!!!");
        }
    }
}

void SendCoinsEntry::hideCheckboxSubtractFeeFromAmount()
{
    ui->checkboxSubtractFeeFromAmount->hide();
}

void SendCoinsEntry::deleteClicked()
{
    Q_EMIT removeEntry(this);
}

void SendCoinsEntry::useAvailableBalanceClicked()
{
    Q_EMIT useAvailableBalance(this);
}

bool SendCoinsEntry::validate(bool allowZAddresses)
{
    if (!model)
        return false;

    // Check input validity
    bool retval = true;

    #ifdef ENABLE_BIP70
    // Skip checks for payment request
    if (recipient.paymentRequest.IsInitialized())
        return retval;
    #endif

    if (!model->validateAddress(ui->payTo->text(), allowZAddresses))
    {
        ui->payTo->setValid(false);
        retval = false;
    }

    if (!ui->payAmount->validate())
    {
        retval = false;
    }

    //Validate memo is encoded as hex if be submitted as hex
    if (ui->radioEncodeHex->isChecked()) {
      QString memo = ui->memo->text();
      if (memo.length() > 0) {
          if (!IsHex(memo.toStdString())) {
              retval = false;
          }
      }
    }

    //Validate memo
    if(!model->validateMemo(ui->memo->text())) {
        retval = false;
    }

    // Sending a zero amount is invalid
    if (ui->payAmount->value(0) <= 0)
    {
        ui->payAmount->setValid(false);
        retval = false;
    }

    // Reject dust outputs:
    if (IsValidDestinationString(ui->payTo->text().toStdString()))
    {
        if (retval && GUIUtil::isDust(ui->payTo->text(), ui->payAmount->value())) {
            ui->payAmount->setValid(false);
            retval = false;
        }
    }

    return retval;
}

SendCoinsRecipient SendCoinsEntry::getValue()
{
    #ifdef ENABLE_BIP70
    // Payment request
    if (recipient.paymentRequest.IsInitialized())
        return recipient;
    #endif

    // Normal payment
    recipient.address = ui->payTo->text();
    recipient.label = ui->addAsLabel->text();
    recipient.amount = ui->payAmount->value();
    recipient.memo = ui->memo->text();
    recipient.message = ui->messageTextLabel->text();
    recipient.fSubtractFeeFromAmount = (ui->checkboxSubtractFeeFromAmount->checkState() == Qt::Checked);

    return recipient;
}

QWidget *SendCoinsEntry::setupTabChain(QWidget *prev)
{
    QWidget::setTabOrder(prev, ui->payTo);
    QWidget::setTabOrder(ui->payTo, ui->addAsLabel);
    QWidget *w = ui->payAmount->setupTabChain(ui->addAsLabel);
    QWidget::setTabOrder(w, ui->checkboxSubtractFeeFromAmount);
    QWidget::setTabOrder(ui->checkboxSubtractFeeFromAmount, ui->addressBookButton);
    QWidget::setTabOrder(ui->addressBookButton, ui->pasteButton);
    QWidget::setTabOrder(ui->pasteButton, ui->deleteButton);
    return ui->deleteButton;
}

void SendCoinsEntry::setValue(const SendCoinsRecipient &value)
{
    recipient = value;

    #ifdef ENABLE_BIP70
    if (recipient.paymentRequest.IsInitialized()) // payment request
    {
        if (recipient.authenticatedMerchant.isEmpty()) // unauthenticated
        {
            ui->payTo_is->setText(recipient.address);
            ui->memoTextLabel_is->setText(recipient.message);
            ui->payAmount_is->setValue(recipient.amount);
            ui->payAmount_is->setReadOnly(true);
            setCurrentWidget(ui->SendCoins_UnauthenticatedPaymentRequest);
        }
        else // authenticated
        {
            ui->payTo_s->setText(recipient.authenticatedMerchant);
            ui->memoTextLabel_s->setText(recipient.message);
            ui->payAmount_s->setValue(recipient.amount);
            ui->payAmount_s->setReadOnly(true);
            setCurrentWidget(ui->SendCoins_AuthenticatedPaymentRequest);
        }
    }
    else // normal payment
    #endif
    {
        // message
        ui->messageTextLabel->setText(recipient.message);
        ui->messageTextLabel->setVisible(!recipient.message.isEmpty());
        ui->messageLabel->setVisible(!recipient.message.isEmpty());

        ui->addAsLabel->clear();
        ui->payTo->setText(recipient.address); // this may set a label from addressbook
        if (!recipient.label.isEmpty()) // if a label had been set from the addressbook, don't overwrite with an empty label
            ui->addAsLabel->setText(recipient.label);
        ui->payAmount->setValue(recipient.amount);
    }
}

void SendCoinsEntry::setAddress(const QString &address)
{
    ui->payTo->setText(address);
    ui->payAmount->setFocus();
}

void SendCoinsEntry::setAmount(const CAmount &amount)
{
    ui->payAmount->setValue(amount);
}

bool SendCoinsEntry::isClear()
{
    return ui->payTo->text().isEmpty() && ui->payTo_is->text().isEmpty() && ui->payTo_s->text().isEmpty();
}

void SendCoinsEntry::setFocus()
{
    ui->payTo->setFocus();
}

void SendCoinsEntry::updateDisplayUnit()
{
    if(model && model->getOptionsModel())
    {
        // Update payAmount with the current unit
        ui->payAmount->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
        ui->payAmount_is->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
        ui->payAmount_s->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
    }
}

void SendCoinsEntry::updateHexMemo()
{
    if(model && model->getOptionsModel())
    {
        bool hexEnabled = model->getOptionsModel()->getHexMemo();
        if (!hexEnabled) {
              ui->radioEncodeHex->setChecked(false);
              ui->radioEncodeString->setChecked(true);
        }

        // Update payAmount with the current unit
        ui->encodingFrame->setVisible(hexEnabled);

    }
}

bool SendCoinsEntry::updateLabel(const QString &address)
{
    if(!model)
        return false;

    // Fill in label from address book, if address has an associated label
    QString associatedLabel = model->getAddressTableModel()->labelForAddress(address);
    if(!associatedLabel.isEmpty())
    {
        ui->addAsLabel->setText(associatedLabel);
        return true;
    }

    return false;
}
