
#include "SequenceView.h"
#include "SequenceViewContexts.h"
#include "SequenceViewInternal.h"
#include "../util/UIDef.h"

SequenceView::WriteModeContext::WriteModeContext(SequenceView *sview)
	: Context(sview)
{
}

SequenceView::WriteModeContext::~WriteModeContext()
{
}

/*
SequenceView::Context *SequenceView::WriteModeContext::Enter(QEnterEvent *)
{
	return this;
}

SequenceView::Context *SequenceView::WriteModeContext::Leave(QEnterEvent *)
{
	return this;
}
*/
SequenceView::Context *SequenceView::WriteModeContext::PlayingPane_MouseMove(QMouseEvent *event)
{
	qreal time = sview->Y2Time(event->y());
	int iTime = time;
	int lane = sview->X2Lane(event->x());
	if (sview->snapToGrid){
		iTime = sview->SnapToLowerFineGrid(iTime);
	}
	QList<SoundNoteView *> notes;
	bool conflicts = false;
	NoteConflict conf;
	if (lane >= 0)
		notes = sview->HitTestPlayingPaneMulti(lane, event->y(), iTime, event->modifiers() & Qt::AltModifier, &conflicts, &conf);
	if (notes.size() > 0){
		// Write mode can't drag notes (that's Edit mode), so don't imply it with
		// a move cursor — hovering a note just lets you select/delete it.
		sview->playingPane->setCursor(Qt::ArrowCursor);
		if (conflicts){
			sview->cursor->SetLayeredSoundNotesWithConflict(notes, conf);
		}else{
			sview->cursor->SetExistingSoundNote(notes[0]);
		}
	}else if (lane >= 0){
		sview->playingPane->setCursor(Qt::CrossCursor);
		sview->cursor->SetNewSoundNote(SoundNote(iTime, lane, 0, sview->DefaultNewNoteType(event->modifiers() & Qt::ShiftModifier)));
	}else{
		sview->playingPane->setCursor(Qt::ArrowCursor);
		sview->cursor->SetNothing();
	}
	return this;
}

SequenceView::Context *SequenceView::WriteModeContext::PlayingPane_MousePress(QMouseEvent *event)
{
	qreal time = sview->Y2Time(event->y());
	int iTime = time;
	int lane = sview->X2Lane(event->x());
	if (sview->snapToGrid){
		iTime = sview->SnapToLowerFineGrid(iTime);
	}
	QList<SoundNoteView *> notes;
	bool conflicts = false;
	NoteConflict conf;
	if (lane >= 0)
		notes = sview->HitTestPlayingPaneMulti(lane, event->y(), iTime, event->modifiers() & Qt::AltModifier, &conflicts, &conf);

	sview->ClearBpmEventsSelection();
	if (event->button() == Qt::RightButton && (event->modifiers() & Qt::AltModifier)){
		sview->ClearNotesSelection();
		return new PreviewContext(this, sview, event->pos(), event->button(), iTime);
	}
	if (notes.size() > 0){
		switch (event->button()){
            case Qt::LeftButton:
                // select note
                if (event->modifiers() & Qt::ControlModifier){
                    for (auto note : notes)
                        sview->ToggleNoteSelection(note);
                }else{
                    QSet<SoundNoteView *> noteSet(notes.begin(), notes.end());
                    if (sview->selectedNotes.intersects(noteSet)){
                        // don't deselect other notes
                    }else{
                        sview->ClearNotesSelection();
                    }
                    for (auto note : notes)
                        sview->SelectAdditionalNote(note);
                }
                if (conflicts){
                    sview->cursor->SetLayeredSoundNotesWithConflict(notes, conf);
                }else{
                    sview->cursor->SetExistingSoundNote(notes[0]);
                }
                if ((event->modifiers() & Qt::ControlModifier) == 0){
                    sview->ClearChannelSelection();
                }
                for (auto note : notes){
                    sview->SetCurrentChannel(note->GetChannelView(), true);
                }
                sview->PreviewMultiNote(notes);
                break;
            case Qt::RightButton:
            {
                // if single note in the current channel, delete it
                // otherwise, simply select them
                auto note = notes[0]->GetNote();
                bool handled = false;
                if (notes.size() == 1 && notes[0]->GetChannelView()->IsCurrent()){
                    SoundChannel *c = notes[0]->GetChannelView()->GetChannel();
                    if (sview->groupedBgmView && note.lane > 0 && note.noteType == 0){
                        // Grouped-BGM: un-key a keyed sample START instead of
                        // deleting it, so the music is preserved (sample -> BGM).
                        handled = c->InsertNote(SoundNote(note.location, 0, 0, note.noteType));
                    }else{
                        // A cut (continuation, noteType 1) or a normal note is
                        // removed outright — removing a cut heals the slice so the
                        // previous note reclaims the sample.
                        handled = c->RemoveNote(note);
                    }
                }
                if (handled)
                {
                    sview->ClearNotesSelection();
                    sview->cursor->SetNewSoundNote(note);
                }else{
                    sview->ClearNotesSelection();
                    for (auto note : notes)
                        sview->SelectAdditionalNote(note);
                    if (conflicts){
                        sview->cursor->SetLayeredSoundNotesWithConflict(notes, conf);
                    }else{
                        sview->cursor->SetExistingSoundNote(notes[0]);
                    }
                    if ((event->modifiers() & Qt::ControlModifier) == 0){
                        sview->ClearChannelSelection();
                    }
                    for (auto note : notes){
                        sview->SetCurrentChannel(note->GetChannelView(), true);
                    }
                }
                break;
            }
            case Qt::MiddleButton:
                // preview only one channel
                sview->ClearNotesSelection();
                sview->SetCurrentChannel(notes[0]->GetChannelView());
                return new PreviewContext(this, sview, event->pos(), event->button(), iTime);
            default:
                break;
		}
	}else{
		if (sview->currentChannel >= 0 && lane > 0 && iTime >= 0 && event->button() == Qt::LeftButton){
			int noteType;
			if (sview->groupedBgmView){
				// Grouped-BGM (pre-cut) charting never adds new sound — a placed
				// note only ever "takes from" the sample the BGM has at this time.
				int target = sview->FindSampleChannelAtTime(iTime);
				if (target >= 0){
					// a sample has a note exactly here -> key it (InsertNote will
					// relocate that note onto the key lane); preserve its noteType.
					noteType = sview->soundChannels[target]->GetChannel()->GetNotes().value(iTime).noteType;
				}else{
					// no exact note: take from the sample still sounding here as a
					// continuation (cut). If nothing is sounding, register nothing.
					target = sview->FindSoundingSampleChannelAtTime(iTime);
					if (target < 0){
						sview->cursor->SetNewSoundNote(SoundNote(iTime, lane, 0, 0));
						return this; // outside every sample's playback window
					}
					noteType = 1; // continuation
				}
				sview->SetCurrentChannel(sview->soundChannels[target], true);
			}else{
				noteType = sview->DefaultNewNoteType(event->modifiers() & Qt::ShiftModifier);
			}
			// insert note (maybe moving existing note)
			SoundNote note(iTime, lane, 0, noteType);
			if (sview->soundChannels[sview->currentChannel]->GetChannel()->InsertNote(note)){
				// select the note
				const QMap<int, SoundNoteView*> &notes = sview->soundChannels[sview->currentChannel]->GetNotes();
				sview->SelectSingleNote(notes[iTime]);
				sview->PreviewSingleNote(notes[iTime]);
				sview->cursor->SetExistingSoundNote(notes[iTime]);
				sview->timeLine->update();
				sview->playingPane->update();
				for (auto cview : sview->soundChannels){
					cview->update();
				}

				// Enter Draw-Note Mode
				return new WriteModeDrawNoteContext(this, sview, iTime, event->pos());

			}else{
				// note was not created
				qApp->beep();
				sview->cursor->SetNewSoundNote(note);
			}
		}else if (event->button() == Qt::RightButton && sview->groupedBgmView && iTime >= 0 && lane >= 0){
			// Right-click on an empty spot in Classic BMS mode: split the sounding
			// sample at this tick — the remainder returns to the background as a
			// continuation note, so a keyed note above the split only triggers the
			// first part while the total audio stays identical (charting never
			// alters the music). Clicking exactly on an existing split heals it;
			// clicking a legacy x_stop marker clears the stop (un-silences the
			// remainder) instead.
			int ch = sview->FindSoundingSampleChannelAtTime(iTime);
			if (ch >= 0){
				SoundChannel *channel = sview->soundChannels[ch]->GetChannel();
				const auto &ns = channel->GetNotes();
				int startLoc = -1;
				for (auto it = ns.begin(); it != ns.end() && it.key() <= iTime; ++it)
					if (it.value().noteType == 0)
						startLoc = it.key();
				if (startLoc >= 0){
					bool handled = false;
					SoundNote start = ns.value(startLoc);
					if (start.stop > 0 && startLoc + start.stop == iTime){
						start.stop = 0;
						handled = channel->InsertNote(start); // undoable in-place update
					}else if (ns.contains(iTime) && ns[iTime].noteType == 1 && ns[iTime].lane == 0){
						handled = channel->RemoveNote(ns[iTime]); // heal the split
					}else if (!ns.contains(iTime)){
						handled = channel->InsertNote(SoundNote(iTime, 0, 0, 1));
					}
					if (handled){
						sview->playingPane->update();
						for (auto cv : sview->soundChannels)
							cv->update();
					}
				}
			}
		}
	}
	return this;
}

SequenceView::Context *SequenceView::WriteModeContext::PlayingPane_MouseRelease(QMouseEvent *)
{
	return this;
}

SequenceView::Context *SequenceView::WriteModeContext::MeasureArea_MouseMove(QMouseEvent *event)
{
	qreal time = sview->Y2Time(event->y());
	int iTime = time;
	if (sview->snapToGrid){
		iTime = sview->SnapToLowerFineGrid(iTime);
	}
	if (event->modifiers() & Qt::ControlModifier){
		// edit bar lines
		const auto bars = sview->document->GetBarLines();
		int hitTime = iTime;
		if ((event->modifiers() & Qt::AltModifier) == 0){
			// Alt to bypass absorption
			auto i = bars.upperBound(iTime);
			if (i != bars.begin()){
				i--;
				if (i != bars.end() && sview->Time2Y(i.key()) - 16 <= event->y()){
					hitTime = i.key();
				}
			}
		}
		if (bars.contains(hitTime)){
			sview->timeLine->setCursor(Qt::ArrowCursor);
			sview->cursor->SetExistingBarLine(bars[hitTime]);
		}else{
			sview->timeLine->setCursor(Qt::ArrowCursor);
			sview->cursor->SetNewBarLine(BarLine(iTime, 0));
		}
	}else{
		// just show time
		sview->timeLine->setCursor(Qt::ArrowCursor);
		sview->cursor->SetTime(iTime);
	}
	return this;
}

SequenceView::Context *SequenceView::WriteModeContext::MeasureArea_MousePress(QMouseEvent *event)
{
	qreal time = sview->Y2Time(event->y());
	int iTime = time;
	if (sview->snapToGrid){
		iTime = sview->SnapToLowerFineGrid(iTime);
	}
	if (event->modifiers() & Qt::ControlModifier){
		// edit bar lines
		const auto bars = sview->document->GetBarLines();
		int hitTime = iTime;
		if ((event->modifiers() & Qt::AltModifier) == 0){
			// Alt to bypass absorption
			auto i = bars.upperBound(iTime);
			if (i != bars.begin()){
				i--;
				if (i != bars.end() && sview->Time2Y(i.key()) - 16 <= event->y()){
					hitTime = i.key();
				}
			}
		}
		sview->ClearNotesSelection();
		sview->ClearBpmEventsSelection();
		if (bars.contains(hitTime)){
			switch (event->button()){
			case Qt::LeftButton: {
				// update one
				BarLine bar = bars[hitTime];
				bar.Ephemeral = false;
				sview->document->InsertBarLine(bar);
				sview->cursor->SetExistingBarLine(bar);
				break;
			}
			case Qt::RightButton: {
				// delete one
				sview->document->RemoveBarLine(hitTime);
				//sview->cursor->SetNewBarLine(BarLine(time, 0));
				sview->cursor->SetTime(iTime);
				break;
			}
			default:
				break;
			}
		}else{
			if (event->button() == Qt::LeftButton){
				// insert one
				sview->document->InsertBarLine(BarLine(iTime, 0));
			}
		}
	}else{
		sview->ClearNotesSelection();
		sview->ClearBpmEventsSelection();
	}
	return this;
}

SequenceView::Context *SequenceView::WriteModeContext::MeasureArea_MouseRelease([[maybe_unused]] QMouseEvent *event)
{
	return this;
}


SequenceView::Context *SequenceView::WriteModeContext::BpmArea_MouseMove(QMouseEvent *event){
	qreal time = sview->Y2Time(event->y());
	int iTime = time;
	if (sview->snapToGrid){
		iTime = sview->SnapToLowerFineGrid(iTime);
	}
	const auto events = sview->document->GetBpmEvents();
	int hitTime = iTime;
	if ((event->modifiers() & Qt::AltModifier) == 0){
		// Alt to bypass absorption
		auto i = events.upperBound(iTime);
		if (i != events.begin()){
			i--;
			if (i != events.end() && sview->Time2Y(i.key()) - 16 <= event->y()){
				hitTime = i.key();
			}
		}
	}
	if (events.contains(hitTime)){
		sview->timeLine->setCursor(Qt::ArrowCursor);
		sview->cursor->SetExistingBpmEvent(events[hitTime]);
	}else{
		auto i = events.upperBound(iTime);
		double bpm = i==events.begin() ? sview->document->GetInfo()->GetInitBpm() : (i-1)->value;
		sview->timeLine->setCursor(Qt::ArrowCursor);
		sview->cursor->SetNewBpmEvent(BpmEvent(iTime, bpm));
	}
	return this;
}

SequenceView::Context *SequenceView::WriteModeContext::BpmArea_MousePress(QMouseEvent *event){
	qreal time = sview->Y2Time(event->y());
	int iTime = time;
	if (sview->snapToGrid){
		iTime = sview->SnapToLowerFineGrid(iTime);
	}
	const auto events = sview->document->GetBpmEvents();
	int hitTime = iTime;
	if ((event->modifiers() & Qt::AltModifier) == 0){
		// Alt to bypass absorption
		auto i = events.upperBound(iTime);
		if (i != events.begin()){
			i--;
			if (i != events.end() && sview->Time2Y(i.key()) - 16 <= event->y()){
				hitTime = i.key();
			}
		}
	}
	sview->ClearNotesSelection();
	if (events.contains(hitTime)){
		switch (event->button()){
		case Qt::LeftButton:
			// edit one
			if (event->modifiers() & Qt::ControlModifier){
				sview->ToggleBpmEventSelection(events[hitTime]);
			}else{
				sview->SelectSingleBpmEvent(events[hitTime]);
			}
			break;
		case Qt::RightButton: {
			// delete one
			if (sview->document->RemoveBpmEvent(hitTime)){
				//auto i = events.upperBound(time);
				//double bpm = i==events.begin() ? document->GetInfo()->GetInitBpm() : (i-1)->value;
				//sview->cursor->SetNewBpmEvent(BpmEvent(time, bpm));
				sview->cursor->SetTime(iTime);
				sview->ClearBpmEventsSelection();
			}
			break;
		}
		default:
			break;
		}
	}else{
		if (event->button() == Qt::LeftButton){
			// add event
			auto i = events.upperBound(iTime);
			double bpm = i==events.begin() ? sview->document->GetInfo()->GetInitBpm() : (i-1)->value;
			BpmEvent event(iTime, bpm);
			if (sview->document->InsertBpmEvent(event)){
				sview->cursor->SetExistingBpmEvent(event);
				sview->SelectSingleBpmEvent(event);
			}
		}
	}
	return this;
}

SequenceView::Context *SequenceView::WriteModeContext::BpmArea_MouseRelease(QMouseEvent *){
	return this;
}







SequenceView::WriteModeDrawNoteContext::WriteModeDrawNoteContext(SequenceView::WriteModeContext *parent, SequenceView *sview,
		int notePos,
		QPoint dragOrigin)
	: Context(sview, parent)
	, locker(sview)
	, notePos(notePos)
	, dragOrigin(dragOrigin)
	, dragBegan(false)
{
	auto *channel = sview->soundChannels[sview->currentChannel]->GetChannel();
	QMap<SoundChannel *, QSet<int> > locs;
	locs.insert(channel, QSet<int>());
	locs[channel].insert(notePos);
	updateNotesCxt = new Document::DocumentUpdateSoundNotesContext(sview->document, locs);
	dragNotesPreviousTime = notePos;
}


SequenceView::WriteModeDrawNoteContext::~WriteModeDrawNoteContext()
{
	if (updateNotesCxt){
		updateNotesCxt->Cancel();
		delete updateNotesCxt;
	}
}

/*
SequenceView::Context *SequenceView::WriteModeDrawNoteContext::Enter(QEnterEvent *)
{
	return this;
}

SequenceView::Context *SequenceView::WriteModeDrawNoteContext::Leave(QEnterEvent *)
{
	return this;
}
*/
SequenceView::Context *SequenceView::WriteModeDrawNoteContext::PlayingPane_MouseMove(QMouseEvent *event)
{
	if (!dragBegan){
		if (UIUtil::DragBegins(dragOrigin, event->pos())){
			dragBegan = true;
		}else{
			return this;
		}
	}
	qreal time = sview->Y2Time(event->y());
	int iTime = time;
    [[maybe_unused]] int iTimeUpper = time;
	int lane = sview->X2Lane(event->x());
	if (sview->snapToGrid){
		iTime = sview->SnapToLowerFineGrid(time);
		iTimeUpper = sview->SnapToUpperFineGrid(time);
	}
    [[maybe_unused]] int laneX;
	if (lane < 0){
		if (event->x() < sview->sortedLanes[0].left){
			laneX = 0;
		}else{
			laneX = sview->sortedLanes.size() - 1;
		}
	}else{
		laneX = sview->sortedLanes.indexOf(sview->lanes[lane]);
	}
	if (iTime != dragNotesPreviousTime){
		QMap<SoundChannel*, QMap<int, SoundNote>> notes;
		QMap<SoundChannel*, QMap<int, SoundNote>> originalNotes = updateNotesCxt->GetOldNotes();
		for (SoundNoteView *nv : sview->selectedNotes){
			auto *channel = nv->GetChannelView()->GetChannel();
			if (!notes.contains(channel)){
				notes.insert(channel, QMap<int, SoundNote>());
			}
			SoundNote note = originalNotes[channel][nv->GetNote().location];
			note.length = std::max(0, note.length + iTime - notePos);
			notes[channel].insert(note.location, note);
		}
		updateNotesCxt->Update(notes);
	}
	dragNotesPreviousTime = iTime;
	return this;
}

SequenceView::Context *SequenceView::WriteModeDrawNoteContext::PlayingPane_MousePress([[maybe_unused]] QMouseEvent *event)
{
	return this;
}

SequenceView::Context *SequenceView::WriteModeDrawNoteContext::PlayingPane_MouseRelease(QMouseEvent *event)
{
	if (event->button() != Qt::LeftButton){
		// ignore
		return this;
	}
	updateNotesCxt->Finish();
	delete updateNotesCxt;
	updateNotesCxt = nullptr;
	// Classic BMS mode: a keyed long note should own the sample only up to its
	// release, so auto-split there — the remainder returns to the background,
	// exactly like a manual right-click split at the LN's end. Skip when a note
	// already bounds the release tick, when the sample has ended by then, or
	// for `up` notes (their release re-trigger has its own semantics).
	if (sview->groupedBgmView && sview->currentChannel >= 0){
		SoundChannel *channel = sview->soundChannels[sview->currentChannel]->GetChannel();
		const auto &ns = channel->GetNotes();
		if (ns.contains(notePos)){
			const SoundNote n = ns[notePos];
			const int end = n.location + n.length;
			if (n.length > 0 && !n.up && !ns.contains(end)){
				int startLoc = -1;
				for (auto it = ns.begin(); it != ns.end() && it.key() <= n.location; ++it)
					if (it.value().noteType == 0)
						startLoc = it.key();
				if (startLoc >= 0 && end < channel->GetSampleEndTick(startLoc)){
					channel->InsertNote(SoundNote(end, 0, 0, 1));
				}
			}
		}
	}
	return Escape();
}





