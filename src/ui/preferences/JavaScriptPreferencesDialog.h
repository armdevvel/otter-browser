/**************************************************************************
* Otter Browser: Web browser controlled by the user, not vice-versa.
* Copyright (C) 2014 Jan Bajer aka bajasoft <jbajer@gmail.com>
* Copyright (C) 2015 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
**************************************************************************/

#ifndef OTTER_JAVASCRIPTPREFERENCESDIALOG_H
#define OTTER_JAVASCRIPTPREFERENCESDIALOG_H

#include <QtWidgets/QDialog>

namespace Otter
{

namespace Ui
{
	class JavaScriptPreferencesDialog;
}

class JavaScriptPreferencesDialog : public QDialog
{
	Q_OBJECT

public:
	explicit JavaScriptPreferencesDialog(const QVariantMap &options, QWidget *parent = 0);
	~JavaScriptPreferencesDialog();

	QVariantMap getOptions() const;

protected:
	void changeEvent(QEvent *event);

private:
	Ui::JavaScriptPreferencesDialog *m_ui;
};

}

#endif
