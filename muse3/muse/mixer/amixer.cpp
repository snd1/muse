//=========================================================
//  MusE
//  Linux Music Editor
//  $Id: amixer.cpp,v 1.49.2.5 2009/11/16 01:55:55 terminator356 Exp $
//
//  (C) Copyright 2000-2004 Werner Schweer (ws@seh.de)
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; version 2 of
//  the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//=========================================================

#include "muse_math.h"

#include <QApplication>
#include <QMenuBar>
#include <QPaintEvent>
#include <QActionGroup>

#include "amixer.h"
#include "app.h"
#include "helper.h"
#include "icons.h"
#include "song.h"
#include "audio.h"

#include "astrip.h"
#include "mstrip.h"
#include "track.h"

#include "xml.h"

#define __WIDTH_COMPENSATION 4

// For debugging output: Uncomment the fprintf section.
#define DEBUG_MIXER(dev, format, args...)  // fprintf(dev, format, ##args);

namespace MusEGui {

bool ScrollArea::viewportEvent(QEvent* event)
{
  DEBUG_MIXER(stderr, "viewportEvent type %d\n", (int)event->type());
//   event->ignore();
  // Let it do the layout now, before we emit.
  const bool res = QScrollArea::viewportEvent(event);
  
  if(event->type() == QEvent::LayoutRequest)       
    emit layoutRequest();
         
//   return false;
//   return true;
  return res;
}

//---------------------------------------------------------
//   AudioMixer
//
//    inputs | synthis | tracks | groups | master
//---------------------------------------------------------

AudioMixerApp::AudioMixerApp(QWidget* parent, MusEGlobal::MixerConfig* c)
   : QMainWindow(parent)
{
      _resizeFlag = false;
      _preferKnobs = MusEGlobal::config.preferKnobsVsSliders;
      cfg = c;
      oldAuxsSize = 0;
      routingDialog = 0;
      setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding));
      setWindowTitle(cfg->name);
      setWindowIcon(*museIcon);

      //cfg->displayOrder = MusEGlobal::MixerConfig::STRIPS_TRADITIONAL_VIEW;

      QMenu* menuConfig = menuBar()->addMenu(tr("&Create"));
      MusEGui::populateAddTrack(menuConfig,true);
      connect(menuConfig, &QMenu::triggered, [](QAction* a) { MusEGlobal::song->addNewTrack(a); } );
      
      QMenu* menuView = menuBar()->addMenu(tr("&View"));
      menuStrips = menuView->addMenu(tr("Strips"));
      connect(menuStrips, &QMenu::aboutToShow, [this]() { stripsMenu(); } );

      routingId = menuView->addAction(tr("Routing"));
      routingId->setCheckable(true);
      connect(routingId, &QAction::triggered, [this]() { toggleRouteDialog(); } );

      menuView->addSeparator();

      QActionGroup* actionItems = new QActionGroup(this);
      actionItems->setExclusive(false);
      
      showMidiTracksId = new QAction(tr("Show Midi Tracks"), actionItems);
//      showDrumTracksId = new QAction(tr("Show Drum Tracks"), actionItems);
      showNewDrumTracksId = new QAction(tr("Show Drum Tracks"), actionItems);
      showWaveTracksId = new QAction(tr("Show Wave Tracks"), actionItems);

      QAction *separator = new QAction(this);
      separator->setSeparator(true);
      actionItems->addAction(separator);

      showInputTracksId = new QAction(tr("Show Inputs"), actionItems);
      showOutputTracksId = new QAction(tr("Show Outputs"), actionItems);
      showGroupTracksId = new QAction(tr("Show Groups"), actionItems);
      showAuxTracksId = new QAction(tr("Show Auxs"), actionItems);
      showSyntiTracksId = new QAction(tr("Show Synthesizers"), actionItems);

//      showDrumTracksId->setVisible(false);
      
      showMidiTracksId->setCheckable(true);
//      showDrumTracksId->setCheckable(true);
      showNewDrumTracksId->setCheckable(true);
      showWaveTracksId->setCheckable(true);
      showInputTracksId->setCheckable(true);
      showOutputTracksId->setCheckable(true);
      showGroupTracksId->setCheckable(true);
      showAuxTracksId->setCheckable(true);
      showSyntiTracksId->setCheckable(true);

      connect(showMidiTracksId,    &QAction::triggered, [this](bool v) { showMidiTracksChanged(v); } );
//      connect(showDrumTracksId,    &QAction::triggered, [this](bool v) { showDrumTracksChanged(v); } );
      connect(showNewDrumTracksId, &QAction::triggered, [this](bool v) { showNewDrumTracksChanged(v); } );
      connect(showWaveTracksId,    &QAction::triggered, [this](bool v) { showWaveTracksChanged(v); } );
      connect(showInputTracksId,   &QAction::triggered, [this](bool v) { showInputTracksChanged(v); } );
      connect(showOutputTracksId,  &QAction::triggered, [this](bool v) { showOutputTracksChanged(v); } ); 
      connect(showGroupTracksId,   &QAction::triggered, [this](bool v) { showGroupTracksChanged(v); } );
      connect(showAuxTracksId,     &QAction::triggered, [this](bool v) { showAuxTracksChanged(v); } );
      connect(showSyntiTracksId,   &QAction::triggered, [this](bool v) { showSyntiTracksChanged(v); } );

      menuView->addActions(actionItems->actions());
      
      ///view = new QScrollArea();
      view = new ScrollArea();
      view->setFocusPolicy(Qt::NoFocus);
      view->setContentsMargins(0, 0, 0, 0);
      //view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
      setCentralWidget(view);
      
      central = new QWidget(view);
      central->setFocusPolicy(Qt::NoFocus);
      central->setContentsMargins(0, 0, 0, 0);
      //splitter = new QSplitter(view);
      mixerLayout = new QHBoxLayout();
      central->setLayout(mixerLayout);
      mixerLayout->setSpacing(0);
      mixerLayout->setContentsMargins(0, 0, 0, 0);
      view->setWidget(central);
      //view->setWidget(splitter);
      view->setWidgetResizable(true);
      
      // FIXME: Neither of these two replacement functor version work. What's wrong here?
      connect(view, SIGNAL(layoutRequest()), SLOT(setSizing()));  
      //connect(view, &ScrollArea::layoutRequest, [this]() { setSizing(); } );
      //connect(view, QOverload<>::of(&ScrollArea::layoutRequest), [=]() { setSizing(); } );
      
      connect(MusEGlobal::song, &MusECore::Song::songChanged, [this](MusECore::SongChangedStruct_t f) { songChanged(f); } );
      connect(MusEGlobal::muse, &MusEGui::MusE::configChanged, [this]() { configChanged(); } );

      initMixer();
      redrawMixer();

      central->installEventFilter(this);
      mixerLayout->installEventFilter(this);
      view ->installEventFilter(this);

}

void AudioMixerApp::stripsMenu()
{
  menuStrips->clear();
  connect(menuStrips, &QMenu::triggered, [this](QAction* a) { handleMenu(a); } );
  QAction *act;

  act = menuStrips->addAction(tr("Traditional order"));
  act->setData(MusEGlobal::MixerConfig::STRIPS_TRADITIONAL_VIEW);
  act->setCheckable(true);
  if (cfg->displayOrder == MusEGlobal::MixerConfig::STRIPS_TRADITIONAL_VIEW)
    act->setChecked(true);

  act = menuStrips->addAction(tr("Arranger order"));
  act->setData(MusEGlobal::MixerConfig::STRIPS_ARRANGER_VIEW);
  act->setCheckable(true);
  if (cfg->displayOrder == MusEGlobal::MixerConfig::STRIPS_ARRANGER_VIEW)
    act->setChecked(true);

  act = menuStrips->addAction(tr("User order"));
  act->setData(MusEGlobal::MixerConfig::STRIPS_EDITED_VIEW);
  act->setCheckable(true);
  if (cfg->displayOrder == MusEGlobal::MixerConfig::STRIPS_EDITED_VIEW)
    act->setChecked(true);

  menuStrips->addSeparator();
  act = menuStrips->addAction(tr("Show all hidden strips"));
  act->setData(UNHIDE_STRIPS);
  menuStrips->addSeparator();

  // loop through all tracks and show the hidden ones
  int i=0,h=0;
  foreach (Strip *s, stripList) {
    if (!s->getStripVisible()){
      act = menuStrips->addAction(tr("Unhide strip: ") + s->getTrack()->name());
      act->setData(i);
      h++;
    }
    i++;
  }
  if (h==0) {
    act = menuStrips->addAction(tr("(no hidden strips)"));
    act->setData(UNHANDLED_NUMBER);
  }
}

void AudioMixerApp::handleMenu(QAction *act)
{
  DEBUG_MIXER(stderr, "handleMenu %d\n", act->data().toInt());
  int operation = act->data().toInt();
  if (operation >= 0) {
    Strip* s = stripList.at(act->data().toInt());
    s->setStripVisible(true);
    stripVisibleChanged(s, true);
  } else if (operation ==  UNHIDE_STRIPS) {
    foreach (Strip *s, stripList) {
      s->setStripVisible(true);
      stripVisibleChanged(s, true);
    }
  } else if (operation == MusEGlobal::MixerConfig::STRIPS_TRADITIONAL_VIEW) {
    cfg->displayOrder = MusEGlobal::MixerConfig::STRIPS_TRADITIONAL_VIEW;
  } else if (operation == MusEGlobal::MixerConfig::STRIPS_ARRANGER_VIEW) {
    cfg->displayOrder = MusEGlobal::MixerConfig::STRIPS_ARRANGER_VIEW;
  } else if (operation == MusEGlobal::MixerConfig::STRIPS_EDITED_VIEW) {
    cfg->displayOrder = MusEGlobal::MixerConfig::STRIPS_EDITED_VIEW;
  }
  redrawMixer();
}

bool AudioMixerApp::stripIsVisible(Strip* s)
{
  if (!s->getStripVisible())
    return false;

  MusECore::Track *t = s->getTrack();
  switch (t->type())
  {
    case MusECore::Track::AUDIO_SOFTSYNTH:
      if (!cfg->showSyntiTracks)
        return false;
      break;
    case MusECore::Track::AUDIO_INPUT:
      if (!cfg->showInputTracks)
        return false;
      break;
    case MusECore::Track::AUDIO_OUTPUT:
      if (!cfg->showOutputTracks)
        return false;
      break;
    case MusECore::Track::AUDIO_AUX:
      if (!cfg->showAuxTracks)
        return false;
      break;
    case MusECore::Track::AUDIO_GROUP:
      if (!cfg->showGroupTracks)
        return false;
      break;
    case MusECore::Track::WAVE:
      if (!cfg->showWaveTracks)
        return false;
      break;
    case MusECore::Track::MIDI:
    case MusECore::Track::NEW_DRUM:
      if (!cfg->showMidiTracks)
        return false;
      break;

  }
  return true;
}

void AudioMixerApp::redrawMixer()
{
  DEBUG_MIXER(stderr, "redrawMixer type %d, mixerLayout count %d\n", cfg->displayOrder, mixerLayout->count());
  // empty layout
  while (mixerLayout->count() > 0) {
    mixerLayout->removeItem(mixerLayout->itemAt(0));
  }

  switch (cfg->displayOrder) {
    case MusEGlobal::MixerConfig::STRIPS_ARRANGER_VIEW:
      {
        DEBUG_MIXER(stderr, "Draw strips with arranger view\n");
        MusECore::TrackList *tl = MusEGlobal::song->tracks();
        MusECore::TrackList::iterator tli = tl->begin();
        for (; tli != tl->end(); tli++) {
          DEBUG_MIXER(stderr, "Adding strip %s\n", (*tli)->name().toLatin1().data());
          StripList::iterator si = stripList.begin();
          for (; si != stripList.end(); si++) {
            if((*si)->getTrack() == *tli) {
              addStripToLayoutIfVisible(*si);
            }
          }
        }
      }
      break;
    case MusEGlobal::MixerConfig::STRIPS_EDITED_VIEW:
      {
        DEBUG_MIXER(stderr, "Draw strips with edited view\n");
        // add them back in the selected order
        StripList::iterator si = stripList.begin();
        for (; si != stripList.end(); ++si) {
            DEBUG_MIXER(stderr, "Adding strip %s\n", (*si)->getTrack()->name().toLatin1().data());
            addStripToLayoutIfVisible(*si);
        }
        DEBUG_MIXER(stderr, "mixerLayout count is now %d\n", mixerLayout->count());
      }
      break;
    case MusEGlobal::MixerConfig::STRIPS_TRADITIONAL_VIEW:
      {
        DEBUG_MIXER(stderr, "TRADITIONAL VIEW mixerLayout count is now %d\n", mixerLayout->count());
        addStripsTraditionalLayout();
      }

      break;
  }

  // Setup all tabbing for each strip, and between strips.
  setupComponentTabbing();

  update();
}

Strip* AudioMixerApp::findStripForTrack(StripList &sl, MusECore::Track *t)
{
  StripList::iterator si = sl.begin();
  for (;si != sl.end(); si++) {
    if ((*si)->getTrack() == t)
      return *si;
  }
  DEBUG_MIXER(stderr, "AudioMixerApp::findStripForTrack - ERROR: there was no strip for this track!\n");
  return NULL;
}

void AudioMixerApp::fillStripListTraditional()
{
  StripList oldList = stripList;
  stripList.clear();
  MusECore::TrackList *tl = MusEGlobal::song->tracks();

  //  add Input Strips
  MusECore::TrackList::iterator tli = tl->begin();
  for (; tli != tl->end(); ++tli) {
    if ((*tli)->type() == MusECore::Track::AUDIO_INPUT)
      stripList.append(findStripForTrack(oldList,*tli));
  }

  //  Synthesizer Strips
  tli = tl->begin();
  for (; tli != tl->end(); ++tli) {
    if ((*tli)->type() == MusECore::Track::AUDIO_SOFTSYNTH)
      stripList.append(findStripForTrack(oldList,*tli));
  }

  //  generate Wave Track Strips
  tli = tl->begin();
  for (; tli != tl->end(); ++tli) {
    if ((*tli)->type() == MusECore::Track::WAVE)
      stripList.append(findStripForTrack(oldList,*tli));
  }

  //  generate Midi channel/port Strips
  tli = tl->begin();
  for (; tli != tl->end(); ++tli) {
    if ((*tli)->type() == MusECore::Track::MIDI ||
        (*tli)->type() == MusECore::Track::NEW_DRUM)
      stripList.append(findStripForTrack(oldList,*tli));
  }

  //  Groups
  tli = tl->begin();
  for (; tli != tl->end(); ++tli) {
    if ((*tli)->type() == MusECore::Track::AUDIO_GROUP)
      stripList.append(findStripForTrack(oldList,*tli));
  }

  //  Aux
  tli = tl->begin();
  for (; tli != tl->end(); ++tli) {
    if ((*tli)->type() == MusECore::Track::AUDIO_AUX)
      stripList.append(findStripForTrack(oldList,*tli));
  }

  //    Master
  tli = tl->begin();
  for (; tli != tl->end(); ++tli) {
    if ((*tli)->type() == MusECore::Track::AUDIO_OUTPUT)
      stripList.append(findStripForTrack(oldList,*tli));
  }
}


void AudioMixerApp::moveStrip(Strip *s)
{
  DEBUG_MIXER(stderr, "Recreate stripList\n");
  if (cfg->displayOrder == MusEGlobal::MixerConfig::STRIPS_ARRANGER_VIEW) {

    const int tracks_sz = MusEGlobal::song->tracks()->size();
    for (int i=0; i< stripList.size(); i++)
    {
      Strip *s2 = stripList.at(i);
      if (s2 == s) continue;

      DEBUG_MIXER(stderr, "loop loop %d %d width %d\n", s->pos().x(),s2->pos().x(), s2->width());
      if (s->pos().x()+s->width()/2 < s2->pos().x()+s2->width() // upper limit
          && s->pos().x()+s->width()/2 > s2->pos().x() ) // lower limit
      {
        // found relevant pos.
        int sTrack = MusEGlobal::song->tracks()->index(s->getTrack());
        int dTrack = MusEGlobal::song->tracks()->index(s2->getTrack());
        if (sTrack >= 0 && dTrack >= 0)   // sanity check
        {
          if (sTrack < tracks_sz && dTrack < tracks_sz)   // sanity check
            MusEGlobal::song->applyOperation(MusECore::UndoOp(MusECore::UndoOp::MoveTrack, sTrack, dTrack));
        }
      }
    }

  } else if (cfg->displayOrder == MusEGlobal::MixerConfig::STRIPS_TRADITIONAL_VIEW)
  {
    fillStripListTraditional();
    cfg->displayOrder = MusEGlobal::MixerConfig::STRIPS_EDITED_VIEW;
  }
  DEBUG_MIXER(stderr, "moveStrip %s! stripList.size = %d\n", s->getLabelText().toLatin1().data(), stripList.size());

  for (int i=0; i< stripList.size(); i++)
  {
    Strip *s2 = stripList.at(i);
    // Ignore the given strip, or invisible strips.
    if(s2 == s || !s2->getStripVisible()) continue;

    DEBUG_MIXER(stderr, "loop loop %d %d width %d\n", s->pos().x(),s2->pos().x(), s2->width());
    if (s->pos().x()+s->width()/2 < s2->pos().x()+s2->width() // upper limit
        && s->pos().x()+s->width()/2 > s2->pos().x() ) // lower limit
    {
      DEBUG_MIXER(stderr, "got new pos: %d\n", i);
#if DEBUG_MIXER
      bool isSuccess = stripList.removeOne(s);
      DEBUG_MIXER(stderr, "Removed strip %d", isSuccess);
#else
      stripList.removeOne(s);
#endif
      stripList.insert(i,s);
      DEBUG_MIXER(stderr, "Inserted strip at %d", i);
      // Move the corresponding config list item.
      moveConfig(s, i);
      break;
    }
  }
  redrawMixer();
  update();
}

void AudioMixerApp::addStripToLayoutIfVisible(Strip *s)
{
  if (stripIsVisible(s)) {
    s->setVisible(true);
    stripVisibleChanged(s, true);
    mixerLayout->addWidget(s);
  } else {
    s->setVisible(false);
    stripVisibleChanged(s, false);
  }
}

void AudioMixerApp::addStripsTraditionalLayout()
{
  //  generate Input Strips
  StripList::iterator si = stripList.begin();
  for (; si != stripList.end(); ++si) {
    if ((*si)->getTrack()->type() == MusECore::Track::AUDIO_INPUT)
      addStripToLayoutIfVisible(*si);
  }

  //  Synthesizer Strips
  si = stripList.begin();
  for (; si != stripList.end(); ++si) {
    if ((*si)->getTrack()->type() == MusECore::Track::AUDIO_SOFTSYNTH)
      addStripToLayoutIfVisible(*si);
  }

  //  generate Wave Track Strips
  si = stripList.begin();
  for (; si != stripList.end(); ++si) {
    if ((*si)->getTrack()->type() == MusECore::Track::WAVE)
      addStripToLayoutIfVisible(*si);
  }

  //  generate Midi channel/port Strips
  si = stripList.begin();
  for (; si != stripList.end(); ++si) {
    if ((*si)->getTrack()->type() == MusECore::Track::MIDI ||
        (*si)->getTrack()->type() == MusECore::Track::NEW_DRUM)
      addStripToLayoutIfVisible(*si);
  }

  //  Groups
  si = stripList.begin();
  for (; si != stripList.end(); ++si) {
    if ((*si)->getTrack()->type() == MusECore::Track::AUDIO_GROUP)
      addStripToLayoutIfVisible(*si);
  }

  //  Aux
  si = stripList.begin();
  for (; si != stripList.end(); ++si) {
    if ((*si)->getTrack()->type() == MusECore::Track::AUDIO_AUX)
      addStripToLayoutIfVisible(*si);
  }

  //    Master
  si = stripList.begin();
  for (; si != stripList.end(); ++si) {
    if ((*si)->getTrack()->type() == MusECore::Track::AUDIO_OUTPUT)
      addStripToLayoutIfVisible(*si);
  }
}

void AudioMixerApp::stripVisibleChanged(Strip* s, bool v)
{
  const MusECore::Track* t = s->getTrack();
  const int sn = t->serial();
  if (!cfg->stripConfigList.empty()) {
    const int sz = cfg->stripConfigList.size();
    for (int i=0; i < sz; i++) {
      MusEGlobal::StripConfig& sc = cfg->stripConfigList[i];
      DEBUG_MIXER(stderr, "stripVisibleChanged() processing strip [%d][%d]\n", sc._serial, sc._visible);
      if(!sc.isNull() && sc._serial == sn) {
          sc._visible = v;
          return;
        }
      }
    }
  fprintf(stderr, "stripVisibleChanged() StripConfig not found [%d]\n", sn);
}

void AudioMixerApp::stripUserWidthChanged(Strip* s, int w)
{
  const MusECore::Track* t = s->getTrack();
  const int sn = t->serial();
  if (!cfg->stripConfigList.empty()) {
    const int sz = cfg->stripConfigList.size();
    for (int i=0; i < sz; i++) {
      MusEGlobal::StripConfig& sc = cfg->stripConfigList[i];
      DEBUG_MIXER(stderr, "stripUserWidthChanged() processing strip [%d][%d]\n", sc._serial, sc._width);
      if(!sc.isNull() && sc._serial == sn) {
          sc._width = w;
          return;
        }
      }
    }
  fprintf(stderr, "stripUserWidthChanged() StripConfig not found [%d]\n", sn);
}

void AudioMixerApp::setSizing()
{
  DEBUG_MIXER(stderr, "setSizing\n");
      int w = 0;
      
      w = mixerLayout->minimumSize().width();
      
      if(const QStyle* st = style())
      {
        st = st->proxy();
        w += 2 * st->pixelMetric(QStyle::PM_DefaultFrameWidth);
      }
      
      if(w < 40)
        w = 40;
      view->setUpdatesEnabled(false);
      setUpdatesEnabled(false);
      if(stripList.size() <= 6)
        setMinimumWidth(w);

      // Hack flag to prevent overwriting the config geometry when resizing...
      _resizeFlag = true;
      // At this point the width may be UNEXPANDABLE and may be small !
      // This line forces the maximum width, to at least ALLOW expansion ...
      setMaximumWidth(w);
      // ... and now, this line restores the desired size.
      if(size() != cfg->geometry.size())
        resize(cfg->geometry.size());
      // Now reset the flag in case it wasn't already by the resize handler...
      _resizeFlag = false;
      
      setUpdatesEnabled(true);
      view->setUpdatesEnabled(true);

}

//---------------------------------------------------------
//   addStrip
//---------------------------------------------------------

void AudioMixerApp::addStrip(const MusECore::Track* t, const MusEGlobal::StripConfig& sc, int insert_pos)
{
    DEBUG_MIXER(stderr, "addStrip\n");
    Strip* strip;

    // Make them non-embedded: Moveable, hideable, and with an expander handle.
    if (t->isMidiTrack())
          strip = new MidiStrip(central, (MusECore::MidiTrack*)t, true, false);
    else
          strip = new AudioStrip(central, (MusECore::AudioTrack*)t, true, false);

    // Broadcast changes to other selected tracks.
    strip->setBroadcastChanges(true);

    // Set focus yielding to the mixer window.
    if(MusEGlobal::config.smartFocus)
    {
      strip->setFocusYieldWidget(this);
      //strip->setFocusPolicy(Qt::WheelFocus);
    }

    connect(strip, &Strip::clearStripSelection, [this]() { clearStripSelection(); } );
    connect(strip, &Strip::moveStrip, [this](Strip* s) { moveStrip(s); } );
    connect(strip, &Strip::visibleChanged, [this](Strip* s, bool v) { stripVisibleChanged(s, v); } );
    connect(strip, &Strip::userWidthChanged, [this](Strip* s, int w) { stripUserWidthChanged(s, w); } );

    // No insert position given?
    if(insert_pos == -1)
    {
      DEBUG_MIXER(stderr, "putting new strip [%s] at end\n", t->name().toLatin1().data());
      stripList.append(strip);
    }
    else
    {
      DEBUG_MIXER(stderr, "inserting new strip [%s] at %d\n", t->name().toLatin1().data(), insert_pos);
      stripList.insert(insert_pos, strip);
    }

    strip->setVisible(sc._visible);
    strip->setStripVisible(sc._visible);
    if(sc._width >= 0)
      strip->setUserWidth(sc._width);

    // Create a strip config now if no strip config exists yet for this strip (sc is default, with invalid sn).
    if(sc.isNull())
    {
      const MusEGlobal::StripConfig sc(t->serial(), strip->getStripVisible(), strip->userWidth());
      cfg->stripConfigList.append(sc);
    }
}

//---------------------------------------------------------
//   clearAndDelete
//---------------------------------------------------------

void AudioMixerApp::clearAndDelete()
{
  DEBUG_MIXER(stderr, "clearAndDelete\n");
  StripList::iterator si = stripList.begin();
  for (; si != stripList.end(); ++si)
  {
    mixerLayout->removeWidget(*si);
    //(*si)->deleteLater();
    delete (*si);
  }

  cfg->stripConfigList.clear();
  stripList.clear();
  // Obsolete. Support old files.
  cfg->stripOrder.clear();
  oldAuxsSize = -1;
}

//---------------------------------------------------------
//   initMixer
//---------------------------------------------------------

void AudioMixerApp::initMixer()
{
  DEBUG_MIXER(stderr, "initMixer %d\n", cfg->stripOrder.empty() ? cfg->stripConfigList.size() : cfg->stripOrder.size());
  setWindowTitle(cfg->name);
  //clearAndDelete();

  showMidiTracksId->setChecked(cfg->showMidiTracks);
//  showDrumTracksId->setChecked(cfg->showDrumTracks);
  showNewDrumTracksId->setChecked(cfg->showNewDrumTracks);
  showInputTracksId->setChecked(cfg->showInputTracks);
  showOutputTracksId->setChecked(cfg->showOutputTracks);
  showWaveTracksId->setChecked(cfg->showWaveTracks);
  showGroupTracksId->setChecked(cfg->showGroupTracks);
  showAuxTracksId->setChecked(cfg->showAuxTracks);
  showSyntiTracksId->setChecked(cfg->showSyntiTracks);

  int auxsSize = MusEGlobal::song->auxs()->size();
  oldAuxsSize = auxsSize;
  const MusECore::TrackList *tl = MusEGlobal::song->tracks();

  // NOTE: stripOrder string list is obsolete. Must support old songs.
  if (!cfg->stripOrder.empty()) {
    const int sz = cfg->stripOrder.size();
    for (int i=0; i < sz; i++) {
      DEBUG_MIXER(stderr, "processing strip [%s][%d]\n", cfg->stripOrder.at(i).toLatin1().data(), cfg->stripVisibility.at(i));
      for (MusECore::ciTrack tli = tl->cbegin(); tli != tl->cend(); ++tli) {
        if ((*tli)->name() == cfg->stripOrder.at(i)) {
          MusEGlobal::StripConfig sc;
          sc._visible = cfg->stripVisibility.at(i);
          addStrip(*tli, sc);
          break;
        }
      }
    }
  }
  // NOTE: stripConfigList is the replacement for obsolete stripOrder string list.
  else if (!cfg->stripConfigList.empty()) {
    const int sz = cfg->stripConfigList.size();
    for (int i=0; i < sz; i++) {
      const MusEGlobal::StripConfig& sc = cfg->stripConfigList.at(i);
      DEBUG_MIXER(stderr, "processing strip #:%d [%d][%d]\n", i, sc._serial, sc._visible);
      if(!sc._deleted && !sc.isNull()) {
        const MusECore::Track* t = tl->findSerial(sc._serial);
        if(t)
          addStrip(t, sc);
        }
    }
  }
  else {
    for (MusECore::ciTrack tli = tl->cbegin(); tli != tl->cend(); ++tli) {
      addStrip(*tli);
    }
  }
}

//---------------------------------------------------------
//   configChanged
//---------------------------------------------------------

void AudioMixerApp::configChanged()    
{ 
  DEBUG_MIXER(stderr, "configChanged\n");
  StripList::iterator si = stripList.begin();  // Catch when fonts change, viewable tracks, etc. No full rebuild.
  for (; si != stripList.end(); ++si) 
        (*si)->configChanged();
  
  // Detect when knobs are preferred.
  // TODO: Later if added: Detect when a rack component is added or removed. 
  //       Or other stuff requiring this retabbing.
  if(_preferKnobs != MusEGlobal::config.preferKnobsVsSliders)
  {
    _preferKnobs = MusEGlobal::config.preferKnobsVsSliders;
    // Now set up all tabbing on all strips.
    setupComponentTabbing();
  }
}

//---------------------------------------------------------
//   updateStripList
//---------------------------------------------------------

bool AudioMixerApp::updateStripList()
{
  DEBUG_MIXER(stderr, "updateStripList stripList %d tracks %zd\n", stripList.size(), MusEGlobal::song->tracks()->size());
  
  if (stripList.empty() &&
      // Both obsolete. Support old files.
      (!cfg->stripOrder.empty() ||
      !cfg->stripConfigList.empty()))
      {
        initMixer();
        return true;
      }

  bool changed = false;

  const MusECore::TrackList *tl = MusEGlobal::song->tracks();
  // check for superfluous strips
  for (StripList::iterator si = stripList.begin(); si != stripList.end(); ) {
    if (!tl->contains((*si)->getTrack())) {
      DEBUG_MIXER(stderr, "Did not find track for strip %s - Removing\n", (*si)->getLabelText().toLatin1().data());
      //(*si)->deleteLater();
      delete (*si);
      si = stripList.erase(si);
      changed = true;
    }
    else
      ++si;
  }

  // Mark deleted tracks' strip configs as deleted.
  // Do NOT remove them. They must hang around so undeleted tracks can find them again (below).
  const int config_sz = cfg->stripConfigList.size();
  for (int i = 0; i < config_sz; ++i )
  {
    MusEGlobal::StripConfig& sc = cfg->stripConfigList[i];
    if(!sc._deleted && tl->indexOfSerial(sc._serial) < 0)
      sc._deleted = true;
  }

  // check for new tracks
  for (MusECore::ciTrack tli = tl->cbegin(); tli != tl->end();++tli) {
    const MusECore::Track* track = *tli;
    const int sn = track->serial();
    StripList::const_iterator si = stripList.cbegin();
    for (; si != stripList.cend(); ++si) {
      if ((*si)->getTrack() == track) {
        break;
      }
    }
    if (si == stripList.cend()) {
      DEBUG_MIXER(stderr, "Did not find strip for track %s - Adding\n", track->name().toLatin1().data());
      // Check if there's an existing strip config for this track.
      int idx = 0;
      int i = 0;
      for(; i < config_sz; ++i)
      {
        MusEGlobal::StripConfig& sc = cfg->stripConfigList[i];
        if(!sc.isNull() && sc._serial == sn)
        {
          // Be sure to mark the strip config as undeleted (in use).
          sc._deleted = false;
          // Insert a new strip at the index, with the given config.
          addStrip(track, sc, idx);
          changed = true;
          break;
        }
        if(!sc._deleted)
          ++idx;
      }
      // No config found? Append a new strip with a default config.
      if(i == config_sz)
      {
        addStrip(track);
        changed = true;
      }
    }
  }
  return changed;
}

void AudioMixerApp::updateSelectedStrips()
{
  foreach(Strip *s, stripList)
  {
    if(MusECore::Track* t = s->getTrack())
    {
      if(s->isSelected() != t->selected())
        s->setSelected(t->selected());
    }
  }
}

void AudioMixerApp::moveConfig(const Strip* s, int new_pos)
{
  if(cfg->stripConfigList.empty())
    return;
  const MusECore::Track* track = s->getTrack();
  if(!track)
    return;
  const int sn = track->serial();

  // The config list may also contain configs marked as 'deleted'.
  // Get the 'real' new_pos index, skipping over them.
  const int config_sz = cfg->stripConfigList.size();
  int new_pos_idx = -1;
  int s_idx = -1;
  int j = 0;
  for (int i = 0; i < config_sz; ++i)
  {
    const MusEGlobal::StripConfig& sc = cfg->stripConfigList.at(i);
    // Only 'count' configs that are not deleted.
    if(!sc._deleted)
    {
      if(new_pos_idx == -1 && j == new_pos)
        new_pos_idx = i;
      ++j;
    }
    // While we're at it, remember the index of the given strip.
    if(s_idx == -1 && sc._serial == sn)
      s_idx = i;
    // If we have both pieces of information we're done.
    if(new_pos_idx != -1 && s_idx != -1)
      break;
  }

  // Nothing to do?
  if(new_pos_idx == -1 || s_idx == -1 || new_pos_idx == s_idx)
    return;
  
  // To avoid keeping a copy of the item if doing REMOVE then INSERT, do INSERT then REMOVE.
  // QList::swap() is available but is too new at 5.13.

  // Pre-increment the new pos index if removing an item would affect it.
  if(new_pos_idx > s_idx)
    ++new_pos_idx;

  cfg->stripConfigList.insert(new_pos_idx, cfg->stripConfigList.at(s_idx));

  // Pre-increment the original pos index if removing an item would affect it.
  if(s_idx > new_pos_idx)
    ++s_idx;

  cfg->stripConfigList.removeAt(s_idx);
}

QWidget* AudioMixerApp::setupComponentTabbing(QWidget* previousWidget)
{
  QWidget* prev = previousWidget;
  QLayoutItem* li;
  QWidget* widget;
  Strip* strip;
  const int cnt = mixerLayout->count();
  for(int i = 0; i < cnt; ++i)
  {
    li = mixerLayout->itemAt(i);
    if(!li)
      continue;
    widget = li->widget();
    if(!widget)
      continue;
    strip = qobject_cast<Strip*>(widget);
    if(!strip)
      continue;
    prev = strip->setupComponentTabbing(prev);
  }
  return prev;
}

//---------------------------------------------------------
//   songChanged
//---------------------------------------------------------

void AudioMixerApp::songChanged(MusECore::SongChangedStruct_t flags)
{
  bool changed = false;

  if (flags & (SC_TRACK_INSERTED | SC_TRACK_REMOVED))
  {
    if(updateStripList())
      changed = true;
  }

  // Moving a track is not detected by updateStripList(). Do it here :-)
  if (flags & SC_TRACK_MOVED)
    changed = true;
  
  // Redrawing is costly to do every time. If no strips were added, removed, or moved,
  //  then there should be no reason to redraw the mixer.
  if(changed)
  {
    if (flags &
      // The only flags which would require a redraw, which is very costly, are the following:
      (SC_TRACK_REMOVED | SC_TRACK_INSERTED | SC_TRACK_MOVED
      //| SC_CHANNELS   // Channels due to one/two meters and peak labels can change the strip width.
      //| SC_AUX
      ))
    redrawMixer();
  }

  StripList::iterator si = stripList.begin();
  for (; si != stripList.end(); ++si) {
        (*si)->songChanged(flags);
        }

  if(flags & SC_TRACK_SELECTION)
    updateSelectedStrips();
}

//---------------------------------------------------------
//   closeEvent
//---------------------------------------------------------

void AudioMixerApp::closeEvent(QCloseEvent* e)
      {
      emit closed();
      e->accept();
      }

//---------------------------------------------------------
//   toggleRouteDialog
//---------------------------------------------------------

void AudioMixerApp::toggleRouteDialog()
      {
      showRouteDialog(routingId->isChecked());
      }

//---------------------------------------------------------
//   showRouteDialog
//---------------------------------------------------------

void AudioMixerApp::showRouteDialog(bool on)
      {
      if (on && routingDialog == 0) {
            routingDialog = new MusEGui::RouteDialog(this);
            connect(routingDialog, &RouteDialog::closed, [this]() { routingDialogClosed(); } );
            }
      if (routingDialog)
            routingDialog->setVisible(on);
      //menuView->setItemChecked(routingId, on);
      routingId->setChecked(on);
      }

//---------------------------------------------------------
//   routingDialogClosed
//---------------------------------------------------------

void AudioMixerApp::routingDialogClosed()
{
      routingId->setChecked(false);
}

//---------------------------------------------------------
//   show hide track groups
//---------------------------------------------------------

void AudioMixerApp::showMidiTracksChanged(bool v)
{
    cfg->showMidiTracks = v;
    redrawMixer();
}
//void AudioMixerApp::showDrumTracksChanged(bool v)
//{
//    cfg->showDrumTracks = v;
//    redrawMixer();
//}
void AudioMixerApp::showNewDrumTracksChanged(bool v)
{
    cfg->showNewDrumTracks = v;
    redrawMixer();
}
void AudioMixerApp::showWaveTracksChanged(bool v)
{
    cfg->showWaveTracks = v;
    redrawMixer();
}
void AudioMixerApp::showInputTracksChanged(bool v)
{
    cfg->showInputTracks = v;
    redrawMixer();
}
void AudioMixerApp::showOutputTracksChanged(bool v)
{
    cfg->showOutputTracks = v;
    redrawMixer();
}
void AudioMixerApp::showGroupTracksChanged(bool v)
{
    cfg->showGroupTracks = v;
    redrawMixer();
}
void AudioMixerApp::showAuxTracksChanged(bool v)
{
    cfg->showAuxTracks = v;
    redrawMixer();
}
void AudioMixerApp::showSyntiTracksChanged(bool v)
{
    cfg->showSyntiTracks = v;
    redrawMixer();
}

void AudioMixerApp::keyPressEvent(QKeyEvent *ev)
{
  bool moveEnabled=false;
  const bool shift = ev->modifiers() & Qt::ShiftModifier;
  const bool alt = ev->modifiers() & Qt::AltModifier;
  const bool ctl = ev->modifiers() & Qt::ControlModifier;
  if (ctl && alt) {
    moveEnabled=true;
  }

  switch (ev->key()) {
    case Qt::Key_Left:
      if (moveEnabled)
      {
        selectNextStrip(false, !shift);
        ev->accept();
        return;
      }
      break;

    case Qt::Key_Right:
      if (moveEnabled)
      {
        selectNextStrip(true, !shift);
        ev->accept();
        return;
      }
      break;

    default:
      break;
  }

  ev->ignore();
  return QMainWindow::keyPressEvent(ev);
}

void AudioMixerApp::resizeEvent(QResizeEvent* e)
{
  e->ignore();
  QMainWindow::resizeEvent(e);
  // Make sure to update the config size.
  // Hack flag to prevent overwriting the config geometry when resizing.
  // Do NOT overwrite if we set the flag before calling.
  if(!_resizeFlag)
    cfg->geometry.setSize(e->size());
}

void AudioMixerApp::moveEvent(QMoveEvent* e)
{
  e->ignore();
  QMainWindow::moveEvent(e);
  // Make sure to update the config geometry position.
  cfg->geometry.setTopLeft(e->pos());
}

void AudioMixerApp::clearStripSelection()
{
  foreach (Strip *s, stripList)
    s->setSelected(false);
}

void AudioMixerApp::selectNextStrip(bool isRight, bool /*clearAll*/)
{
  Strip *prev = NULL;

  for (int i = 0; i < mixerLayout->count(); i++)
  {
    QWidget *w = mixerLayout->itemAt(i)->widget();
    if (w)
    {
      if (prev && !prev->isEmbedded() && prev->isSelected() && isRight) // got it
      {
        Strip* st = static_cast<Strip*>(w);
        //if(clearAll)  // TODO
        {
          MusEGlobal::song->selectAllTracks(false);
          clearStripSelection();
        }
        st->setSelected(true);
        if(st->getTrack())
          st->getTrack()->setSelected(true);
        MusEGlobal::song->update(SC_TRACK_SELECTION);
        return;
      }
      else if( !static_cast<Strip*>(w)->isEmbedded() && static_cast<Strip*>(w)->isSelected() && prev && !prev->isEmbedded() && !isRight)
      {
        //if(clearAll) // TODO
        {
          MusEGlobal::song->selectAllTracks(false);
          clearStripSelection();
        }
        prev->setSelected(true);
        if(prev->getTrack())
          prev->getTrack()->setSelected(true);
        MusEGlobal::song->update(SC_TRACK_SELECTION);
        return;
      }
      else {
        prev = static_cast<Strip*>(w);
      }
    }
  }

  QWidget *w;
  if (isRight)
    w = mixerLayout->itemAt(0)->widget();
  else
    w = mixerLayout->itemAt(mixerLayout->count()-1)->widget();
  Strip* st = static_cast<Strip*>(w);
  if(st && !st->isEmbedded())
  {
    //if(clearAll) // TODO
    {
      MusEGlobal::song->selectAllTracks(false);
      clearStripSelection();
    }
    st->setSelected(true);
    if(st->getTrack())
      st->getTrack()->setSelected(true);
    MusEGlobal::song->update(SC_TRACK_SELECTION);
  }
}

bool AudioMixerApp::eventFilter(QObject *obj,
                             QEvent *event)
{
  DEBUG_MIXER(stderr, "eventFilter type %d\n", (int)event->type());
    QKeyEvent *keyEvent = NULL;//event data, if this is a keystroke event
    bool result = false;//return true to consume the keystroke

    if (event->type() == QEvent::KeyPress)
    {
         keyEvent = dynamic_cast<QKeyEvent*>(event);
         this->keyPressEvent(keyEvent);
         result = true;
    }//if type()

    else if (event->type() == QEvent::KeyRelease)
    {
        keyEvent = dynamic_cast<QKeyEvent*>(event);
        this->keyReleaseEvent(keyEvent);
        result = true;
    }//else if type()

    //### Standard event processing ###
    else
        result = QObject::eventFilter(obj, event);

    return result;
}//eventFilter


} // namespace MusEGui
