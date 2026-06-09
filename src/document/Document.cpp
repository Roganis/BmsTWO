#include <QFile>
#include "Document.h"
#include "DocumentAux.h"
#include "SoundChannel.h"
#include "History.h"
#include "HistoryUtil.h"
#include "MasterCache.h"
#include "EditConfig.h"
#include "../bmson/Bmson100.h"
#include "../util/ResolutionUtil.h"
#include "../bms/Bms.h"

using namespace Bmson100;

const double BmsConsts::MaxBpm = 1.e+6;
const double BmsConsts::MinBpm = 1.e-6;

bool BmsConsts::IsBpmValid(double value)
{
    return value >= MinBpm && value <= MaxBpm;
}

double BmsConsts::ClampBpm(double value)
{
    return std::max(BmsConsts::MinBpm, std::min(BmsConsts::MaxBpm, value));
}


Document::Document(QObject *parent)
    : QObject(parent)
    , info(this)
{
    history = new EditHistory(this);
    master = new MasterCache(this);
    outputVersion = BmsonIO::NativeVersion;
    savedVersion = BmsonIO::NativeVersion;
    connect(this, SIGNAL(TimeMappingChanged()), this, SLOT(ReconstructMasterCache()), Qt::QueuedConnection);
    connect(&info, SIGNAL(InitBpmChanged(double)), this, SLOT(OnInitBpmChanged()));

    masterEnabled = EditConfig::GetEnableMasterChannel();
    connect(EditConfig::Instance(), SIGNAL(EnableMasterChannelChanged(bool)), this, SLOT(EnableMasterChannelChanged(bool)));
}

Document::~Document()
{
    delete history;
    delete master;
}

void Document::Initialize()
{
    directory = QDir::root();

    bmsonFields = BmsonIO::InitialBmson();
    actualLength = 0;
    totalLength = 0;
    info.Initialize();
    barLines.insert(0, BarLine(0, 0));
    UpdateTotalLength();
    ReconstructMasterCache();

    savedVersion = BmsonIO::NativeVersion;
    history->SetReservedAction(false); // in spite of outputVersion
}

void Document::LoadFile(QString filePath)
{
    //
    // TODO: Return errors instead of exceptions.
    //

    directory = QFileInfo(filePath).absoluteDir();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)){
        throw tr("Failed to open file.");
    }
    if (file.size() >= 0x40000000){
        throw tr("Malformed bmson file.");
    }
    QJsonParseError jsonError;
    QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError){
        throw tr("Malformed bmson file.");
    }
    if (!json.isObject()){
        throw tr("Malformed bmson file.");
    }
    BmsonConvertContext cx;
    bmsonFields = BmsonIO::NormalizeBmson(cx, json.object());
    if (cx.GetState() == BmsonConvertContext::BMSON_ERROR){
        throw cx.GetCombinedMessage();
    }
    actualLength = 0;
    totalLength = 0;
    info.LoadBmson(bmsonFields[Bmson::Bms::InfoKey]);
    barLines.insert(0, BarLine(0, 0)); // make sure BarLine at 0, even if bms.barLines is empty.
    QJsonArray jsonBar = bmsonFields[Bmson::Bms::BarLinesKey].toArray();
    for (int i=0; i<jsonBar.size(); i++){
        BarLine barLine(jsonBar[i]);
        barLines.insert(barLine.Location, barLine);
    }
    QJsonArray jsonEvent = bmsonFields[Bmson::Bms::BpmEventsKey].toArray();
    for (int i=0; i<jsonEvent.size(); i++){
        BpmEvent event(jsonEvent[i]);
        bpmEvents.insert(event.location, event);
    }
    if (bmsonFields.contains(Bmson::Bms::BgaKey)){
        bga = Bga(bmsonFields[Bmson::Bms::BgaKey]);
    }
    QJsonArray jsonStop = bmsonFields[Bmson::Bms::StopEventsKey].toArray();
    for (int i=0; i<jsonStop.size(); i++){
        StopEvent event(jsonStop[i]);
        stopEvents.insert(event.location, event);
    }
    QJsonArray soundChannelsJson = bmsonFields[Bmson::Bms::SoundChannelsKey].toArray();
    for (int i=0; i<soundChannelsJson.size(); i++){
        auto *channel = new SoundChannel(this);
        channel->LoadBmson(soundChannelsJson[i]);
        soundChannelLength.insert(channel, channel->GetLength());
        soundChannels.push_back(channel);
    }
    UpdateTotalLength();
    ReconstructMasterCache();
    this->filePath = filePath;

    // conversion is not revertible by Undo.
    if (cx.IsConverted()){
        history->MarkAbsolutelyDirty();
    }
    savedVersion = BmsonIO::NativeVersion; // in spite of actual version
    history->SetReservedAction(outputVersion != savedVersion);

    emit FilePathChanged();
}

void Document::LoadBms(const Bms::Bms &bms)
{
    directory = QFileInfo(bms.path).absoluteDir();

    bmsonFields = BmsonIO::InitialBmson();
    actualLength = 0;
    totalLength = 0;

    // read bms

    info.LoadBms(bms);

    {
        // 小節線
        const int lastObjPos = Bms::BmsUtil::GetTotalLength(bms) * info.GetResolution();
        int pos = 0;
        barLines.insert(0, BarLine(0, 0));
        for (int i=0; i<bms.sections.length() && pos<=lastObjPos; i++){
            int sectionLength = Bms::BmsUtil::GetSectionLengthInBmson(info.GetResolution(), bms.sections[i]);
            pos += sectionLength;
            barLines.insert(pos, BarLine(pos, 0));
        }
    }
    {
        // BPMイベント
        int pos = 0;
        for (int i=0; i<bms.sections.length(); i++){
            int sectionLength = Bms::BmsUtil::GetSectionLengthInBmson(info.GetResolution(), bms.sections[i]);
            // チャンネル03: BPM (オブジェ番号のFF解釈が整数BPM値)
            if (bms.sections[i].objects.contains(3)){
                const Bms::Sequence &sequence = bms.sections[i].objects[3];
                for (auto obj=sequence.objects.begin(); obj!=sequence.objects.end(); obj++){
                    int relativePos = Bms::BmsUtil::GetPositionInSectionInBmson(info.GetResolution(), bms.sections[i], sequence, obj.key());
                    int ffnum = Bms::BmsUtil::ZZNUMtoFFNUM(obj.value());
                    if (ffnum > 0){
                        bpmEvents.insert(pos+relativePos, BpmEvent(pos+relativePos, (qreal)ffnum));
                    }
                }
            }
            // チャンネル08: 拡張BPM (BPM定義を参照, 存在しない場合は無視)
            if (bms.sections[i].objects.contains(8)){
                const Bms::Sequence &sequence = bms.sections[i].objects[8];
                for (auto obj=sequence.objects.begin(); obj!=sequence.objects.end(); obj++){
                    int relativePos = Bms::BmsUtil::GetPositionInSectionInBmson(info.GetResolution(), bms.sections[i], sequence, obj.key());
                    if (bms.bpmDefs[obj.value()] > 0.0){
                        bpmEvents.insert(pos+relativePos, BpmEvent(pos+relativePos, bms.bpmDefs[obj.value()]));
                    }
                }
            }
            pos += sectionLength;
        }
    }
    {
        // STOPイベント
        int pos = 0;
        for (int i=0; i<bms.sections.length(); i++){
            int sectionLength = Bms::BmsUtil::GetSectionLengthInBmson(info.GetResolution(), bms.sections[i]);
            // チャンネル09: STOP (STOP定義を参照, 存在しない場合は無視)
            if (bms.sections[i].objects.contains(9)){
                const Bms::Sequence &sequence = bms.sections[i].objects[9];
                for (auto obj=sequence.objects.begin(); obj!=sequence.objects.end(); obj++){
                    int relativePos = Bms::BmsUtil::GetPositionInSectionInBmson(info.GetResolution(), bms.sections[i], sequence, obj.key());
                    if (bms.stopDefs[obj.value()] > 0.0){
                        stopEvents.insert(pos+relativePos, StopEvent(pos+relativePos, Bms::BmsUtil::GetStopDurationInBmson(info.GetResolution(), bms.stopDefs[obj.value()])));
                    }
                }
            }
            pos += sectionLength;
        }
        // The live `stopEvents` model is now the source of truth; ExportTo serializes it.
    }
    {
        // BGAイベント
        bga.bgaEvents = Bms::BmsUtil::GetBgaEvents(bms, info.GetResolution());
        bga.layerEvents = Bms::BmsUtil::GetLayerEvents(bms, info.GetResolution());
        bga.missEvents = Bms::BmsUtil::GetMissEvents(bms, info.GetResolution());

        QVector<bool> bmpUsed(bms.bmpDefs.length());
        for (const auto &ev : std::as_const(bga.bgaEvents)) {
            if (ev.id < bmpUsed.length()){
                bmpUsed[ev.id] = true;
            }
        }
        for (const auto &ev : std::as_const(bga.layerEvents)) {
            if (ev.id < bmpUsed.length()){
                bmpUsed[ev.id] = true;
            }
        }
        for (const auto &ev : std::as_const(bga.missEvents)) {
            if (ev.id < bmpUsed.length()){
                bmpUsed[ev.id] = true;
            }
        }

        // BGAヘッダ
        for (int i=0; i<bms.bmpDefs.length(); i++){
            if (bmpUsed[i] || !bms.bmpDefs[i].isEmpty()){
                bga.headers.insert(i, BgaHeader(i, bms.bmpDefs[i]));
            }
        }
        // The live `bga` model is now the source of truth; ExportTo serializes it.
    }

    QVector<QMap<int, SoundNote>> notes = Bms::BmsUtil::GetNotesOfBmson(bms, bms.mode, info.GetResolution());

    for (int i=0; i<bms.wavDefs.length(); i++){
        const QString &name = bms.wavDefs[i];
        if (name.isEmpty() && notes[i].empty())
            continue;
        auto *channel = new SoundChannel(this);
        channel->InitializeWithNotes(name, notes[i]);
        soundChannelLength.insert(channel, channel->GetLength());
        soundChannels.push_back(channel);
    }

    UpdateTotalLength();
    ReconstructMasterCache();

    savedVersion = BmsonIO::NativeVersion;
    history->MarkAbsolutelyDirty();
}

// This function may modify document, but the change is not recorded in undo buffer
QString Document::GetRelativePath(QString filePath)
{
    QFileInfo fi(filePath);
    if (directory.isRoot()){
        directory = fi.absoluteDir();
        return fi.fileName();
    }else{
        return directory.relativeFilePath(filePath);
    }
}

QString Document::GetAbsolutePath(QString fileName) const
{
    if (directory.isRoot()){
        return QString();
    }
    return directory.absoluteFilePath(fileName);
}

bool Document::IsFilePathTraversalInternal(QString path) const
{
    QString relativePath = directory.relativeFilePath(path);
    return relativePath.startsWith("../") || !QDir(relativePath).isRelative();
}

bool Document::IsFilePathTraversal(QString path) const
{
    if (directory.isRoot()){
        return false;
    }else{
        return IsFilePathTraversalInternal(path);
    }
}

QStringList Document::FindTraversalFilePaths(const QStringList &filePaths) const
{
    QStringList traversalPaths;
    QDir dirTemp = directory;
    for (const auto &path : filePaths) {
        if (dirTemp.isRoot()){
            dirTemp = QFileInfo(path).absoluteDir();
        }else{
            if (IsFilePathTraversalInternal(path)){
                traversalPaths << path;
            }
        }
    }
    return traversalPaths;
}



void Document::ExportTo(const QString &exportFilePath)
{
    //
    // TODO: Return errors instead of exceptions.
    //

    bmsonFields[Bmson::Bms::InfoKey] = info.SaveBmson();
    QJsonArray jsonBarLines;
    for (BarLine barLine : std::as_const(barLines)){
        if (!barLine.Ephemeral || barLine.Location <= actualLength){
            jsonBarLines.append(barLine.SaveBmson());
        }
    }
    bmsonFields[Bmson::Bms::BarLinesKey] = jsonBarLines;
    QJsonArray jsonBpmEvents;
    for (BpmEvent event : std::as_const(bpmEvents)){
        jsonBpmEvents.append(event.SaveBmson());
    }
    bmsonFields[Bmson::Bms::BpmEventsKey] = jsonBpmEvents;
    QJsonArray jsonStopEvents;
    for (StopEvent event : std::as_const(stopEvents)){
        jsonStopEvents.append(event.SaveBmson());
    }
    bmsonFields[Bmson::Bms::StopEventsKey] = jsonStopEvents;
    if (!bga.headers.empty() || !bga.bgaEvents.empty() || !bga.layerEvents.empty() || !bga.missEvents.empty()){
        bmsonFields[Bmson::Bms::BgaKey] = bga.AsJson();
    }
    QJsonArray jsonSoundChannels;
    for (SoundChannel *channel : std::as_const(soundChannels)){
        jsonSoundChannels.append(channel->SaveBmson());
    }
    bmsonFields[Bmson::Bms::SoundChannelsKey] = jsonSoundChannels;
    QFile file(exportFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)){
        throw tr("Failed to open file.");
    }

    // TODO: configuration
    BmsonConvertContext cxt;
    QJsonObject obj = BmsonIO::Convert(cxt, bmsonFields, outputVersion);
    if (cxt.GetState() == BmsonConvertContext::BMSON_ERROR){
        throw tr("Failed to convert bmson format:") + "\n" + cxt.GetCombinedMessage();
    }
    // savedVersion = outputVersion;
    file.write(QJsonDocument(obj).toJson(BmsonIO::GetSaveJsonFormat()));
}

void Document::Save()
{
    ExportTo(filePath);
    savedVersion = outputVersion;
    history->SetReservedAction(false);
    history->MarkClean();
}

void Document::SaveAs(const QString &filePath)
{
    this->filePath = filePath;
    this->directory = QFileInfo(filePath).absoluteDir();
    Save();
    emit FilePathChanged();
}
void Document::SetRecoveredFilePath(const QString &filePath)
{
    this->filePath = filePath;
    this->directory = filePath.isEmpty() ? QDir::root() : QFileInfo(filePath).absoluteDir();
    history->MarkAbsolutelyDirty();
    emit FilePathChanged();
}

void Document::SetOutputVersion(BmsonIO::BmsonVersion version)
{
    outputVersion = version;
    // if untitled, savedVersion is ignored and reserved action is disabled.
    history->SetReservedAction(!directory.isRoot() && version != savedVersion);
}

double Document::GetAbsoluteTime(int ticks) const
{
    const double resolution = info.GetResolution();
    int tt = 0;
    double seconds = 0;
    double bpm = info.GetInitBpm();
    auto ib = bpmEvents.begin();
    auto is = stopEvents.begin();
    // Walk BPM and STOP events in location order. A STOP at location s pauses
    // playback for `duration` pulses' worth of time at the BPM in effect there.
    while (true){
        int nb = (ib != bpmEvents.end()) ? ib.key() : INT_MAX;
        int ns = (is != stopEvents.end()) ? is.key() : INT_MAX;
        int next = std::min(nb, ns);
        if (next >= ticks)
            break;
        seconds += (next - tt) * 60.0 / (bpm * resolution);
        tt = next;
        if (ns == next){
            // stop duration uses the BPM currently in effect (before any
            // simultaneous BPM change)
            seconds += is->value * 60.0 / (bpm * resolution);
            ++is;
        }
        if (nb == next){
            bpm = ib->value;
            ++ib;
        }
    }
    return seconds + (ticks - tt) * 60.0 / (bpm * resolution);
}

int Document::FromAbsoluteTime(double destSeconds) const
{
    const double resolution = info.GetResolution();
    int tt = 0;
    double seconds = 0;
    double bpm = info.GetInitBpm();
    auto ib = bpmEvents.begin();
    auto is = stopEvents.begin();
    while (true){
        int nb = (ib != bpmEvents.end()) ? ib.key() : INT_MAX;
        int ns = (is != stopEvents.end()) ? is.key() : INT_MAX;
        int next = std::min(nb, ns);
        if (next == INT_MAX)
            break;
        double seg = (next - tt) * 60.0 / (bpm * resolution);
        if (seconds + seg >= destSeconds)
            break; // destination lies within the current linear segment
        seconds += seg;
        tt = next;
        if (ns == next){
            double stopSeconds = is->value * 60.0 / (bpm * resolution);
            if (seconds + stopSeconds >= destSeconds){
                // destination falls during the stop: tick stays put
                return tt;
            }
            seconds += stopSeconds;
            ++is;
        }
        if (nb == next){
            bpm = ib->value;
            ++ib;
        }
    }
    return tt + (destSeconds - seconds) * (bpm * resolution) / 60.0;
}

int Document::GetTotalLength() const
{
    return totalLength;
}

int Document::GetTotalVisibleLength() const
{
    return totalLength;
}

QList<QPair<SoundChannel*, int> > Document::FindConflictingNotes(SoundNote note) const
{
    QList<QPair<SoundChannel*, int>> noteRefs;
    for (auto channel : soundChannels){
        const QMap<int, SoundNote> &notes = channel->GetNotes();
        QMap<int, SoundNote>::const_iterator inote = notes.lowerBound(note.location);
        if (inote != notes.begin())
            inote--;
        while (inote != notes.end() && inote->location <= note.location + note.length){
            if (inote->lane > 0 && inote->lane == note.lane && inote->location+inote->length>=note.location){
                noteRefs.append(QPair<SoundChannel*,int>(channel, inote->location));
            }
            inote++;
        }
    }
    return noteRefs;
}

QMap<int, QMap<int, NoteConflict>> Document::FindConflictsByLanes(int timeBegin, int timeEnd) const
{
    QMultiMap<int, QPair<SoundChannel*, SoundNote>> allNotes = FindNotes(timeEnd);
    QMap<int, QMap<int, NoteConflict>> conflictsByLanes;
    QMap<int, QMultiMap<SoundChannel*, SoundNote>> currentNotesByLanes;
    for (const QPair<SoundChannel *, SoundNote> &pair : std::as_const(allNotes)) {
        int lane = pair.second.lane;
        int location = pair.second.location;
        if (lane == 0)
            continue;
        if (!currentNotesByLanes.contains(lane)){
            currentNotesByLanes.insert(lane, QMultiMap<SoundChannel*, SoundNote>());
        }
        for (auto i=currentNotesByLanes[lane].begin(); i!=currentNotesByLanes[lane].end(); ){
            if (i.value().location + i.value().length < location){
                i = currentNotesByLanes[lane].erase(i);
                continue;
            }
            i++;
        }
        if (location >= timeBegin && !currentNotesByLanes[lane].empty()){
            NoteConflict conf;
            if (conflictsByLanes[lane].contains(location)){
                conf = conflictsByLanes[lane][location];
            }else{
                conf.lane = lane;
                conf.location = location;
                conf.involvedNotes = QList<QPair<SoundChannel*, SoundNote>>();
                conf.type = 0;
                for (auto n = currentNotesByLanes[lane].begin(); n != currentNotesByLanes[lane].end(); n++){
                    conf.involvedNotes << QPair<SoundChannel*, SoundNote>(n.key(), n.value());
                    if (n.value().location == location){
                        conf.type |= NoteConflict::LAYERING_FLAG;
                        if (n.value().length != pair.second.length){
                            conf.type |= NoteConflict::NONUNIFORM_LAYERING_FLAG;
                        }
                    }else{
                        conf.type |= NoteConflict::OVERLAPPING_FLAG;
                    }
                }
            }
            conf.involvedNotes << pair;
            conflictsByLanes[lane][location] = conf;
        }
        currentNotesByLanes[lane].insert(pair.first, pair.second);
    }
    return conflictsByLanes;
}

QMultiMap<int, QPair<SoundChannel *, SoundNote> > Document::FindNotes(int timeEnd) const
{
    QMultiMap<int, QPair<SoundChannel*, SoundNote>> allNotes;
    for (auto channel : soundChannels){
        const auto &notes = channel->GetNotes();
        for (const auto &note : notes) {
            if (timeEnd >= 0 && note.location >= timeEnd) break;
            allNotes.insert(note.location, QPair<SoundChannel*, SoundNote>(channel, note));
        }
    }
    return allNotes;
}


void Document::InsertSoundChannelInternal(SoundChannel *channel, int index)
{
    soundChannelLength.insert(channel, channel->GetLength());
    soundChannels.insert(index, channel);
    channel->AddAllIntoMasterCache(1);
    emit SoundChannelInserted(index, channel);
    emit AfterSoundChannelsChange();
}

void Document::RemoveSoundChannelInternal(SoundChannel *channel, int index)
{
    if (soundChannels.size() <= index || soundChannels[index] != channel){
        qDebug() << "Sound channels don't match!";
        // insurance (try to behave as likely as possible)
        int correctIndex = soundChannels.indexOf(channel);
        if (correctIndex < 0)
            return;
        soundChannels.removeAt(correctIndex);
        soundChannelLength.remove(channel);
        emit SoundChannelRemoved(correctIndex, channel);
        emit AfterSoundChannelsChange();
        return;
    }
    channel->AddAllIntoMasterCache(-1);
    soundChannels.removeAt(index);
    soundChannelLength.remove(channel);
    emit SoundChannelRemoved(index, channel);
    emit AfterSoundChannelsChange();
}

bool Document::DetectConflictsAroundNotes(const QMultiMap<int, SoundNote> &notes) const
{
    for (const auto &note : notes) {
        // todo: make more efficient
        auto ns = FindConflictingNotes(note);
        if (ns.size() > 1){ // one element is the note itself
            return true;
        }
    }
    return false;
}




void Document::InsertNewSoundChannels(const QList<QString> &soundFilePaths, int index)
{
    for (int i=0; i<soundFilePaths.size(); i++){
        auto *channel = new SoundChannel(this);
        channel->LoadSound(soundFilePaths[i]);

        auto *action = new InsertSoundChannelAction(this, channel, index < 0 ? soundChannels.size() : index+i);
        history->Add(action);
    }
}

void Document::DestroySoundChannel(int index)
{
    if (index < 0 || index >= (int)soundChannels.size())
        return;
    auto *action = new RemoveSoundChannelAction(this, soundChannels.at(index), index);
    history->Add(action);
}

void Document::MoveSoundChannel(int indexBefore, int indexAfter)
{
    if (indexBefore < 0 || indexBefore >= (int)soundChannels.size())
        return;
    indexAfter = std::max(0, std::min((int)soundChannels.size()-1, indexAfter));
    if (indexBefore == indexAfter)
        return;
    auto updater = [this](int indexOld, int indexNew){
        auto *channel = soundChannels.takeAt(indexOld);
        soundChannels.insert(indexNew, channel);
        emit SoundChannelMoved(indexOld, indexNew);
        emit AfterSoundChannelsChange();
    };
    auto *action = new BiEditValueAction<int>(updater, indexBefore, indexAfter, tr("move sound channel"), true);
    history->Add(action);
}

void Document::ChannelLengthChanged(SoundChannel *channel, int length)
{
    soundChannelLength.insert(channel, length);
    UpdateTotalLength();
}

void Document::UpdateTotalLength()
{
    int oldValue = totalLength;
    actualLength = 0;
    for (int length : std::as_const(soundChannelLength)){
        if (length > actualLength)
            actualLength = length;
    }
    totalLength = actualLength + 32 * 4 * info.GetResolution();

    // update ephemeral bars
    for (QMap<int, BarLine>::iterator i=barLines.begin(); i!=barLines.end(); ){
        if (i->Ephemeral){
            i = barLines.erase(i);
            continue;
        }
        i++;
    }
    if (!barLines.contains(0)){
        barLines.insert(0, BarLine(0, 0));
    }
    for (int t = barLines.lastKey() + 4*info.GetResolution(); t<totalLength; t+=4*info.GetResolution()){
        barLines.insert(t, BarLine(t, 0, true));
    }

    if (oldValue != totalLength){
        emit TotalLengthChanged(totalLength);
    }
}

void Document::OnInitBpmChanged()
{
    emit TimeMappingChanged();
}

void Document::EnableMasterChannelChanged(bool enabled)
{
    if (masterEnabled != enabled){
        masterEnabled = enabled;
        ReconstructMasterCache();
    }
}

bool Document::InsertBarLine(BarLine bar)
{
    if (barLines.contains(bar.Location) && barLines[bar.Location] == bar)
        return false;
    QMap<int, BarLine> oldBarLines = barLines; // zeitaku!!!!!!!
    auto wh = barLines.insert(bar.Location, bar);
    for (auto i=barLines.begin(); i!=wh; i++){
        if (i->Ephemeral){
            i->Ephemeral = false;
        }
    }
    UpdateTotalLength(); // update ephemeral bars after `bar`
    emit BarLinesChanged();
    auto updater = [this](QMap<int, BarLine> value){
        barLines = value;
        emit BarLinesChanged();
    };
    history->Add(new EditValueAction<QMap<int, BarLine>>(updater, oldBarLines, barLines, tr("add bar line"), false));
    return true;
}

bool Document::RemoveBarLine(int location)
{
    if (!barLines.contains(location)) return false;
    QMap<int, BarLine> oldBarLines = barLines;
    barLines.remove(location);
    UpdateTotalLength(); // update ephemeral bars after `bar`
    emit BarLinesChanged();
    auto updater = [this](QMap<int, BarLine> value){
        barLines = value;
        emit BarLinesChanged();
    };
    history->Add(new EditValueAction<QMap<int, BarLine>>(updater, oldBarLines, barLines, tr("remove bar line"), false));
    return true;
}



bool Document::InsertBpmEvent(BpmEvent event)
{
    auto shower = [=](){
        emit ShowBpmEventLocation(event.location);
    };
    if (bpmEvents.contains(event.location)){
        if (bpmEvents[event.location] == event){
            return false;
        }
        auto updater = [this](BpmEvent value){
            bpmEvents.insert(value.location, value);
            emit TimeMappingChanged();
        };
        history->Add(new EditValueAction<BpmEvent>(updater, bpmEvents[event.location], event, tr("update BPM event"), true, shower));
        return true;
    }else{
        auto adder = [this](BpmEvent value){
            bpmEvents.insert(value.location, value);
            emit TimeMappingChanged();
        };
        auto remover = [this](BpmEvent value){
            bpmEvents.remove(value.location);
            emit TimeMappingChanged();
        };
        history->Add(new AddValueAction<BpmEvent>(adder, remover, event, tr("add BPM event"), true, shower));
        return true;
    }
}

bool Document::RemoveBpmEvent(int location)
{
    auto shower = [=](){
        emit ShowBpmEventLocation(location);
    };
    if (!bpmEvents.contains(location)) return false;
    BpmEvent event = bpmEvents.take(location);
    auto adder = [this](BpmEvent value){
        bpmEvents.insert(value.location, value);
        emit TimeMappingChanged();
    };
    auto remover = [this](BpmEvent value){
        bpmEvents.remove(value.location);
        emit TimeMappingChanged();
    };
    history->Add(new RemoveValueAction<BpmEvent>(adder, remover, event, tr("remove BPM event"), true, shower));
    return true;
}

void Document::UpdateBpmEvents(QList<BpmEvent> events)
{
    for (const auto &event : events) {
        if (bpmEvents.contains(event.location)){
            if (!(bpmEvents[event.location] == event))
                goto changed;
        }else{
            goto changed;
        }
    }
    return;
changed:
    auto shower = [=](){
        emit ShowBpmEventLocation(events.first().location);
    };
    auto afterDo = [=](){
        emit TimeMappingChanged();
    };
    auto *actions = new MultiAction(tr("update BPM events"), shower);
    for (const auto &event : std::as_const(events)) {
        if (bpmEvents.contains(event.location)){
            auto updater = [this](BpmEvent value){
                bpmEvents.insert(value.location, value);
            };
            actions->AddAction(new EditValueAction<BpmEvent>(updater, bpmEvents[event.location], event, QString(), true));
        }else{
            auto adder = [this](BpmEvent value){
                bpmEvents.insert(value.location, value);
                emit TimeMappingChanged();
            };
            auto remover = [this](BpmEvent value){
                bpmEvents.remove(value.location);
            };
            actions->AddAction(new AddValueAction<BpmEvent>(adder, remover, event, QString(), true));
        }
    }
    actions->Finish(afterDo, afterDo);
    afterDo();
    history->Add(actions);
}

void Document::RemoveBpmEvents(QList<int> locations)
{
    if (locations.isEmpty())
        return;
    auto shower = [=](){
        emit ShowBpmEventLocation(locations.first());
    };
    auto afterDo = [=](){
        emit TimeMappingChanged();
    };
    auto *actions = new MultiAction(tr("remove BPM events"), shower);
    for (auto location : std::as_const(locations)){
        if (!bpmEvents.contains(location)) continue;
        BpmEvent event = bpmEvents.take(location);
        auto adder = [this](BpmEvent value){
            bpmEvents.insert(value.location, value);
        };
        auto remover = [this](BpmEvent value){
            bpmEvents.remove(value.location);
        };
        actions->AddAction(new RemoveValueAction<BpmEvent>(adder, remover, event, tr("remove BPM event"), true, shower));
    }
    actions->Finish(afterDo, afterDo);
    afterDo();
    history->Add(actions);
}


bool Document::InsertStopEvent(StopEvent event)
{
    if (stopEvents.contains(event.location)){
        if (stopEvents[event.location] == event)
            return false;
        auto updater = [this](StopEvent value){
            stopEvents.insert(value.location, value);
            emit StopEventsChanged();
        };
        history->Add(new EditValueAction<StopEvent>(updater, stopEvents[event.location], event, tr("update STOP event"), true));
        return true;
    }else{
        auto adder = [this](StopEvent value){
            stopEvents.insert(value.location, value);
            emit StopEventsChanged();
        };
        auto remover = [this](StopEvent value){
            stopEvents.remove(value.location);
            emit StopEventsChanged();
        };
        history->Add(new AddValueAction<StopEvent>(adder, remover, event, tr("add STOP event"), true));
        return true;
    }
}

bool Document::RemoveStopEvent(int location)
{
    if (!stopEvents.contains(location))
        return false;
    StopEvent event = stopEvents.take(location);
    auto adder = [this](StopEvent value){
        stopEvents.insert(value.location, value);
        emit StopEventsChanged();
    };
    auto remover = [this](StopEvent value){
        stopEvents.remove(value.location);
        emit StopEventsChanged();
    };
    emit StopEventsChanged();
    history->Add(new RemoveValueAction<StopEvent>(adder, remover, event, tr("remove STOP event"), false));
    return true;
}


// --- BGA editing -----------------------------------------------------------
// The BGA payload is small, so each edit snapshots the whole Bga and replays
// it via EditValueAction<Bga>; this keeps the undo logic trivially correct for
// headers and all three event lanes without per-field bookkeeping.

static QMap<int, BgaEvent> &BgaEventMapRef(Bga &bga, BgaLayer layer)
{
    switch (layer){
    case BgaLayer::Layer: return bga.layerEvents;
    case BgaLayer::Poor:  return bga.missEvents;
    case BgaLayer::Base:
    default:              return bga.bgaEvents;
    }
}

const QMap<int, BgaEvent> &Document::GetBgaEvents(BgaLayer layer) const
{
    switch (layer){
    case BgaLayer::Layer: return bga.layerEvents;
    case BgaLayer::Poor:  return bga.missEvents;
    case BgaLayer::Base:
    default:              return bga.bgaEvents;
    }
}

bool Document::InsertBgaHeader(BgaHeader header)
{
    if (bga.headers.contains(header.id) && bga.headers[header.id] == header)
        return false;
    Bga oldBga = bga;
    bga.headers.insert(header.id, header);
    emit BgaChanged();
    auto updater = [this](Bga value){ bga = value; emit BgaChanged(); };
    history->Add(new EditValueAction<Bga>(updater, oldBga, bga, tr("edit BGA header"), false));
    return true;
}

bool Document::RemoveBgaHeader(int id)
{
    if (!bga.headers.contains(id))
        return false;
    Bga oldBga = bga;
    bga.headers.remove(id);
    emit BgaChanged();
    auto updater = [this](Bga value){ bga = value; emit BgaChanged(); };
    history->Add(new EditValueAction<Bga>(updater, oldBga, bga, tr("remove BGA header"), false));
    return true;
}

bool Document::InsertBgaEvent(BgaLayer layer, BgaEvent event)
{
    QMap<int, BgaEvent> &map = BgaEventMapRef(bga, layer);
    if (map.contains(event.location) && map[event.location] == event)
        return false;
    Bga oldBga = bga;
    BgaEventMapRef(bga, layer).insert(event.location, event);
    emit BgaChanged();
    auto updater = [this](Bga value){ bga = value; emit BgaChanged(); };
    history->Add(new EditValueAction<Bga>(updater, oldBga, bga, tr("edit BGA event"), false));
    return true;
}

bool Document::RemoveBgaEvent(BgaLayer layer, int location)
{
    QMap<int, BgaEvent> &map = BgaEventMapRef(bga, layer);
    if (!map.contains(location))
        return false;
    Bga oldBga = bga;
    BgaEventMapRef(bga, layer).remove(location);
    emit BgaChanged();
    auto updater = [this](Bga value){ bga = value; emit BgaChanged(); };
    history->Add(new EditValueAction<Bga>(updater, oldBga, bga, tr("remove BGA event"), false));
    return true;
}


bool Document::MultiChannelDeleteSoundNotes(const QMultiMap<SoundChannel *, SoundNote> &notes)
{
    if (notes.empty())
        return false;
    int minLocation = INT_MAX;
    SoundChannel *channel = nullptr;
    for (auto i=notes.begin(); i!=notes.end(); i++){
        if (i->location < minLocation){
            minLocation = i->location;
            channel = i.key();
        }
    }
    auto shower = [=](){
        emit channel->ShowNoteLocation(minLocation);
    };
    auto *actions = new MultiAction(tr("delete sound notes"), shower);
    for (auto i=notes.begin(); i!=notes.end(); i++){
        auto action = i.key()->RemoveNoteInternal(i.value(), false);
        if (!action)
            continue;
        actions->AddAction(action);
    }
    if (actions->Count() == 0){
        delete actions;
        return false;
    }
    auto afterDo = [=](){
        emit AnyNotesChanged();
    };
    actions->Finish(afterDo, afterDo);
    afterDo();
    history->Add(actions);
    return true;
}

bool Document::MultiChannelUpdateSoundNotes(const QMultiMap<SoundChannel *, SoundNote> &notes,
        UpdateNotePolicy policy,
        QList<int> acceptableLanes)
{
    if (notes.empty())
        return false;
    int minLocation = INT_MAX;
    SoundChannel *channel = nullptr;
    for (auto i=notes.begin(); i!=notes.end(); i++){
        if (i->location < minLocation){
            minLocation = i->location;
            channel = i.key();
        }
    }
    auto shower = [=](){
        emit channel->ShowNoteLocation(minLocation);
    };
    auto *actions = new MultiAction(tr("update sound notes"), shower);
    for (auto i=notes.begin(); i!=notes.end(); i++){
        auto action = i.key()->InsertNoteInternal(i.value(), false, policy, acceptableLanes);
        if (!action)
            continue;
        actions->AddAction(action);
    }
    if (actions->Count() == 0){
        delete actions;
        return false;
    }
    auto afterDo = [=](){
        emit AnyNotesChanged();
    };
    actions->Finish(afterDo, afterDo);
    afterDo();
    history->Add(actions);
    return true;
}

Document::DocumentUpdateSoundNotesContext *Document::BeginModalEditSoundNotes(const QMap<SoundChannel*, QSet<int>> &noteLocations)
{
    return new DocumentUpdateSoundNotesContext(this, noteLocations);
}

int Document::GetAcceptableResolutionDivider()
{
    QSet<int> locs;
    locs.insert(info.GetResolution());

    // Bar Lines
    for (const auto &bar : std::as_const(barLines)) {
        locs.insert(bar.Location);
    }
    // BPM Events
    for (const auto &bpm : std::as_const(bpmEvents)) {
        locs.insert(bpm.location);
    }
    // TODO: Stop Events
    // SoundChannels
    for (auto channel : std::as_const(soundChannels)){
        locs.unite(channel->GetAllLocations());
    }
    // TODO: BGA

    return ResolutionUtil::GetAcceptableDivider(locs);
}

void Document::ConvertResolutionInternal(int newResolution)
{
    int oldResolution = info.GetResolution();
    info.ForceSetResolution(newResolution);

    // Bar Lines
    auto barLinesOld = barLines;
    barLines.clear();
    for (auto bar : barLinesOld){
        bar.Location = ResolutionUtil::ConvertTicks(bar.Location, newResolution, oldResolution);
        barLines.insert(bar.Location, bar);
    }

    // BPM Events
    auto bpmEventsOld = bpmEvents;
    bpmEvents.clear();
    for (auto bpm : bpmEventsOld){
        bpm.location = ResolutionUtil::ConvertTicks(bpm.location, newResolution, oldResolution);
        bpmEvents.insert(bpm.location, bpm);
    }

    // TODO: Stop Events

    // SoundChannels
    soundChannelLength.clear();
    for (auto channel : std::as_const(soundChannels)){
        channel->ConvertResolution(newResolution, oldResolution);
        soundChannelLength.insert(channel, channel->GetLength());
    }

    // TODO: BGA data

    UpdateTotalLength();

    ReconstructMasterCache();
    emit ResolutionConverted();
}

void Document::ConvertResolution(int newResolution)
{
    int div = GetAcceptableResolutionDivider();
    if (newResolution % (info.GetResolution() / div) == 0){
        auto updater = [this](int value){
            ConvertResolutionInternal(value);
        };
        history->Add(new EditValueAction<int>(updater, info.GetResolution(), newResolution, tr("convert resolution"), true));
    }else{
        // not acceptable (lose information / irriversible)
        history->Clear();
        history->MarkAbsolutelyDirty();
        ConvertResolutionInternal(newResolution);
    }
}

void Document::ReconstructMasterCache()
{
    if (!masterEnabled)
        return;
    master->ClearAll();
    for (auto channel : std::as_const(soundChannels)){
        channel->AddAllIntoMasterCache();
    }
}

Document::DocumentUpdateSoundNotesContext::DocumentUpdateSoundNotesContext(Document *document, QMap<SoundChannel *, QSet<int> > noteLocations)
    : document(document)
{
    // get oldNotes
    for (auto i=noteLocations.begin(); i!=noteLocations.end(); i++){
        const QMap<int,SoundNote> &original = i.key()->notes;
        QMap<int,SoundNote> tmp;
        for (int loc : std::as_const(i.value())){
            tmp.insert(loc, original[loc]);
        }
        oldNotes.insert(i.key(), tmp);
    }
    newNotes = oldNotes;
}

void Document::DocumentUpdateSoundNotesContext::Update(QMap<SoundChannel *, QMap<int, SoundNote> > notes)
{
    // try to update all at once
    QMultiMap<int, SoundNote> notesMerged;
    for (auto i=notes.begin(); i!=notes.end(); i++){
        QMap<int,SoundNote> &n = i.key()->notes;
        for (auto j=i.value().begin(); j!=i.value().end(); j++){
            n.insert(j.key(), j.value());
            notesMerged.insert(j.key(), j.value());
        }
    }

    // NOW DON'T CHECK ANY CONFLICTS
    //if (document->DetectConflictsAroundNotes(notesMerged)){
    //	// rollback (newNotes: latest valid arrangement)
    //	for (auto i=newNotes.begin(); i!=newNotes.end(); i++){
    //		QMap<int,SoundNote> &n = i.key()->notes;
    //		for (auto j=i.value().begin(); j!=i.value().end(); j++){
    //			n.insert(j.key(), j.value());
    //		}
    //	}
    //}else{
        // accept
        newNotes = notes;
        for (auto i=notes.begin(); i!=notes.end(); i++){
            i.key()->UpdateCache();
            i.key()->UpdateVisibleRegionsInternal();
            for (auto j=i.value().begin(); j!=i.value().end(); j++){
                emit i.key()->NoteChanged(j.key(), j.value());
            }
        }
        emit document->AnyNotesChanged();
    //}
}

void Document::DocumentUpdateSoundNotesContext::Cancel()
{
    for (auto i=oldNotes.begin(); i!=oldNotes.end(); i++){
        QMap<int,SoundNote> &n = i.key()->notes;
        for (auto j=i.value().begin(); j!=i.value().end(); j++){
            n.insert(j.key(), j.value());
        }
    }
    for (auto i=oldNotes.begin(); i!=oldNotes.end(); i++){
        i.key()->UpdateCache();
        i.key()->UpdateVisibleRegionsInternal();
        for (auto j=i.value().begin(); j!=i.value().end(); j++){
            emit i.key()->NoteChanged(j.key(), j.value());
        }
    }
}

QMap<SoundChannel *, QMap<int, SoundNote> > Document::DocumentUpdateSoundNotesContext::GetOldNotes() const
{
    return oldNotes;
}

class Document::DocumentUpdateSoundNotesAction : public EditAction
{
    Document *document;
    QMap<SoundChannel*, QMap<int, SoundNote>> oldNotes;
    QMap<SoundChannel*, QMap<int, SoundNote>> newNotes;
public:
    DocumentUpdateSoundNotesAction(Document *document, QMap<SoundChannel*, QMap<int, SoundNote>> oldNotes, QMap<SoundChannel*, QMap<int, SoundNote>> newNotes);
    virtual ~DocumentUpdateSoundNotesAction();
    virtual void Undo();
    virtual void Redo();
    virtual QString GetName();
    virtual void Show();
};

void Document::DocumentUpdateSoundNotesContext::Finish()
{
    if (oldNotes == newNotes){
        return;
    }
    document->history->Add(new DocumentUpdateSoundNotesAction(document, oldNotes, newNotes));
}

Document::DocumentUpdateSoundNotesAction::DocumentUpdateSoundNotesAction(
        Document *document, QMap<SoundChannel *, QMap<int, SoundNote> > oldNotes, QMap<SoundChannel *, QMap<int, SoundNote> > newNotes
        )
    : document(document)
    , oldNotes(oldNotes)
    , newNotes(newNotes)
{
}

Document::DocumentUpdateSoundNotesAction::~DocumentUpdateSoundNotesAction()
{
}

void Document::DocumentUpdateSoundNotesAction::Undo()
{
    for (auto i=oldNotes.begin(); i!=oldNotes.end(); i++){
        QMap<int,SoundNote> &n = i.key()->notes;
        for (auto j=i.value().begin(); j!=i.value().end(); j++){
            n.insert(j.key(), j.value());
        }
    }
    for (auto i=oldNotes.begin(); i!=oldNotes.end(); i++){
        i.key()->UpdateCache();
        i.key()->UpdateVisibleRegionsInternal();
        for (auto j=i.value().begin(); j!=i.value().end(); j++){
            emit i.key()->NoteChanged(j.key(), j.value());
        }
    }
    emit document->AnyNotesChanged();
}

void Document::DocumentUpdateSoundNotesAction::Redo()
{
    for (auto i=newNotes.begin(); i!=newNotes.end(); i++){
        QMap<int,SoundNote> &n = i.key()->notes;
        for (auto j=i.value().begin(); j!=i.value().end(); j++){
            n.insert(j.key(), j.value());
        }
    }
    for (auto i=newNotes.begin(); i!=newNotes.end(); i++){
        i.key()->UpdateCache();
        i.key()->UpdateVisibleRegionsInternal();
        for (auto j=i.value().begin(); j!=i.value().end(); j++){
            emit i.key()->NoteChanged(j.key(), j.value());
        }
    }
    emit document->AnyNotesChanged();
}

QString Document::DocumentUpdateSoundNotesAction::GetName()
{
    return tr("update sound notes");
}

void Document::DocumentUpdateSoundNotesAction::Show()
{
    int minLocation = INT_MAX;
    SoundChannel *channel = nullptr;
    for (auto i=newNotes.begin(); i!=newNotes.end(); i++){
        for (auto j=i.value().begin(); j!=i.value().end(); j++){
            if (j.key() < minLocation){
                channel = i.key();
                minLocation = j.key();
            }
        }
    }
    emit channel->ShowNoteLocation(minLocation);
}







BarLine::BarLine(const QJsonValue &json)
    : BmsonObject(json)
    , Ephemeral(false)
{
    Location = bmsonFields[Bmson::BarLine::LocationKey].toInt();
}

QJsonValue BarLine::SaveBmson()
{
    bmsonFields[Bmson::BarLine::LocationKey] = Location;
    return bmsonFields;
}

QMap<QString, QJsonValue> BarLine::GetExtraFields() const
{
    QMap<QString, QJsonValue> fields;
    for (QJsonObject::const_iterator i=bmsonFields.begin(); i!=bmsonFields.end(); i++){
        if (i.key() != Bmson::BarLine::LocationKey){
            fields.insert(i.key(), i.value());
        }
    }
    return fields;
}

void BarLine::SetExtraFields(const QMap<QString, QJsonValue> &fields)
{
    for (auto i=fields.begin(); i!=fields.end(); i++){
        if (i.key() != Bmson::BarLine::LocationKey){
            bmsonFields[i.key()] = i.value();
        }
    }
}

QJsonObject BarLine::AsJson() const
{
    QJsonObject obj = bmsonFields;
    obj[Bmson::BarLine::LocationKey] = Location;
    return obj;
}

bool BarLine::operator ==(const BarLine &r) const
{
    return AsJson() == r.AsJson();
}

BpmEvent::BpmEvent(const QJsonValue &json)
    : BmsonObject(json)
{
    location = bmsonFields[Bmson::BpmEvent::LocationKey].toInt();
    value = bmsonFields[Bmson::BpmEvent::BpmKey].toDouble();
}

QJsonValue BpmEvent::SaveBmson()
{
    bmsonFields[Bmson::BpmEvent::LocationKey] = location;
    bmsonFields[Bmson::BpmEvent::BpmKey] = value;
    return bmsonFields;
}

QMap<QString, QJsonValue> BpmEvent::GetExtraFields() const
{
    QMap<QString, QJsonValue> fields;
    for (QJsonObject::const_iterator i=bmsonFields.begin(); i!=bmsonFields.end(); i++){
        if (i.key() != Bmson::BpmEvent::LocationKey && i.key() != Bmson::BpmEvent::BpmKey){
            fields.insert(i.key(), i.value());
        }
    }
    return fields;
}

void BpmEvent::SetExtraFields(const QMap<QString, QJsonValue> &fields)
{
    bmsonFields = QJsonObject();
    for (auto i=fields.begin(); i!=fields.end(); i++){
        if (i.key() != Bmson::BpmEvent::LocationKey && i.key() != Bmson::BpmEvent::BpmKey){
            bmsonFields[i.key()] = i.value();
        }
    }
}

QJsonObject BpmEvent::AsJson() const
{
    QJsonObject obj = bmsonFields;
    obj[Bmson::BpmEvent::LocationKey] = location;
    obj[Bmson::BpmEvent::BpmKey] = value;
    return obj;
}

bool BpmEvent::operator ==(const BpmEvent &r) const
{
    return AsJson() == r.AsJson();
}

StopEvent::StopEvent(const QJsonValue &json)
    : BmsonObject(json)
{
    location = bmsonFields[Bmson::StopEvent::LocationKey].toInt();
    value = bmsonFields[Bmson::StopEvent::DurationKey].toDouble();
}

QJsonValue StopEvent::SaveBmson()
{
    bmsonFields[Bmson::StopEvent::LocationKey] = location;
    bmsonFields[Bmson::StopEvent::DurationKey] = value;
    return bmsonFields;
}

QMap<QString, QJsonValue> StopEvent::GetExtraFields() const
{
    QMap<QString, QJsonValue> fields;
    for (QJsonObject::const_iterator i=bmsonFields.begin(); i!=bmsonFields.end(); i++){
        if (i.key() != Bmson::StopEvent::LocationKey && i.key() != Bmson::StopEvent::DurationKey){
            fields.insert(i.key(), i.value());
        }
    }
    return fields;
}

void StopEvent::SetExtraFields(const QMap<QString, QJsonValue> &fields)
{
    bmsonFields = QJsonObject();
    for (auto i=fields.begin(); i!=fields.end(); i++){
        if (i.key() != Bmson::StopEvent::LocationKey && i.key() != Bmson::StopEvent::DurationKey){
            bmsonFields[i.key()] = i.value();
        }
    }
}

QJsonObject StopEvent::AsJson() const
{
    QJsonObject obj = bmsonFields;
    obj[Bmson::StopEvent::LocationKey] = location;
    obj[Bmson::StopEvent::DurationKey] = value;
    return obj;
}

bool StopEvent::operator ==(const StopEvent &r) const
{
    return AsJson() == r.AsJson();
}
