/********************************************************************
    Copyright (c) 2013-2014 - QSanguosha-Hegemony Team

  This file is part of QSanguosha-Hegemony.

  This game is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 3.0 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  See the LICENSE file for more details.

  QSanguosha-Hegemony Team
*********************************************************************/

#include "rule-summary.h"
#include "engine.h"
#include "scenario.h"

#include <QListWidget>
#include <QTextEdit>
#include <QHBoxLayout>
#include <QFile>
#include <QTextStream>

RuleSummary::RuleSummary(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Rule Summary"));
    resize(800, 600);

    list = new QListWidget;
    list->setMaximumWidth(100);

    content_box = new QTextEdit;
    content_box->setReadOnly(true);
    content_box->setProperty("description", true);

    QHBoxLayout *layout = new QHBoxLayout;

    layout->addWidget(content_box);
    layout->addWidget(list);

    setLayout(layout);

    QStringList names = Sanguosha->getModScenarioNames();
    names << "Hegemony";
    foreach (QString name, names) {
        QString text = Sanguosha->translate(name);
        QListWidgetItem *item = new QListWidgetItem(text, list);
        item->setData(Qt::UserRole, name);
    }

    connect(list, SIGNAL(currentRowChanged(int)), this, SLOT(loadContent(int)));

    if (!names.isEmpty())
        loadContent(0);
}

void RuleSummary::loadContent(int row) {
    QString name = list->item(row)->data(Qt::UserRole).toString();
    QString filename = QString("scenarios/%1.html").arg(name);
    QFile file(filename);
    if (file.open(QIODevice::ReadOnly)) {
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        QString content = stream.readAll();
        content_box->setHtml(content);
    }
}

