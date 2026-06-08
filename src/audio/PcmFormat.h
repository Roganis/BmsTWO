#ifndef PCMFORMAT_H
#define PCMFORMAT_H

#include <QtGlobal>
#include <QString>

// Portable PCM format descriptor used by the WAV/OGG decoders.
//
// Qt6's QAudioFormat can only represent UInt8/Int16/Int32/Float and dropped
// sampleSize()/sampleType(); it cannot describe the intermediate decode
// formats the decoders handle (e.g. 24-bit, unsigned 16-bit). This mirrors the
// subset of the old Qt5 QAudioFormat API the decoders rely on, so the decode
// logic is unchanged across Qt versions. (The playback sink still uses a real
// QAudioFormat; this type is only the decoders' internal descriptor.)
struct PcmFormat
{
	enum SampleType { Unknown, SignedInt, UnSignedInt, Float };
	enum Endian { LittleEndian, BigEndian };

	int m_sampleRate = 0;
	int m_channelCount = 0;
	int m_sampleSize = 0; // bits per sample
	SampleType m_sampleType = Unknown;

	void setSampleRate(int v) { m_sampleRate = v; }
	void setChannelCount(int v) { m_channelCount = v; }
	void setSampleSize(int v) { m_sampleSize = v; }
	void setSampleType(SampleType v) { m_sampleType = v; }
	void setByteOrder(Endian) {}      // decoders always produce little-endian
	void setCodec(const QString &) {} // always linear PCM

	int sampleRate() const { return m_sampleRate; }
	int channelCount() const { return m_channelCount; }
	int sampleSize() const { return m_sampleSize; }
	SampleType sampleType() const { return m_sampleType; }
	Endian byteOrder() const { return LittleEndian; }

	int bytesPerSample() const { return m_sampleSize / 8; }
	int bytesPerFrame() const { return m_channelCount * (m_sampleSize / 8); }
	qint32 bytesForFrames(qint32 frameCount) const { return frameCount * bytesPerFrame(); }

	bool isValid() const { return m_sampleRate > 0 && m_channelCount > 0 && m_sampleSize > 0; }

	bool operator==(const PcmFormat &o) const
	{
		return m_sampleRate == o.m_sampleRate && m_channelCount == o.m_channelCount
			&& m_sampleSize == o.m_sampleSize && m_sampleType == o.m_sampleType;
	}
	bool operator!=(const PcmFormat &o) const { return !(*this == o); }
};

#endif // PCMFORMAT_H
