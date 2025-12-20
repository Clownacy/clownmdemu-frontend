#ifndef DIALOGS_COMMON_H
#define DIALOGS_COMMON_H

#include <QCheckBox>
#include <QRadioButton>

namespace Dialogs::Common
{
	template<typename T>
	inline void BindRadioButton(QWidget* const parent, QRadioButton* const radiobutton, T &variable, const T value)
	{
		radiobutton->setChecked(variable == value);
		QObject::connect(radiobutton, &QRadioButton::toggled, parent, [&variable, value](const bool enabled){ if (enabled) variable = value; });
	};

	inline void BindCheckBox(QWidget* const parent, QCheckBox* const checkbox, auto &boolean)
	{
		checkbox->setChecked(boolean);
		QObject::connect(checkbox, &QCheckBox::checkStateChanged, parent, [&boolean](const bool enabled){boolean = enabled;});
	};

	inline void BindInverseCheckBox(QWidget* const parent, QCheckBox* const checkbox, auto &boolean)
	{
		checkbox->setChecked(!boolean);
		QObject::connect(checkbox, &QCheckBox::checkStateChanged, parent, [&](const bool enabled){boolean = !enabled;});
	};
}

#endif // DIALOGS_COMMON_H
