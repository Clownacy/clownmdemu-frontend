#ifndef ALLOCATED_DIALOG_H
#define ALLOCATED_DIALOG_H

#include <memory>

#include <QDialog>
#include <QObject>

template<typename T>
class AllocatedDialog
{
private:
	std::unique_ptr<T> menu;

public:
	template<typename... Ts>
	void Open(QWidget* const parent, Ts &&...args)
	{
		menu = std::make_unique<T>(std::forward<Ts>(args)..., parent);
		menu->show();
		QObject::connect(&*menu, &QDialog::finished, parent, [&](){menu = nullptr;});
	}

	T& operator*(this auto &&self)
	{
		return *self.menu;
	}

	T* operator->(this auto &&self)
	{
		return &*self;
	}
};

#endif // ALLOCATED_DIALOG_H
