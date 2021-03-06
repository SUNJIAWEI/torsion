/* Torsion - http://torsionim.org/
 * Copyright (C) 2010, John Brooks <john.brooks@dereferenced.net>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *
 *    * Neither the names of the copyright owners nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ManualConfigPage.h"
#include "TorConnTestWidget.h"
#include <QBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRegExpValidator>
#include <QIntValidator>
#include <QPushButton>
#include <QVariant>

using namespace TorConfig;

ManualConfigPage::ManualConfigPage(QWidget *parent)
    : QWizardPage(parent)
{
    setButtonText(QWizard::CustomButton1, tr("Verify Connection"));

    QBoxLayout *layout = new QVBoxLayout(this);

    QLabel *desc = new QLabel;
    desc->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    desc->setWordWrap(true);
    desc->setTextFormat(Qt::RichText);
    desc->setText(tr(
        "Torsion requires a Tor controller connection instead of a normal proxy connection. "
        "This is configured with the <i>ControlPort</i> and <i>HashedControlPassword</i> options in the "
        "Tor configuration. You must set these options in your Tor configuration, and input them here."
    ));

    layout->addWidget(desc);
    layout->addSpacing(20);

    QFormLayout *formLayout = new QFormLayout;
    layout->addLayout(formLayout);

    /* Test widget */
    torTest = new TorConnTestWidget;
    connect(torTest, SIGNAL(stateChanged()), this, SIGNAL(completeChanged()));

    /* IP */
    ipEdit = new QLineEdit;
    ipEdit->setWhatsThis(tr("The IP of the Tor control connection"));

    QRegExpValidator *validator = new QRegExpValidator(QRegExp(QLatin1String(
            "^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$")),
            ipEdit);
    ipEdit->setValidator(validator);

    registerField(QLatin1String("controlIp*"), ipEdit);
    formLayout->addRow(tr("Control IP"), ipEdit);

    connect(ipEdit, SIGNAL(textChanged(QString)), torTest, SLOT(clear()));

    /* Port */
    portEdit = new QLineEdit;
    portEdit->setValidator(new QIntValidator(1, 65535, portEdit));
    portEdit->setWhatsThis(tr("The port used for the Tor control connection (ControlPort option)"));

    registerField(QLatin1String("controlPort*"), portEdit);
    formLayout->addRow(tr("Control Port"), portEdit);

    connect(portEdit, SIGNAL(textChanged(QString)), torTest, SLOT(clear()));

    /* Password */
    QLineEdit *passwordEdit = new QLineEdit;
    passwordEdit->setWhatsThis(tr("The password for control authentication. Plaintext of the "
                                  "HashedControlPassword option in Tor."));

    registerField(QLatin1String("controlPassword"), passwordEdit);
    formLayout->addRow(tr("Control Password"), passwordEdit);

    connect(passwordEdit, SIGNAL(textChanged(QString)), torTest, SLOT(clear()));

    /* Tester */
    QBoxLayout *testLayout = new QHBoxLayout;

    testLayout->addWidget(torTest, 1, Qt::AlignVCenter | Qt::AlignLeft);

    QPushButton *testBtn = new QPushButton(tr("Test Connection"));
    testLayout->addWidget(testBtn, 0, Qt::AlignVCenter | Qt::AlignRight);

    connect(testBtn, SIGNAL(clicked()), this, SLOT(testSettings()));

    layout->addStretch();
    layout->addLayout(testLayout);
    layout->addStretch();
}

void ManualConfigPage::initializePage()
{
    ipEdit->setText(QLatin1String("127.0.0.1"));
    portEdit->setText(QLatin1String("9051"));
}

bool ManualConfigPage::isComplete() const
{
    return torTest->hasTestSucceeded();
}

void ManualConfigPage::testSettings()
{
    torTest->startTest(field(QLatin1String("controlIp")).toString(),
                       field(QLatin1String("controlPort")).toString().toInt(),
                       field(QLatin1String("controlPassword")).toString().toLocal8Bit());
}
