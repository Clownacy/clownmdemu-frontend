#ifndef ALLOCATED_DIALOG_H
#define ALLOCATED_DIALOG_H

#include <memory>

#include <QDialog>
#include <QObject>

template<typename T>
class AllocatedDialog : private std::unique_ptr<T>
{
private:
	using Base = std::unique_ptr<T>;

public:
	using Base::operator*;
	using Base::operator->;

	template<typename... Ts>
	void Open(QWidget* const parent, Ts &&...args)
	{
		Base &base = *this;
		base = std::make_unique<T>(std::forward<Ts>(args)..., parent);
		base->show();
		QObject::connect(&*base, &QDialog::finished, parent, [&](){base = nullptr;});
	}
};

#endif // ALLOCATED_DIALOG_H
