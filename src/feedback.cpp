/*
    Touch'n'learn - Fun and easy mobile lessons for kids
    Copyright (C) 2010, 2011 by Alessandro Portale
    http://touchandlearn.sourceforge.net

    This file is part of Touch'n'learn

    Touch'n'learn is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Touch'n'learn is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Touch'n'learn; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "feedback.h"
#include <QtCore/QDir>
#include <QtCore/QTimer>

#ifdef USING_QT_MOBILITY
#include <QMediaPlayer>
#else // USING_QT_MOBILITY
#include <phonon/MediaObject>
#include <phonon/AudioOutput>
#endif // USING_QT_MOBILITY

static QString dataPath = QLatin1String("data");

#if defined(Q_OS_SYMBIAN)
#include <remconcoreapitargetobserver.h>    // link against RemConCoreApi.lib
#include <remconcoreapitarget.h>            // and
#include <remconinterfaceselector.h>        // RemConInterfaceBase.lib

class VolumeKeyListener : public QObject, public MRemConCoreApiTargetObserver
{
public:
    VolumeKeyListener(Feedback *feedback);
    ~VolumeKeyListener();
    virtual void MrccatoCommand(TRemConCoreApiOperationId aOperationId,
                                TRemConCoreApiButtonAction aButtonAct);

    void volumeUp();
    void volumeDown();

private:
    CRemConCoreApiTarget *m_iCoreTarget;
    CRemConInterfaceSelector *m_iInterfaceSelector;
    Feedback *m_feedback;
};

VolumeKeyListener::VolumeKeyListener(Feedback *feedback)
    : QObject(feedback)
    , m_feedback(feedback)
{
    QT_TRAP_THROWING(m_iInterfaceSelector = CRemConInterfaceSelector::NewL());
    QT_TRAP_THROWING(m_iCoreTarget = CRemConCoreApiTarget::NewL(*m_iInterfaceSelector, *this));
    m_iInterfaceSelector->OpenTargetL();
}

VolumeKeyListener::~VolumeKeyListener()
{
    delete m_iInterfaceSelector;
}

void VolumeKeyListener::MrccatoCommand(TRemConCoreApiOperationId aOperationId,
                                       TRemConCoreApiButtonAction aButtonAct)
{
    Q_UNUSED(aButtonAct)
    switch(aOperationId) {
    case ERemConCoreApiVolumeUp:
        volumeUp();
        break;
    case ERemConCoreApiVolumeDown:
        volumeDown();
        break;
    default:
        break;
    }
}
#else // Q_OS_SYMBIAN
#include <QtGui/QShortcut>
#include <QtGui/QApplication>

class VolumeKeyListener : public QObject
{
    Q_OBJECT

public:
    VolumeKeyListener(Feedback *feedback);

private slots:
    void volumeUp();
    void volumeDown();

private:
    Feedback *m_feedback;
};

VolumeKeyListener::VolumeKeyListener(Feedback *feedback)
    : QObject(feedback)
    , m_feedback(feedback)
{
#ifndef Q_OS_LINUX
    QShortcut *volumeUp = new QShortcut(QKeySequence(Qt::Key_Plus), qApp->activeWindow());
    connect(volumeUp, SIGNAL(activated()), SLOT(volumeUp()));

    QShortcut *volumeDown = new QShortcut(QKeySequence(Qt::Key_Minus), qApp->activeWindow());
    connect(volumeDown, SIGNAL(activated()), SLOT(volumeDown()));
#endif // Q_OS_LINUX
}
#endif // Q_OS_SYMBIAN

Feedback::Feedback(QObject *parent)
    : QObject(parent)
    , m_previousCorrectSound(0)
    , m_previousIncorrectSound(0)
    , m_audioVolume(100)
{
    QTimer::singleShot(1, this, SLOT(init()));
}

Feedback::~Feedback()
{
    qDeleteAll(m_correctSounds);
    qDeleteAll(m_incorrectSounds);
}

void Feedback::setDataPath(const QString &path)
{
    dataPath = path;
}

int Feedback::audioVolume() const
{
    return m_audioVolume;
}

void Feedback::setAudioVolume(int volume, bool emitChangedSignal)
{
    m_audioVolume = qBound(0, volume, 100);
    if (emitChangedSignal)
        emit volumeChanged(m_audioVolume);
}

#ifdef USING_QT_MOBILITY
static void playSound(const QList<QMediaPlayer*> &sounds, QMediaPlayer* &previousSound, int volume)
{
    if (sounds.isEmpty())
        return;

    QMediaPlayer *currentSound = 0;
    if (sounds.count() == 1) {
        currentSound = sounds.first();
    } else {
        do {
            const int index = qrand() % sounds.count();
            currentSound = sounds.at(index);
        } while (currentSound == previousSound);
        previousSound = currentSound;
    }
    currentSound->setVolume(volume);
    currentSound->stop();
    currentSound->play();
}

static QMediaPlayer *player(const QString &file)
{
    QMediaPlayer *result = new QMediaPlayer;
    QMediaContent content(QUrl::fromLocalFile(file));
    result->setMedia(content);
    return result;
}
#else // USING_QT_MOBILITY
static void playSound(const QList<Phonon::MediaObject*> &sounds, Phonon::MediaObject* &previousSound, int volume)
{
    Q_UNUSED(volume)

    if (sounds.isEmpty())
        return;

    Phonon::MediaObject *currentSound = 0;
    if (sounds.count() == 1) {
        currentSound = sounds.first();
    } else {
        do {
            const int index = qrand() % sounds.count();
            currentSound = sounds.at(index);
        } while (currentSound == previousSound);
        previousSound = currentSound;
    }
    currentSound->stop();
    currentSound->seek(0);
    currentSound->play();
}

static Phonon::MediaObject *player(const QString &file)
{
    Phonon::MediaObject *result = new Phonon::MediaObject();
    result->setCurrentSource(Phonon::MediaSource(file));
    Phonon::AudioOutput *audioOutput = new Phonon::AudioOutput(Phonon::MusicCategory, result);
    Phonon::createPath(result, audioOutput);
    return result;
}
#endif // USING_QT_MOBILITY

void Feedback::init()
{
    new VolumeKeyListener(this);
    const QDir path(dataPath);
    foreach (const QFileInfo &midiFile, path.entryInfoList(QDir::Files)) {
        if (midiFile.fileName().startsWith(QLatin1String("correct")))
            m_correctSounds.append(player(midiFile.absoluteFilePath()));
        else if (midiFile.fileName().startsWith(QLatin1String("incorrect")))
            m_incorrectSounds.append(player(midiFile.absoluteFilePath()));
    }
}

void Feedback::playCorrectSound() const
{
    playSound(m_correctSounds, m_previousCorrectSound, m_audioVolume);
}

void Feedback::playIncorrectSound() const
{
    playSound(m_incorrectSounds, m_previousIncorrectSound, m_audioVolume);
}

void VolumeKeyListener::volumeUp()
{
    m_feedback->setAudioVolume(m_feedback->audioVolume() + 20);
}

void VolumeKeyListener::volumeDown()
{
    m_feedback->setAudioVolume(m_feedback->audioVolume() - 20);
}

#include "feedback.moc"
