#ifndef DIALOGS_COMMON_H
#define DIALOGS_COMMON_H

#include <QCheckBox>
#include <QRadioButton>

#define DIALOGS_COMMON_BIND_RADIO_BUTTON(RADIO_BUTTON, OPTIONS, OPTION, VALUE) \
	(RADIO_BUTTON)->setChecked((OPTIONS).Get##OPTION() == VALUE); \
	QObject::connect(RADIO_BUTTON, &QRadioButton::toggled, this, [&OPTIONS](const bool enabled){ if (enabled) (OPTIONS).Set##OPTION(VALUE); })

#define DIALOGS_COMMON_BIND_CHECK_BOX(CHECK_BOX, OPTIONS, OPTION) \
	(CHECK_BOX)->setChecked((OPTIONS).Get##OPTION()); \
	QObject::connect(CHECK_BOX, &QCheckBox::checkStateChanged, parent, [&OPTIONS](const bool enabled){(OPTIONS).Set##OPTION(enabled);})

#define DIALOGS_COMMON_BIND_INVERSE_CHECK_BOX(CHECK_BOX, OPTIONS, OPTION) \
	(CHECK_BOX)->setChecked(!(OPTIONS).Get##OPTION()); \
	QObject::connect(CHECK_BOX, &QCheckBox::checkStateChanged, parent, [&OPTIONS](const bool enabled){(OPTIONS).Set##OPTION(!enabled);})

#endif // DIALOGS_COMMON_H
