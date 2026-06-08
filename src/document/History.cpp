#include "History.h"

EditHistory::EditHistory(QObject *parent)
	: QObject(parent)
	, savedAction(nullptr)
	, reservedAction(false)
{
}

EditHistory::~EditHistory()
{
	Clear();
}

void EditHistory::Clear()
{
	ClearFutureActions();
	ClearPastActions();
}

void EditHistory::MarkAbsolutelyDirty()
{
	savedAction = reinterpret_cast<EditAction*>(1);
	emit OnHistoryChanged();
}

void EditHistory::MarkClean()
{
	if (undoActions.empty()){
		savedAction = nullptr;
	}else{
		savedAction = undoActions.back().get();
	}
	emit OnHistoryChanged();
}

void EditHistory::SetReservedAction(bool exists)
{
	reservedAction = exists;
	emit OnHistoryChanged();
}

bool EditHistory::IsDirty() const
{
	if (undoActions.empty()){
		return savedAction != nullptr || reservedAction;
	}else{
		return savedAction != undoActions.back().get() || reservedAction;
	}
}

void EditHistory::ClearFutureActions()
{
	redoActions.clear();
}

void EditHistory::ClearPastActions()
{
	undoActions.clear();
}

void EditHistory::Add(EditAction *action)
{
	ClearFutureActions();
	undoActions.push_back(std::unique_ptr<EditAction>(action));
	emit OnHistoryChanged();
}

bool EditHistory::CanUndo(QString *out_name) const
{
	if (undoActions.empty())
		return false;
	if (out_name){
		*out_name = undoActions.back()->GetName();
	}
	return true;
}

bool EditHistory::CanRedo(QString *out_name) const
{
	if (redoActions.empty())
		return false;
	if (out_name){
		*out_name = redoActions.back()->GetName();
	}
	return true;
}

void EditHistory::Undo()
{
    //
    // TODO: Return errors instead of exceptions.
    //

	if (undoActions.empty())
		return;
	try{
		EditAction *action = undoActions.back().get();
		action->Undo();
		action->Show();
		redoActions.push_back(std::move(undoActions.back()));
		undoActions.pop_back();
		emit OnHistoryChanged();
	}catch(EditActionException &e){
		//---
		throw;
	}
}

void EditHistory::Redo()
{
    //
    // TODO: Return errors instead of exceptions.
    //

	if (redoActions.empty())
		return;
	try{
		EditAction *action = redoActions.back().get();
		action->Redo();
		action->Show();
		undoActions.push_back(std::move(redoActions.back()));
		redoActions.pop_back();
		emit OnHistoryChanged();
	}catch(EditActionException &e){
		throw;
	}
}

