#ifndef HISTORY_H
#define HISTORY_H

#include <QObject>
#include <QString>
#include <memory>
#include <vector>


class EditActionException
{
public:
	QString message;
};


class EditAction
{
public:
	virtual ~EditAction(){}
	virtual void Undo(){} // [DONE >> UNDONE]
	virtual void Redo(){} // [UNDONE >> DONE]
	virtual QString GetName(){ return QString(); }
	virtual void Show(){}
};




class EditHistory : public QObject
{
	Q_OBJECT

private:
	// The stacks own their actions (move-only unique_ptr; std::vector is used
	// because Qt containers don't support move-only value types).
	std::vector<std::unique_ptr<EditAction>> undoActions;
	std::vector<std::unique_ptr<EditAction>> redoActions;
	EditAction *savedAction; // non-owning marker for clean/dirty tracking
	bool reservedAction;

	void ClearFutureActions();
	void ClearPastActions();

public:
	EditHistory(QObject *parent=nullptr);
	virtual ~EditHistory();

	void Clear();
	void MarkAbsolutelyDirty();
	void MarkClean();
	void SetReservedAction(bool exists);
	bool IsDirty() const;
	void Add(EditAction *action); // `action` must be in DONE state
	bool CanUndo(QString *out_name=nullptr) const;
	bool CanRedo(QString *out_name=nullptr) const;

public slots:
	void Undo();
	void Redo();

signals:
	void OnHistoryChanged();

};


#endif // HISTORY_H
